#include "Scene.h"
#include "DirectionalLight.h"
#include "Skybox.h"
#include "CommandList.h"
#include "CommandQueue.h"
#include "ImGui.h"
#include "Device.h"

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
}

/// TODO: use UUIDs for gameobject storage, returned pointer here is for testing only and will be invalidated when vector resizes
// Note: vector reallocation may be slow if memory not reserved, not too worried about it for now
GameObject* Scene::CreateGameObject(CommandList& copyCommandList, const GameObject::GameObjectParams& goParams, std::shared_ptr<Mesh> mesh) {
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
		o.Render(directCommandList, e, *this);
	}

	// Render depth from directional light for same objects above
	m_DirectionalLight.SetShadowDepthPipelineStateAndRenderTarget(directCommandList);
	for(auto& obj : m_SceneObjects) {
		obj.RenderToDirectionalShadowMap(directCommandList, *this);
	}
}

void Scene::SetDirectionalLightAngle(float rotX, float rotY, float rotZ) {
	m_DirectionalLight.SetDirection(rotX, rotY, rotZ);
}

void Scene::RenderDebugComponents() {
	/// TODO: unfinished material picker
	{
		static const size_t kBufferSize = 100;

		/// TODO: start with correct selected material index
		static int selectedIdx = 0;

		if(ImGui::BeginTable("PBR Material Table", 4, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_Borders)) {
			int idx = 0;
			for(const auto& entry : std::filesystem::directory_iterator(L"assets/materials")) {
				// convert wide string to const char* for ImGui component
				char labelBuf[kBufferSize];
				size_t bytesConverted;
				wcstombs_s(&bytesConverted, labelBuf, kBufferSize, entry.path().filename().c_str(), kBufferSize - 1);

				ImGui::TableNextColumn();
				if(ImGui::Selectable(labelBuf, idx == selectedIdx)) {
					auto& copyCommandQueue = m_Device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
					auto copyCommandList = copyCommandQueue.GetCommandList();

					for(auto& go : m_SceneObjects) {
						go.UpdateShaderResources(*copyCommandList, entry.path().filename());
					}
					copyCommandQueue.ExecuteCommandList(copyCommandList);
					copyCommandQueue.FlushWait();

					selectedIdx = idx;
				};
				idx++;
			}
			ImGui::EndTable();
		}
	}
}
