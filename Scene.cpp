#include "Scene.h"
#include "DirectionalLight.h"
#include "Skybox.h"
#include "CommandList.h"
#include "CommandQueue.h"
#include "ImGui.h"
#include "Device.h"
#include "Picker.h"
#include "Logger.h"
#include "Helpers.h"
#include "StringHelpers.h"
#include "EditorGui.h"
#include "Colors.h"

#include <filesystem>

Scene::Scene(Device& device, CommandList& copyCommandList, CommandList& computeCommandList, const DirectionalLight::DirectionalLightParams& dirLightParams, const Skybox::SkyboxParams& skyboxParams, int windowWidth, int windowHeight)
	: m_DirectionalLight(device, dirLightParams)
	, m_Skybox(device, copyCommandList, computeCommandList, skyboxParams)
	, m_Device(device)
	, m_GameWindowWidth(windowWidth)
	, m_GameWindowHeight(windowHeight)
{
	// arbitrary default camera position
	XMVECTOR cameraPos    = XMVectorSet(0, 5, -20, 1);
	XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
	XMVECTOR cameraUp     = XMVectorSet(0, 1, 0, 0);

	m_MainCamera.Set_LookAt(cameraPos, cameraTarget, cameraUp);

	m_SceneObjects.reserve(sk_MaxSceneObjects);

	m_Picker = std::make_unique<Picker>();

	/// TODO: dont hardcode asset path
	// Get all material names from asset folder (mostly for testing, scene instance should only hold used materials)
	// Note: wide strings from file system is supported, but it can not be properly displayed with ImGui
	for(const auto& entry : std::filesystem::directory_iterator(L"assets/materials")) {
		m_MaterialNames.emplace_back(entry.path().filename().c_str());
	}
}

void Scene::ComputeSkyboxIBLs(CommandList& directCommandList) {
	m_Skybox.ComputeIBLMaps(directCommandList);
}

/// TODO: TEMP
/// UNUSED
void Scene::SetCubemap(CommandList& copyCommandList, const std::wstring& hdrTextureName) {
	m_Skybox.SetCubemap(copyCommandList, hdrTextureName);
}

void Scene::SetSkybox(CommandList& copyCommandList, CommandList& computeCommandList, const Skybox::SkyboxParams& skyboxParams) {
	m_Skybox = Skybox(m_Device, copyCommandList, computeCommandList, skyboxParams);
	s_ChangeSkybox = true;
}
/// END TEMP

void Scene::Render(const RenderTarget& targetRT, D3D12_VIEWPORT viewPort, CommandList& directCommandList, const UpdateEventArgs& e) {
	// Render depth from directional light
	m_DirectionalLight.SetShadowDepthPipelineStateAndRenderTarget(directCommandList);
	for(auto& o : m_SceneObjects) {
		o.RenderToDirectionalShadowMap(directCommandList, m_DirectionalLight);
	}

	// Render skybox and objects with same render target
	directCommandList.ClearTexture(targetRT.GetTexture(AttachmentPoint::Color0), Colors::DefaultBackground);
	directCommandList.ClearDepthStencilTexture(targetRT.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
	directCommandList.SetViewport(viewPort);
	directCommandList.SetRenderTarget(targetRT);
	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_Skybox.Render(directCommandList, m_MainCamera);

	// Render scene objects
	// All game objects use the same PSO/root sig right now
	for(auto& o : m_SceneObjects) {
		/// TODO: TEMP
		if(s_ChangeSkybox) {
			o.UpdateIBLShaderResources(*this);
		}

		// Render pipeline currently set (inside function below) for every object even though they are the same
		o.Render(directCommandList, e, *this);
	}

	/// TODO: TEMP
	s_ChangeSkybox = false;
}

void Scene::OnMouseButtonReleased(const MouseButtonEventArgs& e) {
	// Gameobject raycast picking for object inspector GUI, gameobjects are outlined if picked
	// Disabled if:
	//   - ImGui mouse event is occuring i.e. mouse is hovered on top of a ImGui element
	//	 - or ImGui UI is not open
	if(e.Button == MouseButtonEventArgs::Left) {
		if(EditorGui::Get().GetUIVisibilityState() && !ImGui::GetIO().WantCaptureMouse) {
			// Update picked object
			m_Picker->MouseRaycast(*this, e.X, e.Y, m_GameWindowWidth, m_GameWindowHeight);
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
	if(e.Key == KeyCode::X) {
		if(const GameObject* go = m_Picker->GetPickedObject()) {
			m_Picker->ClearPickedObject();
		}
	}
}