#pragma once
#include <memory>
#include <string>
#include "DX12EngineCore/RenderTarget.h"

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
		const std::wstring& hdrTextureName;
		ImageBasedLightingPSO* iblPSO;
	};

	// Loads "hdrTextureName" from file as skybox
	Skybox(Device& device, CommandList& copyCommandList, CommandList& computeCommandList, const SkyboxParams& params);
	
	void SetCubemap(CommandList& copyCommandList, CommandList& computeCommandList, const std::wstring& hdrTextureName);

	// Draw skybox
	// Note: Render target needs to be set externally
	void Render(CommandList& directCommandList, const Camera& camera);

	// Compute/draw precomputed textures for IBL
	void ComputeIBLMaps(CommandList& directCommandList);

	std::shared_ptr<Texture> GetIrradianceTexture() const { return m_IrradianceConvolutionCubemap_RT.GetTexture(AttachmentPoint::Color0); };
	std::shared_ptr<Texture> GetPrefilterTexture()  const { return m_PrefilterCubemap_RT.GetTexture(AttachmentPoint::Color0); };
	std::shared_ptr<Texture> Get_BRDF_LUT_Texture() const { return m_BRDF_LUT_RT.GetTexture(AttachmentPoint::Color0); };

	std::wstring_view GetTextureName() const { return m_SkyboxTextureName; }

private:
	std::wstring m_SkyboxTextureName;

	// PSO owned by DemoGame currently
	ImageBasedLightingPSO* m_IBL_PSO;

	std::shared_ptr<Mesh> m_SkyboxCubeMesh;

	std::shared_ptr<Texture> m_HDRPanoTexture;
	std::shared_ptr<Texture> m_SkyCubemapTexture;

	RenderTarget m_IrradianceConvolutionCubemap_RT;
	RenderTarget m_PrefilterCubemap_RT;
	RenderTarget m_BRDF_LUT_RT;
};

