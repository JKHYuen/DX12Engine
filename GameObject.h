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

#include "DirectXCollision.h"
#include "PBRObjectPSO.h"

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace DirectX;

class CommandList;
class DirectionalLight;
class Mesh;
class Texture;
class Camera;
class UpdateEventArgs;
class Scene;
class UnlitPSO;
class UnlitPrimitivePSO;
class PBRObjectPSO;

struct PBRVertexProps;
struct PBRLightProps;
struct PBRTessellationProps;

class GameObject {

	friend class EditorGui;

public:
	struct EntityParams {
		// string passed by value for convenience e.g. when modifying instances of this struct
		std::string name;
		Scene& scene;

		// Only support construction with degrees for now
		// This should ideally be a union with quaternion and radian representations, but will need some validation
		XMFLOAT3 scale, radianEulerRotation, translation;
	};

	/// TODO: Should be able to use any PSO, not just PBRObjectPSO (need polymorphism) / component system
	// Instance of RenderProps will be kept as member 
	// Most of these values are controlled directly by EditorGui right now, not ideal see note in EditorGui.h
	// This is to avoid writing tons of getter/setters
	struct RenderProps {
		std::wstring pbrMatName {};

		bool isShadowCaster = true;

		float heightMapMagnitude = 0.0f;

		// using PBRRenderFlags out of convenience, this could be error prone
		PBRRenderFlags tessellationModeFlag = PBRRenderFlags_UniformTessellation; 
		float tessellationMagnitude = 1.0f;

		float parallaxMagnitude = 0.0f;
		bool useParallaxShadow = false;
		int minParallaxLayers = 8;
		int maxParallaxLayers = 32;

		XMFLOAT2 uvScale { 1.0f, 1.0f };

		// PSOs are owned by DemoGame
		PBRObjectPSO* pbrPSO {};
	};

	// NOTE: copy command list must still be executed after GameObject, this is to keep flexibility to batch copy commands together
	// We don't use RenderProps&& so there isn't accidental object invalidation ofr the caller
	// Initialize with preconstructed mesh
	GameObject(CommandList& copyCommandList, const EntityParams& params, const RenderProps& renderProps, std::shared_ptr<Mesh> mesh);

	/// TODO: Initialize with mesh loaded from file
	//GameObject(CommandList& copyCommandList, const EntityParams& params, const RenderProps& renderProps, const std::wstring& meshFilePath);

	void Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene, bool b_RenderWireframe = false);

	void RenderSilhouette(CommandList& directCommandList, const UpdateEventArgs& e, UnlitPSO* unlitPSO, XMFLOAT4 color);

	void RenderBoundingBox(CommandList& directCommandList, const UpdateEventArgs& e, UnlitPrimitivePSO* unlitPSO, const Scene& scene, XMFLOAT4 color);

	void RenderToDirectionalShadowMap(CommandList& directCommandList, const DirectionalLight& directionalLight);

	// Currently only compatible with PBRObjectPSO
	void UpdatePBRShaderResourcesFromFile(CommandList& copyCommandList, const std::wstring& pbrMatName);

	void UpdateIBLShaderResources(const Scene& scene);

	XMFLOAT3 GetTranslation()   const { return m_Translation; };
	// Radians
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
	
private:
	std::shared_ptr<Mesh> m_Mesh {};
	std::vector<std::shared_ptr<Texture>> m_TextureResources { PBRObjectPSO::TextureIndex::NumTextures };
	
	BoundingBox m_AABB {};

	// Mesh local origin rotated by model's current rotation matrix
	// This is needed for AABB calc because rotating an object that's local origin is not world 0,0,0 moves AABB center
	XMFLOAT3 m_AABBOffset {};

	void RecalcAABB();

	std::string m_Name;

	XMFLOAT4X4 m_TranslationMat {};
	XMFLOAT4X4 m_RotationMat {};
	XMFLOAT4X4 m_ScaleMat {};

	// Cached and updated only when when rotation or scale matrix is updated (i.e. in translation and scale setters)
	XMFLOAT4X4 m_SRMat;
	
	// Keep track of these separate from matrices for convenience
	XMFLOAT3 m_Translation, m_EulerRotation /*Radians*/, m_Scale;

	bool b_RenderThisFrame;

	/// Things that should be in some sort of component system:
	// Note: Stored CB members are shared between different rendering methods e.g. render bounding box, render silhoutte
	PBRVertexProps m_PBRVertexCB {};
	PBRLightProps m_PBRLightCB {};
	PBRTessellationProps m_TessellationCB {};

	RenderProps m_RenderProps {};
	///
};

