#include "Scene.h"

#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/Device.h"
#include "DX12EngineCore/RenderTarget.h"
#include "DX12EngineCore/IGame.h"

#include "DirectionalLight.h"
#include "Skybox.h"
#include "UnlitPSO.h"
#include "UnlitPrimitivePSO.h"
#include "ImGui.h"
#include "Picker.h"
#include "Logger.h"
#include "EditorGui.h"
#include "Colors.h"
#include "AssetImporter.h"

#include <filesystem>

Scene::Scene(Device& device, CommandList& copyCommandList, CommandList& computeCommandList, const DirectionalLight::DirectionalLightParams& dirLightParams, const Skybox::SkyboxParams& skyboxParams, const IGame& game)
	: m_DirectionalLight(device, dirLightParams)
	// Note: camera starting values are unimportant, they are overriden by OnResize() function in DemoGame on app launch.
	//       Starting FOV however is determined here, will clean this up later.
	, m_MainCamera(45.0f, 1.0f, 0.1f, 100.0f)
	, m_Skybox(device, copyCommandList, computeCommandList, skyboxParams)
	, m_Device(device)
	, m_Game(game)
	, m_AABBRenderMode(AABBRenderMode::None)
{
	// arbitrary default camera position
	XMVECTOR cameraPos    = XMVectorSet(0, 5, -20, 1);
	XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
	XMVECTOR cameraUp     = XMVectorSet(0, 1, 0, 0);

	m_MainCamera.Set_LookAt(cameraPos, cameraTarget, cameraUp);

	m_SceneObjects.reserve(sk_MaxSceneObjects);

	m_Picker = std::make_unique<Picker>();

	// Get all material names from asset folder (mostly for testing, scene instance should only hold used materials)
	// Note: wide strings from file system is supported, but it can not be properly displayed with ImGui
	for(const auto& entry : std::filesystem::directory_iterator(AssetImporter::Get().GetAssetPath() / L"materials")) {
		m_MaterialNames.emplace_back(entry.path().filename().c_str());
	}
}

void Scene::ComputeSkyboxIBLs(CommandList& directCommandList) {
	m_Skybox.ComputeIBLMaps(directCommandList);
}

/// TODO: TEMP
void Scene::SetSkybox(CommandList& copyCommandList, CommandList& computeCommandList, const Skybox::SkyboxParams& skyboxParams) {
	m_Skybox = Skybox(m_Device, copyCommandList, computeCommandList, skyboxParams);
	//m_Skybox.SetCubemap(copyCommandList, computeCommandList, skyboxParams.hdrTextureName);

	mb_ChangeSkybox = true;
}
/// END TEMP

void Scene::Render(const RenderTarget& outputRT, CommandList& directCommandList, const UpdateEventArgs& e) {
	m_MainCamera.UpdateFrustum();

	// Render depth from directional light
	m_DirectionalLight.SetShadowDepthPipelineStateAndRenderTarget(directCommandList);
	for(auto& o : m_SceneObjects) {
		o.RenderToDirectionalShadowMap(directCommandList, m_DirectionalLight);
	}

	// Render skybox and objects with same render target
	directCommandList.ClearTexture(outputRT.GetTexture(AttachmentPoint::Color0), Colors::DefaultBackground);
	directCommandList.ClearDepthStencilTexture(outputRT.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
	directCommandList.SetViewport(outputRT.GetViewport());
	directCommandList.SetRenderTarget(outputRT);

	m_Skybox.Render(directCommandList, m_MainCamera);

	// Render scene objects
	// All game objects use the same PSO/root sig right now
	for(auto& o : m_SceneObjects) {
		/// TODO: TEMP
		if(mb_ChangeSkybox) {
			o.UpdateIBLShaderResources(*this);
		}

		o.Render(directCommandList, e, *this, mb_WireframeRender);
	}

	/// TODO: TEMP
	mb_ChangeSkybox = false;
}

void Scene::RenderBoundingBoxes(const RenderTarget& outputRT, CommandList& directCommandList, const UpdateEventArgs& e, UnlitPrimitivePSO* unlitPrimitivePSO) {
	if(m_AABBRenderMode == AABBRenderMode::None) return;

	unlitPrimitivePSO->SetWireframePipelineState(directCommandList);
	
	directCommandList.SetViewport(outputRT.GetViewport());
	directCommandList.SetRenderTarget(outputRT);
	
	// Bounding box color is hard coded for now
	constexpr XMFLOAT4 aabbColor = XMFLOAT4(0.1f, 1.0f, 0.1f, 1.0f);

	if(m_AABBRenderMode == AABBRenderMode::PickedOnly) {
		if(GameObject* picked = m_Picker->GetPickedObject())
			picked->RenderBoundingBox(directCommandList, e, unlitPrimitivePSO, *this, aabbColor);
	}
	// Render All AABBs
	else {
		for(auto& o : m_SceneObjects) {
			o.RenderBoundingBox(directCommandList, e, unlitPrimitivePSO, *this, aabbColor);
		}
	}

}

uint32_t Scene::GetWindowWidth() const { return m_Game.GetWindowWidth(); }
uint32_t Scene::GetWindowHeight() const { return m_Game.GetWindowHeight(); }

void Scene::OnMouseButtonReleased(const MouseButtonEventArgs& e) {
	// Gameobject raycast picking for object inspector GUI, gameobjects are outlined if picked
	// Disabled if:
	//   - ImGui mouse event is occuring i.e. mouse is hovered on top of a ImGui element
	//	 - or ImGui UI is not open
	if(e.Button == MouseButtonEventArgs::Left) {
		if(EditorGui::Get().GetUIVisibilityState() && !ImGui::GetIO().WantCaptureMouse) {
			// Update picked object
			m_Picker->MouseRaycast(*this, e.X, e.Y, m_Game.GetWindowWidth(), m_Game.GetWindowHeight());
		}

		// Unpick object if mouse clicked and editor UI is not open
		if(!EditorGui::Get().GetUIVisibilityState()) {
			if(const GameObject* go = m_Picker->GetPickedObject()) {
				m_Picker->ClearPickedObject();
			}
		}
	}
}

void Scene::OnKeyPressed(const KeyEventArgs& e) {}

void Scene::OnKeyReleased(const KeyEventArgs& e) {
	switch(e.Key) {
		case KeyCode::X: 
			if(const GameObject* go = m_Picker->GetPickedObject()) {
				m_Picker->ClearPickedObject();
			}
		break;

		case KeyCode::F4:
			mb_WireframeRender = !mb_WireframeRender;
			break;
	}
}