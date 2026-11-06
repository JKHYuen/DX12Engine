#pragma once
#include "DirectionalLight.h"
#include "Events.h"
#include "GameObject.h"
#include "PointLight.h"
#include "RenderConstants.h"
#include "Skybox.h"

#include "DX12EngineCore\CommandList.h"
#include "DX12EngineCore\RenderTarget.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <utility>

class Camera;
class Device;
class GameObject;
class UnlitPSO;
class IGame;
class MouseButtonEventArgs;
class KeyEventArgs;
class UnlitPSO;
class UnlitPrimitivePSO;

class Scene {

	friend class EditorGui;
	friend class Picker;

public:
	Scene(Device& device, CommandList& copyCommandList, CommandList& computeCommandList, const DirectionalLight::DirectionalLightParams& dirLightParams, const Skybox::SkyboxParams& skyboxParams, UnlitPSO* unlitPSO, const IGame& game);

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

	const DirectionalLight* GetDirLight() const { return m_DirectionalLight.get(); }
	const PointLight* GetPointLight(uint32_t idx) const { 
		assert(idx < RenderGlobals::gk_MaxPointLightCount); 
		return m_PointLights[idx].get();
	}

	const Skybox* GetSkybox() const { return m_Skybox.get(); }

	void ComputeSkyboxIBLs(CommandList& directCommandList);

	// Note: see comment on m_SceneObjects member
	//template<class ...TArgs>
	//void AddGameObject(TArgs&&... tArgs) {
	//	assert(m_SceneObjects.size() < sk_MaxSceneObjects);
	//	m_SceneObjects.emplace_back(std::make_unique(std::forward<TArgs>(tArgs)...));
	//}

	// Note: see comment on m_SceneObjects member
	void AddGameObject(std::unique_ptr<GameObject> go) {
		assert(m_SceneObjects.size() < sk_MaxSceneObjects);
		m_SceneObjects.push_back(std::move(go));
	}

	void SetSkybox(CommandList& copyCommandList, CommandList& computeCommandList, const std::wstring& hdrTextureName);
	
	// Window width and height only used for shader buffers that need it, this is the most straight forward way to pass it for now
	void Render(const RenderTarget& outputRT, CommandList& directCommandList, const UpdateEventArgs& e, Camera& camera, uint32_t windowWidth, uint32_t windowHeight);
	// pickedObject can be nullptr
	void RenderBoundingBoxes(const RenderTarget& outputRT, CommandList& directCommandList, const UpdateEventArgs& e, const Camera& camera, UnlitPrimitivePSO* unlitPrimitivePSO, GameObject* pickedObject);

	// This probably shouldn't be in this class
	void SetAABBRenderMode(AABBRenderMode mode) {m_AABBRenderMode = mode; }

	//Camera& GetMainCamera() const { return *m_MainCamera; }

private:
	static const int sk_MaxSceneObjects = 65536; // arbitrary
	/// TODO: Currently no system to validate destoyed objects, smarter storage needed.
	///       Same size objects should at least be grouped together (value storage) in separate arrays in a real engine.
	///       This is difficult until I convert GameObject class hierarchy into a component system.
	std::vector<std::unique_ptr<GameObject>> m_SceneObjects;

	// Maybe move this to a more relevant class (IGame?)
	bool mb_WireframeRender = false;

	// All materials used in this scene, currently just loads all material names in asset folder
	std::vector<std::wstring> m_MaterialNames;

	std::unique_ptr<DirectionalLight> m_DirectionalLight;
	std::vector<std::unique_ptr<PointLight>> m_PointLights;

	std::unique_ptr<Skybox> m_Skybox;

	// Device owned by IGame
	Device& m_Device;

	/// TODO: refactor access to PSO 
	// Only for point light visualization
	UnlitPSO* m_UnlitPSO;

	AABBRenderMode m_AABBRenderMode;
};

