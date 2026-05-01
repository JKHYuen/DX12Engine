#pragma once

/*
	Renderable gameobject with mesh and textures
	Simple implementation that only support objects using PBR shaders/pipeline
	
	NOTE:
		- AABB does not support rotation updates currently (it's non-trivial)
		- A dynamic component system should be used for render features
			- "m_PBR_PSO" will probably need to be a polymorphic type eventually
		- Can use dirty flag system for SRT/root sig updates
*/

#include "DirectXMath.h"
#include "DirectXCollision.h"
#include "PBRObjectPSO.h"

#include <memory>
#include <vector>
#include <string>

using namespace DirectX;

class CommandList;
class DirectionalLight;
class Mesh;
class Texture;
class Camera;
class UpdateEventArgs;
class Scene;
class OutlinePSO;

class GameObject {

	friend class EditorGui;

public:
	/// TODO: Should be able to use any PSO, not just PBRObjectPSO (need polymorphism)
	struct EntityParams {
		// string passed by value for convenience e.g. when modifying instances of this struct
		std::string name;
		Scene& scene;
		XMFLOAT3 translation, eulerRotation, scale;
	};

	/// Things that should be in some sort of component system:
	// Instance of RenderProps will be kept as member 
	struct RenderProps {
		std::wstring pbrMatName {};

		float heightMapMagnitude = 0.0f;

		float parallaxMagnitude = 0.0f;
		bool useParallaxShadow = false;
		int minParallaxLayers = 8;
		int maxParallaxLayers = 32;

		XMFLOAT2 uvScale { 1.0f, 1.0f };

		// PSOs are owned by DemoGame
		PBRObjectPSO* pbrPSO {};
		OutlinePSO* outlinePSO {};
	};

	// NOTE: copy command list must still be executed after GameObject, this is to keep flexibility to batch copy commands together
	// We don't use RenderProps&& so there isn't accidental object invalidation ofr the caller
	// Initialize with preconstructed mesh
	GameObject(CommandList& copyCommandList, const EntityParams& params, RenderProps renderProps, std::shared_ptr<Mesh> mesh);

	/// TODO: UNFINSIHED
	// Initialize with mesh loaded from file
	GameObject(CommandList& copyCommandList, const EntityParams& params, RenderProps renderProps, const std::wstring& meshFilePath);

	void Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene & scene);
	void RenderOutline(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene);

	void RenderToDirectionalShadowMap(CommandList& directCommandList, const DirectionalLight& directionalLight);

	// Currently only compatible with PBRObjectPSO
	void UpdatePBRShaderResources(CommandList& copyCommandList, const std::wstring& pbrMatName);

	void UpdateIBLShaderResources(const Scene& scene);

	XMFLOAT3 GetTranslation()   const { return m_Translation; };
	XMFLOAT3 GetEulerRotation() const { return m_EulerRotation; };
	XMFLOAT3 GetScale()         const { return m_Scale; };

	void SetTranslation(float x, float y, float z);
	void SetEulerRotation(float x, float y, float z);
	void SetScale(float x, float y, float z);

	/// Transform functions aren't very intuitive, good enough for now
	void Translate(float x, float y, float z);   // Adds to world position values
	void EulerRotate(float x, float y, float z); // Adds to euler angles
	void Scale(float x, float y, float z);       // Multiplies current scale (*not add)
	/// 

	std::string_view GetName() const      { return m_Name; }
	void SetName(const std::string& name) { m_Name = name; }

	const BoundingBox& GetAABB() const { return m_AABB; }
	std::shared_ptr<Mesh> GetMesh() const { return m_Mesh; }

	/// Things that should be in some sort of component system:
	void SetOutlineState(bool state) { mb_Outline = state; };
	///
	
private:
	std::shared_ptr<Mesh> m_Mesh {};
	std::vector<std::shared_ptr<Texture>> m_TextureResources { PBRObjectPSO::TextureIndex::NumTextures };
	
	BoundingBox m_AABB {};

	std::string m_Name;

	XMFLOAT4X4 m_TranslationMat {};
	XMFLOAT4X4 m_RotationMat {};
	XMFLOAT4X4 m_ScaleMat {};
	
	// Keep track of these separate from matrices for convenience in UI implementation
	XMFLOAT3 m_Translation, m_EulerRotation, m_Scale;

	/// Things that should be in some sort of component system:
	PBRObjectPSO::VertexProps m_PBRVertexCB {};
	RenderProps m_RenderProps {};
	bool mb_Outline {};
	///
};

