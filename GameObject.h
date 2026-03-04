#pragma once

// Renderable gameobject with mesh and textures
// Simple implementation that only support objects using PBR shaders/pipeline
// "m_PBR_PSO" will probably need to be a polymorphic type eventually

#include <memory>
#include <vector>
#include <string>
#include "DirectXMath.h"
#include "DirectXCollision.h"
#include "PBRObjectPSO.h"

using namespace DirectX;

class CommandList;
class Mesh;
class Texture;
class Camera;
class UpdateEventArgs;
class Scene;

class GameObject {
public:
	GameObject() = default;

	struct GameObjectParams {
		const Scene& scene;
		XMFLOAT3 translation, eulerRotation, scale;
		PBRObjectPSO* PSO;
		const std::wstring& pbrMatName;
	};

	// Warning: copy command list must still be executed after GameObject, this is to keep flexibility to batch copy commands together
	GameObject(CommandList& copyCommandList, GameObjectParams params, std::shared_ptr<Mesh> mesh); // initialize with preconstructed mesh
	GameObject(CommandList& copyCommandList, GameObjectParams params, const std::wstring& meshFileName); // initialize with mesh loaded from file
	
	void Render(CommandList& directCommandList, const UpdateEventArgs & e, const Scene& scene);

	void RenderToDirectionalShadowMap(CommandList& directCommandList, const Scene& scene);

	// Currently only compatible with PBRObjectPSO
	void UpdateShaderResources(CommandList& copyCommandList, const std::wstring& pbrMatName);

	void Translate(float x, float y, float z);
	void Rotate(float x, float y, float z);
	void Scale(float x, float y, float z);

	void SetTranslation(float x, float y, float z);
	void SetRotation(float x, float y, float z);
	void SetScale(float x, float y, float z);

	const BoundingBox& GetAABB() const { return m_AABB; }

	// Note: does not deal with rotations at the moment
	void UpdateAABB();
	
private:
	std::shared_ptr<Mesh> m_Mesh {};
	BoundingBox m_AABB {};

	std::vector<std::shared_ptr<Texture>> m_TextureResources { PBRObjectPSO::sk_NumTextures };

	// PSO is managed/owned by a seperate class, right now it's DemoGame
	PBRObjectPSO* m_PSO {};

	XMFLOAT4X4 m_TranslationMat {};
	XMFLOAT4X4 m_RotationMat {};
	XMFLOAT4X4 m_ScaleMat {};
};

