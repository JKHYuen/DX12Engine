#pragma once

/*
	Renderable gameobject with mesh and textures
	Simple implementation that only support objects using PBR shaders/pipeline
	
	NOTE:
		- AABB does not support rotation updates currently (it's non-trivial)
		- A dynamic component system would be nice, however this makes effecient object storage hard
		  Currently just uses vector with no object destruction support
		- Can use dirty flag system for SRT updates
		- "m_PBR_PSO" will probably need to be a polymorphic type eventually
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
public:
	/// TODO: PSO objects should be added dynamically to GameObjects with polymorphism
	struct GameObjectParams {
		// string passed by value for convenience e.g. when modifying instances of this struct
		std::string name;
		std::wstring pbrMatName;
		const Scene& scene;
		XMFLOAT3 translation, eulerRotation, scale;
		PBRObjectPSO* pbrPSO;
		OutlinePSO* outlinePSO;
	};

	// Warning: copy command list must still be executed after GameObject, this is to keep flexibility to batch copy commands together
	GameObject(CommandList& copyCommandList, GameObjectParams params, std::shared_ptr<Mesh> mesh); // initialize with preconstructed mesh
	GameObject(CommandList& copyCommandList, GameObjectParams params, const std::wstring& meshFileName); // initialize with mesh loaded from file

	void Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene);

	void RenderToDirectionalShadowMap(CommandList& directCommandList, const DirectionalLight& directionalLight);

	// Currently only compatible with PBRObjectPSO
	void UpdateShaderResources(CommandList& copyCommandList, const std::wstring& pbrMatName);

	/// Transfom functions aren't very intuitive, good enough for now
	void Translate(float x, float y, float z); // Adds to world position values
	void EulerRotate(float x, float y, float z); // Adds to euler angles
	void Scale(float x, float y, float z); // Multiplies current scale
	/// 

	void SetTranslation(float x, float y, float z);
	void SetEulerRotation(float x, float y, float z);
	void SetScale(float x, float y, float z);

	XMFLOAT3 GetTranslation() const { return m_Translation; };
	XMFLOAT3 GetEulerRotation() const { return m_EulerRotation; };
	XMFLOAT3 GetScale() const { return m_Scale; };

	std::string_view GetName() const { return m_Name; }
	void SetName(const std::string& name) { m_Name = name; }

	const BoundingBox& GetAABB() const { return m_AABB; }

	std::shared_ptr<Mesh> GetMesh() const { return m_Mesh; }

	/// Things that should be in some sort of component system:
	void SetOutlineState(bool state) { b_Outline = state; };
	std::wstring_view GetMaterialName() const { return m_MaterialName; }
	///
	
private:
	std::shared_ptr<Mesh> m_Mesh {};
	std::vector<std::shared_ptr<Texture>> m_TextureResources { PBRObjectPSO::sk_NumTextures };
	
	BoundingBox m_AABB {};

	// PSOs are owned by DemoGame
	PBRObjectPSO* m_PBR_PSO {};
	OutlinePSO* m_Outline_PSO {};

	std::string m_Name;

	XMFLOAT4X4 m_TranslationMat {};
	XMFLOAT4X4 m_RotationMat {};
	XMFLOAT4X4 m_ScaleMat {};
	
	// Keep track of these separate from matrices for convenience in UI implementation
	XMFLOAT3 m_Translation, m_EulerRotation, m_Scale;

	/// Things that should be in some sort of component system:
	std::wstring m_MaterialName {};
	bool b_Outline {};
	///
};

