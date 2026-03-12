#include "Scene.h"
#include "DirectionalLight.h"
#include "Skybox.h"
#include "CommandList.h"
#include "CommandQueue.h"
#include "ImGui.h"
#include "Device.h"
#include "Picker.h"
#include "Events.h"
#include "Logger.h"

#include <filesystem>

Scene::Scene(Device& device, CommandList& copyCommandList, const DirectionalLight::DirectionalLightParams& dirLightParams, const Skybox::SkyboxParams& skyboxParams, int windowWidth, int windowHeight)
	: m_DirectionalLight(device, dirLightParams)
	, m_Skybox(device, copyCommandList, skyboxParams)
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

	m_Picker = std::make_unique<Picker>(this);

	// Preload material asset folders, this will need to be extracted to a function for hotloading
	// Note: wide strings from file system is supported, but it can not be properly displayed with ImGui
	for(const auto& entry : std::filesystem::directory_iterator(L"assets/materials")) {
		m_MaterialNames.emplace_back(entry.path().filename().c_str());
	}
}

/// TODO: figure out some proper game object storage, returned pointer here will be invalidated if vector resizes
GameObject* Scene::CreateGameObject(CommandList& copyCommandList, const GameObject::GameObjectParams& goParams, std::shared_ptr<Mesh> mesh) {
	assert(m_SceneObjects.size() < sk_MaxSceneObjects);
	m_SceneObjects.emplace_back(copyCommandList, goParams, mesh);
	return &m_SceneObjects.back();
}

void Scene::CreateGameObject(CommandList& copyCommandList, const GameObject::GameObjectParams& goParams, const std::wstring& meshFileName) {
	/// TODO
}

void Scene::ComputeSkyboxIBLMaps(CommandList& directCommandList) {
	m_Skybox.ComputeIBLMaps(directCommandList);
}

void Scene::Render(const RenderTarget& targetRT, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRec, CommandList& directCommandList, const UpdateEventArgs& e) {
	directCommandList.SetScissorRect(scissorRec);
	directCommandList.SetViewport(viewPort);
	directCommandList.SetRenderTarget(targetRT);

	m_Skybox.Render(directCommandList, m_MainCamera);

	// Render scene objects
	// All game objects use the same PSO/root sig right now
	for(auto& o : m_SceneObjects) {
		// Render pipeline currently set (inside function below) for every object even though they are the same
		o.Render(directCommandList, e, *this);
	}

	// Render depth from directional light for same objects above
	m_DirectionalLight.SetShadowDepthPipelineStateAndRenderTarget(directCommandList);
	for(auto& o : m_SceneObjects) {
		o.RenderToDirectionalShadowMap(directCommandList, *this);
	}

}

void Scene::SetDirectionalLightAngle(float rotX, float rotY, float rotZ) {
	m_DirectionalLight.SetDirection(rotX, rotY, rotZ);
}

void Scene::OnMouseButtonReleased(const MouseButtonEventArgs& e) {
	/// TODO: disable picking when dragging object inspector sliders
	// Gameobject raycast picking for object inspector GUI, gameobjects are outlined if picked
	{
		if(GameObject* go = m_Picker->GetPickedObject()) {
			go->SetOutlineState(false);
		}
		if(GameObject* go = m_Picker->MouseRaycast(e.X, e.Y, m_GameWindowWidth, m_GameWindowHeight)) {
			go->SetOutlineState(true);
		}
	}

}

void Scene::RenderImGui() {
	GameObject* picked = m_Picker->GetPickedObject();
	if(picked == nullptr) return;

	static std::wstring_view s_SelectedMat {};
	static float s_ObjTranslation[3];
	static float s_ObjEulerAngles[3];
	static float s_ObjScale[3];
	static GameObject* s_LastPickedObject {};
	
	// Newly picked object, update sliders
	if(picked != s_LastPickedObject) {
		s_SelectedMat = picked->GetMaterialName();
		/// TODO: transform values
	}
	s_LastPickedObject = picked;

	static const ImGuiSliderFlags kSliderFlags = ImGuiSliderFlags_AlwaysClamp;

	ImGui::Begin("Object Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
	{
		if(ImGui::BeginTable("PBR Material Table", 4, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders)) {
			for(auto& s : m_MaterialNames) {
				ImGui::TableNextColumn();

				static const size_t kBufferSize = 100;
				// convert wide string to const char* since ImGui can't render wide chars
				// using https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-s-wcstombs-s-l?view=msvc-170
				char labelBuf[kBufferSize];
				size_t bytesConverted;
				wcstombs_s(&bytesConverted, labelBuf, kBufferSize, s.c_str(), kBufferSize - 1);

				if(ImGui::Selectable(labelBuf, s == s_SelectedMat)) {
					auto& copyCommandQueue = m_Device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
					auto copyCommandList = copyCommandQueue.GetCommandList();

					for(auto& go : m_SceneObjects) {
						go.UpdateShaderResources(*copyCommandList, s);
					}

					copyCommandQueue.ExecuteCommandList(copyCommandList);
					copyCommandQueue.FlushWait();

					s_SelectedMat = s;
				};
			}
			ImGui::EndTable();
		}

		if(ImGui::DragFloat3("Position", s_ObjTranslation, 0.01f, -1000.0f, 1000.0f, "%.2f", kSliderFlags)) {
			picked->SetTranslation(s_ObjTranslation[0], s_ObjTranslation[1], s_ObjTranslation[2]);
		}

		if(ImGui::DragFloat3("Rotation", s_ObjEulerAngles, 0.01f, -1000.0f, 1000.0f, "%.2f", kSliderFlags)) {
			picked->SetEulerRotation(s_ObjEulerAngles[0], s_ObjEulerAngles[1], s_ObjEulerAngles[2]);
		}

		if(ImGui::DragFloat3("Scale", s_ObjScale, 0.01f, -1000.0f, 1000.0f, "%.2f", kSliderFlags)) {
			picked->SetScale(s_ObjScale[0], s_ObjScale[1], s_ObjScale[2]);
		}

		/// Object Inspector Window End
		ImGui::End();
	}

}
