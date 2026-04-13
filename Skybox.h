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
class ImageBasedLightingPSO;

class Skybox {
public:
	struct SkyboxParams {
		std::wstring hdrTextureName;
		ImageBasedLightingPSO* iblPSO;
	};

	// Loads "hdrTextureName" from file as skybox
	Skybox(Device& device, CommandList& copyCommandList, CommandList& computeCommandList, SkyboxParams params);
	
	void SetCubemap(CommandList& copyCommandList, const std::wstring& hdrTextureName);

	// Draw skybox
	// Note: Render target needs to be set externally
	void Render(CommandList& directCommandList, const Camera& camera);

	// Compute/draw precomputed textures for IBL
	void ComputeIBLMaps(CommandList& directCommandList);

	std::shared_ptr<Texture> GetIrradianceTexture() const { return m_IrradianceConvolutionCubemap_RT.GetTexture(AttachmentPoint::Color0); };
	std::shared_ptr<Texture> GetPrefilterTexture()  const { return m_PrefilterCubemap_RT.GetTexture(AttachmentPoint::Color0); };
	std::shared_ptr<Texture> Get_BRDF_LUT_Texture() const { return m_BRDF_LUT_RT.GetTexture(AttachmentPoint::Color0); };

private:
	// PSO owned by DemoGame currently
	ImageBasedLightingPSO* m_IBL_PSO;

	std::shared_ptr<Mesh> m_SkyboxCubeMesh;

	std::shared_ptr<Texture> m_HDRPanoTexture;
	std::shared_ptr<Texture> m_SkyCubemapTexture;

	RenderTarget m_IrradianceConvolutionCubemap_RT;
	RenderTarget m_PrefilterCubemap_RT;
	RenderTarget m_BRDF_LUT_RT;
};

