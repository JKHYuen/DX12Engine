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

private:
	float m_Intensity     = 1.0f;
	float m_Threshold     = 40.0f;
	float m_SoftThreshold = 0.9f;

	std::vector<RenderTarget> m_SamplingRenderTargets;

	// Owned by DemoGame class
	BloomPSO* m_PSO;

	// Actual down/up iterations used, can be different from maxIterations (minimum texture height must be > 2)
	int m_IterationCount;
};

