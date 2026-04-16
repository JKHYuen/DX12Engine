#pragma once
#include "Camera.h"
#include "DirectionalLight.h"
#include "Skybox.h"
#include "GameObject.h"
#include "DataArray.h"
#include "Picker.h"
#include "Events.h"

#include <vector>

class Device;
class MouseButtonEventArgs;
class KeyEventArgs;

class Scene {
	
	friend class Picker;

public:
	/// TODO: make directional light and skybox optional
	Scene(Device& device, CommandList& copyCommandList, CommandList& computeCommandList, const DirectionalLight::DirectionalLightParams& dirLightParams, const Skybox::SkyboxParams& skyboxParams, int windowWidth, int windowHeight);

	Scene(const Scene&) = delete;
	Scene& operator=(Scene&) = delete;
	Scene(Scene&&) = delete;
	Scene& operator=(Scene&&) = delete;

	void OnMouseButtonReleased(const MouseButtonEventArgs& e);
	void OnKeyPressed(const KeyEventArgs& e);
	void OnKeyReleased(const KeyEventArgs& e);

	const DirectionalLight& GetDirectionalLight() const { return m_DirectionalLight; };
	const Skybox& GetSkybox() const { return m_Skybox; };

	GameObject* CreateGameObject(CommandList& copyCommandList, const GameObject::GameObjectParams& goParams, std::shared_ptr<Mesh> mesh);
	void CreateGameObject(CommandList& copyCommandList, const GameObject::GameObjectParams& goParams, const std::wstring& meshFileName);

	void ComputeSkyboxIBLMaps(CommandList& directCommandList);

	/// TODO: test
	void SetCubemap(CommandList& copyCommandList, const std::wstring& hdrTextureName);
	void SetSkybox(CommandList& copyCommandList, CommandList& computeCommandList, const Skybox::SkyboxParams& skyboxParams);
	/// 

	void Render(const RenderTarget& targetRT, D3D12_VIEWPORT viewPort, CommandList& directCommandList, const UpdateEventArgs& e);
	
	void SetDirectionalLightAngle(float rotX, float rotY, float rotZ);

	void SetGameWindowSize(int width, int height) { 
		m_GameWindowWidth = width;
		m_GameWindowHeight = height;
	};

	// Currently only renders obnject inspector window
	void RenderImGui();

	// keep this public out of convenience for now
	Camera m_MainCamera;

private:
	/// TODO: Currently no system to validate destoyed objects, smarter storage needed
	///       Same size objects should at least be grouped together in separate arrays in a real engine
	std::vector<GameObject> m_SceneObjects;
	static const int sk_MaxSceneObjects = 100;

	// All materials used in this scene, currently just loads all material names in asset folder
	std::vector<std::wstring> m_MaterialNames;

	DirectionalLight m_DirectionalLight;
	Skybox m_Skybox;

	Device& m_Device;

	int m_GameWindowWidth;
	int m_GameWindowHeight;

	std::unique_ptr<Picker> m_Picker {};
};

