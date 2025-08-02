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
	Skybox(Device& device, CommandList& copyCommandList, std::wstring hdrTextureName, std::unique_ptr<Mesh> cubeMesh, RenderTarget& renderTarget);
	
	// Draw skybox
	void Render(CommandList& directCommandList, const Camera& camera);

	enum ComputeMode {
		kConvolutionRender   = 0,
		kPrefilterRender     = 1,
		kIntegrateBRDFRender = 2,
		NumComputeType
	};

	// Compute/draw precomputed textures for IBL
	void Precompute(CommandList& directCommandList, const Camera& camera, ComputeMode mode);

	std::shared_ptr<ShaderResourceView> GetIrradianceSRV() const;

private:

	std::shared_ptr<Texture> m_HDRPanoTexture;
	std::shared_ptr<Texture> m_SkyCubemapTexture;

	RenderTarget m_IrradianceConvolutionCubemap_RT;
	RenderTarget m_PrefilterCubemap_RT;

	std::shared_ptr<ShaderResourceView> m_SkyCubemapSRV;
	std::shared_ptr<ShaderResourceView> m_IrradianceCubemapSRV;
	std::shared_ptr<ShaderResourceView> m_PrefilterCubemapSRV;
};

