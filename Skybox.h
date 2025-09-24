#pragma once
#include <memory>
#include <string>
#include "RenderTarget.h"

class Device;
class RootSignature;
class Texture;
class CommandList;
class ShaderResourceView;
class Camera;
class Mesh;

class Skybox {
public:
	Skybox(Device& device, CommandList& copyCommandList, std::wstring hdrTextureName, std::shared_ptr<Mesh> cubeMesh, RenderTarget& renderTarget);
	
	// Draw skybox
	void Render(CommandList& directCommandList, const Camera& camera);

	// Compute/draw precomputed textures for IBL
	void ComputeIBLMaps(CommandList& directCommandList, const Camera& camera);

	std::shared_ptr<ShaderResourceView> GetIrradianceSRV() const { return m_IrradianceCubemapSRV; };
	std::shared_ptr<ShaderResourceView> GetPrefilterSRV() const { return m_PrefilterCubemapSRV; };
	std::shared_ptr<ShaderResourceView> Get_BRDF_LUT_SRV() const { return m_BRDF_LUT_SRV; };

private:
	std::shared_ptr<Texture> m_HDRPanoTexture;
	std::shared_ptr<Texture> m_SkyCubemapTexture;

	RenderTarget m_IrradianceConvolutionCubemap_RT;
	RenderTarget m_PrefilterCubemap_RT;

	std::shared_ptr<ShaderResourceView> m_SkyCubemapSRV;
	std::shared_ptr<ShaderResourceView> m_IrradianceCubemapSRV;
	std::shared_ptr<ShaderResourceView> m_PrefilterCubemapSRV;
	std::shared_ptr<ShaderResourceView> m_BRDF_LUT_SRV;
};

