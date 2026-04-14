#pragma once

// Implementation based on: https://catlikecoding.com/unity/tutorials/advanced-rendering/bloom/

#include <vector>
#include "RenderTarget.h"

class Device;

class BloomPass {
public:
	BloomPass(Device& device, const RenderTarget& screenRenderTarget, int maxIterations);

private:
	std::vector<RenderTarget> m_SamplingRenderTargets;
	RenderTarget m_BloomOutputRT;

	// Actual down/up iterations used, can be different from maxIterations (minimum texture height must be > 2)
	int m_IterationCount;
};

