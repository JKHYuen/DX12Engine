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
class DirectionalLight;

class GameObject {
public:
	void LoadResources(CommandList& copyCommandList, const Skybox& skybox, const DirectionalLight& directionalLight, const std::wstring& pbrMatName, const std::wstring& meshName);
	void Render();
	
private:
	std::shared_ptr<Mesh> m_Mesh;
	std::vector<std::shared_ptr<Texture>> textureResources;

	std::shared_ptr<PBRObjectPSO> pbrPSO;

	XMFLOAT4X4 translationMat;
	XMFLOAT4X4 rotationMat;
	XMFLOAT4X4 scaleMat;
};

