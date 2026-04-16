#pragma once

// Implementation based on: https://catlikecoding.com/unity/tutorials/advanced-rendering/bloom/

#include <vector>
#include "RenderTarget.h"

class Device;
class BloomPSO;
class CommandList;

class BloomPass {
public:
	BloomPass(Device& device, const RenderTarget& screenRenderTarget, BloomPSO* pso, int maxIterations);

	void Render(CommandList& directCommandList);

private:
	std::vector<RenderTarget> m_SamplingRenderTargets;

	// Owned by DemoGame class
	BloomPSO* m_PSO;

	RenderTarget m_BloomOutputRT;

	// Actual down/up iterations used, can be different from maxIterations (minimum texture height must be > 2)
	int m_IterationCount;
};

