#pragma once

// Renderable gameobject with mesh and textures
// Simple implementation that only support objects using PBR shaders/pipeline

#include <memory>
#include <vector>
#include <string>
#include "DirectXMath.h"

using namespace DirectX;

class CommandList;
class Mesh;
class PBRObjectPSO;
class Texture;
class Skybox;
class Camera;
class DirectionalLight;
class UpdateEventArgs;

class GameObject {
public:
	GameObject(XMMATRIX translationMat, XMMATRIX  rotationMat, XMMATRIX  scaleMat, DirectionalLight& directionalLight, Camera& mainCamera, std::shared_ptr<PBRObjectPSO> pbrPSO);
	
	// Load resources with a mesh from file name
	void LoadResources(CommandList& copyCommandList, const Skybox& skybox, const std::wstring& pbrMatName, const std::wstring& meshName);

	// Load resources with a created mesh from file name
	void LoadResources(CommandList& copyCommandList, const Skybox& skybox, const std::wstring& pbrMatName, std::shared_ptr<Mesh> mesh);

	void Render(CommandList& directCommandList, UpdateEventArgs& e);

	void RenderToDirectionalShadowMap(CommandList& directCommandList);
	
private:
	std::shared_ptr<Mesh> m_Mesh;

	std::vector<std::shared_ptr<Texture>> m_TextureResources;
	std::shared_ptr<PBRObjectPSO> m_PBR_PSO;

	Camera& m_MainCamera;
	DirectionalLight& m_DirectionalLight;

	XMFLOAT4X4 m_TranslationMat;
	XMFLOAT4X4 m_RotationMat;
	XMFLOAT4X4 m_ScaleMat;
};

