#pragma once

// Renderable gameobject with mesh and textures
// Simple implementation that only support objects using PBR shaders/pipeline

#include <string>
#include <memory>

class CommandList;
class Mesh;
class Texture;

class GameObject {
public:
	GameObject(std::shared_ptr<CommandList> copyCommandList, std::wstring pbrMatName, std::shared_ptr<Mesh> mesh);

	void Initialize(std::shared_ptr<CommandList> copyCommandList, std::shared_ptr<Mesh> mesh);
	
private:
	std::shared_ptr<Mesh> m_Mesh;

	std::shared_ptr<Texture> m_AlbedoTexture;
	std::shared_ptr<Texture> m_NormalTexture;
	std::shared_ptr<Texture> m_MaterialTexture; // r: AO, g: metallic, b: roughness, a: height 
};

