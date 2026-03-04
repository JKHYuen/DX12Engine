#include "Scene.h"
#include "DirectionalLight.h"
#include "Skybox.h"
#include "CommandList.h"
#include "CommandQueue.h"
#include "ImGui.h"
#include "Device.h"
#include "Logger.h"

#include <filesystem>

Scene::Scene(Device& device, CommandList& copyCommandList, const DirectionalLight::DirectionalLightParams& dirLightParams, const Skybox::SkyboxParams& skyboxParams)
	: m_DirectionalLight(device, dirLightParams)
	, m_Skybox(device, copyCommandList, skyboxParams)
	, m_Device(device)
{
	// arbitrary default camera position
	XMVECTOR cameraPos    = XMVectorSet(0, 5, -20, 1);
	XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
	XMVECTOR cameraUp     = XMVectorSet(0, 1, 0, 0);

	m_MainCamera.Set_LookAt(cameraPos, cameraTarget, cameraUp);

	m_SceneObjects.reserve(sk_MaxSceneObjects);

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

void Scene::RenderImGui() {
	/// TODO: unfinished material picker (need to implement per object)
	{
		static std::wstring selectedMat = L"";
		if(ImGui::BeginTable("PBR Material Table", 4, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders)) {
			for(auto& s : m_MaterialNames) {
				ImGui::TableNextColumn();

				static const size_t kBufferSize = 100;
				// convert wide string to const char* so ImGui can render text
				// using https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-s-wcstombs-s-l?view=msvc-170
				char labelBuf[kBufferSize];
				size_t bytesConverted;
				wcstombs_s(&bytesConverted, labelBuf, kBufferSize, s.c_str(), kBufferSize - 1);

				if(ImGui::Selectable(labelBuf, s == selectedMat)) {
					auto& copyCommandQueue = m_Device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
					auto copyCommandList = copyCommandQueue.GetCommandList();

					for(auto& go : m_SceneObjects) {
						go.UpdateShaderResources(*copyCommandList, s);
					}

					copyCommandQueue.ExecuteCommandList(copyCommandList);
					copyCommandQueue.FlushWait();

					selectedMat = s;
				};
			}

			///// TODO: start with correct selected material index
			//static int selectedIdx = 0;

			//int idx = 0;
			//for(const auto& entry : std::filesystem::directory_iterator(L"assets/materials")) {
			//	// convert wide string to const char* for ImGui component
			//	// using https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/wcstombs-s-wcstombs-s-l?view=msvc-170
			//	char labelBuf[kBufferSize];
			//	size_t bytesConverted;
			//	wcstombs_s(&bytesConverted, labelBuf, kBufferSize, entry.path().filename().c_str(), kBufferSize - 1);

			//	ImGui::TableNextColumn();
			//	if(ImGui::Selectable(labelBuf, idx == selectedIdx)) {
			//		auto& copyCommandQueue = m_Device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
			//		auto copyCommandList = copyCommandQueue.GetCommandList();

			//		for(auto& go : m_SceneObjects) {
			//			go.UpdateShaderResources(*copyCommandList, entry.path().filename());
			//		}

			//		copyCommandQueue.ExecuteCommandList(copyCommandList);
			//		copyCommandQueue.FlushWait();

			//		selectedIdx = idx;
			//	};
			//	idx++;
			//}
			ImGui::EndTable();
		}
	}
}
