#pragma once
#include <vector>
#include "Camera.h"
#include "DirectionalLight.h"
#include "Skybox.h"
#include "GameObject.h"

class Device;

class Scene {
public:
	// TODO: Add default values / instances of directional light and skybox if needed
	// Note skybox and directional light
	Scene(Device& device, CommandList& copyCommandList, const DirectionalLight::DirectionalLightParams& dirLightParams, const Skybox::SkyboxParams& skyboxParams);

	Scene(const Scene&) = delete;
	Scene& operator=(Scene&) = delete;
	Scene(Scene&&) = delete;
	Scene& operator=(Scene&&) = delete;

	const DirectionalLight& GetDirectionalLight() const { return m_DirectionalLight; };
	const Skybox& GetSkybox() const { return m_Skybox; };

	void CreateGameObject(CommandList& copyCommandList, const GameObject::GameObjectParams& goParams, std::shared_ptr<Mesh> mesh);
	void CreateGameObject(CommandList& copyCommandList, const GameObject::GameObjectParams& goParams, const std::wstring& meshFileName);

	void ComputeSkyboxIBLMaps(CommandList& directCommandList);

	void Render(const RenderTarget& targetRT, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRec, CommandList& directCommandList, const UpdateEventArgs& e);
	
	void SetDirectionalLightAngle(float rotX, float rotY, float rotZ);

	// keep this public out of convenience for now
	Camera m_MainCamera;

private:
	std::vector<GameObject> m_SceneObjects;

	DirectionalLight m_DirectionalLight;
	Skybox m_Skybox;
};

