#pragma once

// Implementation based on: https://catlikecoding.com/unity/tutorials/advanced-rendering/bloom/

#include <vector>
#include "RenderTarget.h"

class Device;
class BloomPSO;
class CommandList;

class BloomEffect {

	friend class EditorGui;

public:
	BloomEffect(Device& device, const RenderTarget& screenRenderTarget, BloomPSO* pso, int maxIterations = 16, float intensity = 0.5f, float threshold = 80.0f, float softThreshold = 0.9f);

	// "blendRenderTarget" is an optional render target if we want a texture other than inputRenderTarget to be blended with on the last render pass.
	// (Last render pass uses outputRenderTarget as render target and adds m_SamplingRenderTargets[0] and blendRenderTarget/inputRenderTarget together with intensity multiplier to apply bloom)
	void Render(CommandList& directCommandList, const RenderTarget& inputRenderTarget, const RenderTarget& outputRenderTarget, const RenderTarget* blendRenderTarget = nullptr, bool b_MaskOutInput = false);

	void ResizeRenderTargets(uint32_t width, uint32_t height);

private:
	float m_Intensity;
	float m_Threshold;
	float m_SoftThreshold;

	// Creates new render target at index idx in m_SamplingRenderTargets, SRV and RTV created as well
	void CreateSamplingRenderTarget(size_t idx, uint32_t textureWidth, uint32_t textureHeight);

	// Resized in ctor and ResizeRenderTargets()
	std::vector<RenderTarget> m_SamplingRenderTargets;

	Device& m_Device;

	// Owned by DemoGame class
	BloomPSO* m_PSO;

	DXGI_FORMAT m_TextureFormat;
	D3D12_SHADER_RESOURCE_VIEW_DESC m_SRVDesc {}; // Same SRV desc for all textures in the bloom pass

	// Max number of iterations (resolution halved iteration)
	int m_MaxIterationCount;

	// Actual down/up iterations used, can be different from m_MaxIterationCount (minimum texture height must be > 2)
	int m_IterationCount;
};

