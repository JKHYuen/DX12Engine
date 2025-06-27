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
	Skybox(Device& device, CommandList& copyCommandList, std::wstring hdrTextureName, std::unique_ptr<Mesh> reversedCube, RenderTarget& renderTarget);

	void Render(CommandList& directCommandList, const Camera& camera);

private:
	Device& m_Device;

	std::shared_ptr<RootSignature> m_SkyboxRootSignature;

	std::shared_ptr<Texture> m_HDRPanoTexture;
	std::shared_ptr<Texture> m_SkyCubemapTexture;

	std::shared_ptr<ShaderResourceView> m_SkyCubemapSRV;
};

