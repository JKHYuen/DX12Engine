#include "Scene.h"

#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/Device.h"
#include "DX12EngineCore/IGame.h"
#include "DX12EngineCore/RenderTarget.h"

#include "AssetImporter.h"
#include "Camera.h"
#include "Colors.h"
#include "d3d12.h"
#include "DirectionalLight.h"
#include "EditorGui.h"
#include "Events.h"
#include "GameObject.h"
#include "ImGui.h"
#include "KeyCodes.h"
#include "PBRGameObject.h"
#include "PointLight.h"
#include "RenderConstants.h"
#include "Skybox.h"
#include "UnlitPrimitivePSO.h"
#include "UnlitPSO.h"

#include <cstdint>
#include <DirectXMath.h>
#include <DirectXMathVector.inl>
#include <filesystem>
#include <memory>
#include <string>

using namespace RenderGlobals;

Scene::Scene(Device& device, CommandList& copyCommandList, CommandList& computeCommandList, const DirectionalLight::DirectionalLightParams& dirLightParams, const Skybox::SkyboxParams& skyboxParams, UnlitPSO* unlitPSO, const IGame& game)
	: m_Device(device)
	, m_UnlitPSO(unlitPSO)
	, m_AABBRenderMode(AABBRenderMode::None)
{
	m_DirectionalLight = std::make_unique<DirectionalLight>(device, dirLightParams);
	m_Skybox = std::make_unique<Skybox>(device, copyCommandList, computeCommandList, skyboxParams);

	m_SceneObjects.reserve(sk_MaxSceneObjects);

	// Get all material names from asset folder (mostly for testing, scene instance should only hold used materials)
	// Note: wide strings from file system is supported, but it can not be properly displayed with ImGui
	for(const auto& entry : std::filesystem::directory_iterator(AssetImporter::Get().GetAssetPath() / L"materials")) {
		m_MaterialNames.emplace_back(entry.path().filename().c_str());
	}

	/// Default test point light properties
	m_PointLights.reserve(gk_MaxPointLightCount);
	PointLight::PointLightParams pointLightParams {};
	pointLightParams.translation        = XMFLOAT3 { 0.0f, 5.0f, 7.0f };
	pointLightParams.color              = XMFLOAT3 { 100.0f, 0.0f, 0.0f };
	pointLightParams.radius             = 20.0f;
	pointLightParams.visualizationMesh  = copyCommandList.GetSpherePrimitive();
	pointLightParams.unlitPSO           = unlitPSO;
	pointLightParams.depthPSO           = dirLightParams.depthPSO;
	m_PointLights.push_back(std::make_unique<PointLight>(device, "PointLight0", pointLightParams));

	pointLightParams.translation = XMFLOAT3 { 5.0f, 5.0f, -5.0f };
	pointLightParams.color       = XMFLOAT3 { 0.0f, 100.0f, 0.0f };
	m_PointLights.push_back(std::make_unique<PointLight>(device, "PointLight1", pointLightParams));

	pointLightParams.translation = XMFLOAT3 { -5.0f, 5.0f, -5.0f };
	pointLightParams.color       = XMFLOAT3 { 0.0f, 0.0f, 100.0f };
	m_PointLights.push_back(std::make_unique<PointLight>(device, "PointLight2", pointLightParams));
	///
}

void Scene::ComputeSkyboxIBLs(CommandList& directCommandList) {
	m_Skybox->ComputeIBLMaps(directCommandList);
}

void Scene::SetSkybox(CommandList& copyCommandList, CommandList& computeCommandList, const std::wstring& hdrTextureName) {
	m_Skybox->SetCubemap(m_Device, copyCommandList, computeCommandList, hdrTextureName);
}

void Scene::Render(const RenderTarget& outputRT, CommandList& directCommandList, const UpdateEventArgs& e, Camera& camera, uint32_t windowWidth, uint32_t windowHeight, bool b_EnableFrustumCulling) {
	camera.UpdateFrustum();

	// Render depth from directional light
	m_DirectionalLight->SetShadowDepthPipelineStateAndRenderTarget(directCommandList);
	for(auto& o : m_SceneObjects) {
		if(PBRGameObject* pbrO = dynamic_cast<PBRGameObject*>(o.get())) {
			pbrO->RenderToDirectionalShadowMap(directCommandList, *m_DirectionalLight);
		}
	}
	
	// Render depth from each point light (cubemap)
	m_PointLights[0]->SetShadowDepthPipelineState(directCommandList);
	for(int i = 0; i < RenderGlobals::gk_MaxPointLightCount; i++) {
		m_PointLights[i]->ClearShadowCubemap(directCommandList);
		for(auto& o : m_SceneObjects) {
			if(PBRGameObject* pbrO = dynamic_cast<PBRGameObject*>(o.get())) {
				pbrO->RenderToPointLightShadowMap(directCommandList, *m_PointLights[i]);
			}
		}
	}

	// Render skybox and objects with same render target
	directCommandList.ClearTexture(outputRT.GetTexture(AttachmentPoint::Color0), Colors::DefaultBackground);
	directCommandList.ClearDepthStencilTexture(outputRT.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
	directCommandList.SetViewport(outputRT.GetViewport());
	directCommandList.SetRenderTarget(outputRT);

	m_Skybox->Render(directCommandList, camera);

	// Render point light visualization meshes (debug)
	for(uint32_t i = 0; i < gk_MaxPointLightCount; i++) {
		m_PointLights[i]->RenderVisualizationMesh(directCommandList, e, camera);
	}

	// Render scene objects
	// All game objects use the same PSO/root sig right now
	for(auto& o : m_SceneObjects) {
		if(PBRGameObject* pbrO = dynamic_cast<PBRGameObject*>(o.get())) {
			pbrO->Render(directCommandList, e, *this, camera, windowWidth, windowHeight, b_EnableFrustumCulling, mb_WireframeRender);
		}
	}
}

void Scene::RenderBoundingBoxes(const RenderTarget& outputRT, CommandList& directCommandList, const UpdateEventArgs& e, const Camera& camera, UnlitPrimitivePSO* unlitPrimitivePSO, GameObject* pickedObject) {
	if(m_AABBRenderMode == AABBRenderMode::None) return;

	unlitPrimitivePSO->SetWireframePipelineState(directCommandList);
	
	directCommandList.SetRenderTarget(outputRT);
	directCommandList.SetViewport(outputRT.GetViewport());
	
	// Bounding box color is hard coded for now
	constexpr XMFLOAT4 aabbColor = XMFLOAT4(0.1f, 1.0f, 0.1f, 1.0f);

	if(m_AABBRenderMode == AABBRenderMode::PickedOnly) {
		if(GameObject* picked = pickedObject) {
			if(PBRGameObject* pbrPicked = dynamic_cast<PBRGameObject*>(picked)) {
				pbrPicked->RenderBoundingBox(directCommandList, e, camera, aabbColor);
			}
		}
	}
	// Render All AABBs
	else {
		for(auto& o : m_SceneObjects) {
			if(PBRGameObject* pbrO = dynamic_cast<PBRGameObject*>(o.get())) {
				pbrO->RenderBoundingBox(directCommandList, e, camera, aabbColor);
			}
		}
	}

}

void Scene::OnMouseButtonReleased(const MouseButtonEventArgs& e) {}

void Scene::OnKeyPressed(const KeyEventArgs& e) {}

void Scene::OnKeyReleased(const KeyEventArgs& e) {
	switch(e.Key) {
		case KeyCode::F2:
			mb_WireframeRender = !mb_WireframeRender;
			break;
	}
}