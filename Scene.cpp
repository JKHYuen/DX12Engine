#include "Scene.h"
#include "DirectionalLight.h"
#include "Skybox.h"
#include "CommandList.h"
#include "Device.h"

Scene::Scene(Device& device, CommandList& copyCommandList, const DirectionalLight::DirectionalLightParams& dirLightParams, const Skybox::SkyboxParams& skyboxParams)
	: m_DirectionalLight(device, dirLightParams)
	, m_Skybox(device, copyCommandList, skyboxParams)
{
	// arbitrary default camera position
	XMVECTOR cameraPos    = XMVectorSet(0, 5, -20, 1);
	XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
	XMVECTOR cameraUp     = XMVectorSet(0, 1, 0, 0);

	m_MainCamera.Set_LookAt(cameraPos, cameraTarget, cameraUp);
}

// Note: vector reallocation may be slow if memory not reserved, not too worried about it for now
void Scene::CreateGameObject(CommandList& copyCommandList, const GameObject::GameObjectParams& goParams, std::shared_ptr<Mesh> mesh) {
	m_SceneObjects.emplace_back(copyCommandList, goParams, mesh);
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
