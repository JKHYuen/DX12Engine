#pragma once
#include "Camera.h"
#include "DirectionalLight.h"
#include "Events.h"
#include "GameObject.h"
#include "Picker.h" // unique_ptr member needs this for some reason, unsure why
#include "Skybox.h"

#include "DX12EngineCore\CommandList.h"
#include "DX12EngineCore\RenderTarget.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Device;
class IGame;
class MouseButtonEventArgs;
class KeyEventArgs;
class UnlitPSO;
class UnlitPrimitivePSO;

class Scene {

	friend class EditorGui;
	friend class Picker;

public:
	Scene(Device& device, CommandList& copyCommandList, CommandList& computeCommandList, const DirectionalLight::DirectionalLightParams& dirLightParams, const Skybox::SkyboxParams& skyboxParams, const IGame& game);

	Scene(const Scene&)            = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&)				   = delete;
	Scene& operator=(Scene&&)	   = delete;

	void OnMouseButtonReleased(const MouseButtonEventArgs& e);
	void OnKeyPressed(const KeyEventArgs& e);
	void OnKeyReleased(const KeyEventArgs& e);

	enum AABBRenderMode {
		None,
		All,
		PickedOnly
	};

	const DirectionalLight& GetDirLight() const { return m_DirectionalLight; }

	const Skybox& GetSkybox() const { return m_Skybox; }
	Picker* const GetPicker() const;

	void ComputeSkyboxIBLs(CommandList& directCommandList);

	/// TODO: figure out some proper game object storage, pointers to m_SceneObjects can be invalidated
	template<class ...TArgs>
	void AddGameObject(TArgs&&... tArgs) {
		assert(m_SceneObjects.size() < sk_MaxSceneObjects);
		m_SceneObjects.emplace_back(std::forward<TArgs>(tArgs)...);
	}

	/// TODO: test
	void SetSkybox(CommandList& copyCommandList, CommandList& computeCommandList, const Skybox::SkyboxParams& skyboxParams);
	/// 

	void Render(const RenderTarget& outputRT, CommandList& directCommandList, const UpdateEventArgs& e);	
	void RenderBoundingBoxes(const RenderTarget& outputRT, CommandList& directCommandList, const UpdateEventArgs& e, UnlitPrimitivePSO* unlitPrimitivePSO);
	
	uint32_t GetWindowWidth()  const;
	uint32_t GetWindowHeight() const;

	void SetAABBRenderMode(AABBRenderMode mode) {
		m_AABBRenderMode = mode;
	}

	// keep this public out of convenience for now
	Camera m_MainCamera;

private:
	static const int sk_MaxSceneObjects = 100000;
	/// TODO: Currently no system to validate destoyed objects, smarter storage needed
	///       Same size objects should at least be grouped together in separate arrays in a real engine
	std::vector<GameObject> m_SceneObjects;

	bool mb_WireframeRender = false;

	// All materials used in this scene, currently just loads all material names in asset folder
	std::vector<std::wstring> m_MaterialNames;

	DirectionalLight m_DirectionalLight;
	Skybox m_Skybox;
	
	// Device owned by IGame
	Device& m_Device;

	const IGame& m_Game;

	std::unique_ptr<Picker> m_Picker;

	AABBRenderMode m_AABBRenderMode;

	/// TODO: TEMP
	bool mb_ChangeSkybox = false;
};

