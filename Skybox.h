#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <memory>
#include <string>

class Device;
class RootSignature;
class RenderTarget;
class Texture;
class CommandList;
class ShaderResourceView;
class Camera;
class Mesh;
class RenderTarget;

class Skybox {
public:
	Skybox(Device& device, CommandList& copyCommandList, std::wstring hdrTextureName, std::unique_ptr<Mesh> cubeMesh, RenderTarget& renderTarget);

	void Render(CommandList& directCommandList, const Camera& camera);

private:
	std::shared_ptr<RootSignature> m_SkyboxRootSignature;

	std::shared_ptr<Texture> m_HDRPanoTexture;
	std::shared_ptr<Texture> m_SkyCubemapTexture;

	std::shared_ptr<ShaderResourceView> m_SkyCubemapSRV;
};

