#pragma once

// Renderable gameobject with mesh and textures
// Simple implementation that only support objects using PBR shaders/pipeline
// "m_PBR_PSO" will probably need to be a polymorphic type eventually

#include <memory>
#include <vector>
#include <string>
#include "DirectXMath.h"

using namespace DirectX;

class CommandList;
class Mesh;
class PBRObjectPSO;
class Texture;
class Camera;
class UpdateEventArgs;
class Scene;

class GameObject {
public:
	struct GameObjectParams {
		const Scene& scene;
		XMMATRIX translationMat, rotationMat, scaleMat;
		PBRObjectPSO* PSO;
		const std::wstring& pbrMatName;
	};

	// Warning: copy command list must still be executed after GameObject, this is to keep flexibility to batch copy commands together
	GameObject(CommandList& copyCommandList, GameObjectParams params, std::shared_ptr<Mesh> mesh); // initialize with preconstructed mesh
	GameObject(CommandList& copyCommandList, GameObjectParams params, const std::wstring& meshFileName); // initialize with mesh loaded from file
	
	void Render(CommandList& directCommandList, const UpdateEventArgs & e, const Scene& scene);

	void RenderToDirectionalShadowMap(CommandList& directCommandList, const Scene& scene);
	
private:
	std::shared_ptr<Mesh> m_Mesh;

	std::vector<std::shared_ptr<Texture>> m_TextureResources;
	PBRObjectPSO* m_PSO;

	XMFLOAT4X4 m_TranslationMat;
	XMFLOAT4X4 m_RotationMat;
	XMFLOAT4X4 m_ScaleMat;
};

