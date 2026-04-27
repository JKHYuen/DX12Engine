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

	friend class EditorGui;
	friend class Picker;

public:
	Scene(Device& device, CommandList& copyCommandList, CommandList& computeCommandList, const DirectionalLight::DirectionalLightParams& dirLightParams, const Skybox::SkyboxParams& skyboxParams, int windowWidth, int windowHeight);

	Scene(const Scene&)            = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&)				   = delete;
	Scene& operator=(Scene&&)	   = delete;

	void OnMouseButtonReleased(const MouseButtonEventArgs& e);
	void OnKeyPressed(const KeyEventArgs& e);
	void OnKeyReleased(const KeyEventArgs& e);

	// Note: non const functions that need to be called outside of this class need to be called through scene - not ideal, preserves encapsulation
	const DirectionalLight& GetDirLight() const { return m_DirectionalLight; }
	const Skybox& GetSkybox() const { return m_Skybox; }

	void ComputeSkyboxIBLs(CommandList& directCommandList);
	void SetDirLightAngle(float x, float y, float z);
	void SetDirLightColor(float r, float g, float b);

	// We use std::move to move gameobject "go" into m_SceneObjects instead of constructing directly with emplace_back with Gameobject params so we dont have to keep track of Gameobject constructor params (they might change). "go" is an rvalue ref to avoid copy and to inform the caller object will be moved (i.e. invalidated).
	void AddGameObject(GameObject&& go);

	/// TODO: test
	void SetCubemap(CommandList& copyCommandList, const std::wstring& hdrTextureName);
	void SetSkybox(CommandList& copyCommandList, CommandList& computeCommandList, const Skybox::SkyboxParams& skyboxParams);
	/// 

	void Render(const RenderTarget& targetRT, D3D12_VIEWPORT viewPort, CommandList& directCommandList, const UpdateEventArgs& e);

	// For object picking
	void SetGameWindowSize(int width, int height) { 
		m_GameWindowWidth = width;
		m_GameWindowHeight = height;
	};

	// keep this public out of convenience for now
	Camera m_MainCamera;

private:
	/// TODO: TEMP
	bool s_ChangeSkybox = false;

	/// TODO: Currently no system to validate destoyed objects, smarter storage needed
	///       Same size objects should at least be grouped together in separate arrays in a real engine
	std::vector<GameObject> m_SceneObjects;
	static const int sk_MaxSceneObjects = 100;

	// All materials used in this scene, currently just loads all material names in asset folder
	std::vector<std::wstring> m_MaterialNames;

	DirectionalLight m_DirectionalLight;
	Skybox m_Skybox;

	Device& m_Device;

	// For object picking
	int m_GameWindowWidth;
	int m_GameWindowHeight;
	//

	std::unique_ptr<Picker> m_Picker {};
};

