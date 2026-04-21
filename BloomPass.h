#pragma once

// Implementation based on: https://catlikecoding.com/unity/tutorials/advanced-rendering/bloom/

#include <vector>
#include "RenderTarget.h"

class Device;
class BloomPSO;
class CommandList;

class BloomPass {
public:
	BloomPass(Device& device, const RenderTarget& screenRenderTarget, BloomPSO* pso, int maxIterations = 16);

	void Render(CommandList& directCommandList, const RenderTarget& inputRenderTarget, const RenderTarget& outputRenderTarget);

	void ResizeRenderTargets(uint32_t width, uint32_t height);

	void SetProps(float intensity, float threshold, float softThreshold);

private:
	float m_Intensity     = 1.0f;
	float m_Threshold     = 40.0f;
	float m_SoftThreshold = 0.9f;

	// Creates new render target at index idx in m_SamplingRenderTargets, SRV and RTV created as well
	void CreateSamplingRenderTarget(size_t idx, uint32_t textureWidth, uint32_t textureHeight);

	// Resized in ctor and ResizeRenderTargets()
	std::vector<RenderTarget> m_SamplingRenderTargets;

	Device& m_Device;

	// Owned by DemoGame class
	BloomPSO* m_PSO;

	DXGI_FORMAT m_TextureFormat;

	// Max number of iterations (resolution halved iteration)
	int m_MaxIterationCount;

	// Actual down/up iterations used, can be different from m_MaxIterationCount (minimum texture height must be > 2)
	int m_IterationCount;
};

