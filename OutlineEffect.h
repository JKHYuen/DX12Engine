#pragma once
#include <memory>
#include "RenderTarget.h"

class Device;
class OutlinePSO;
class BloomPSO;
class BloomEffect;
class CommandList;

class OutlineEffect {
public:
	OutlineEffect(Device& device, const RenderTarget& screenRenderTarget, OutlinePSO* outlinePSO, BloomEffect* bloomRenderPass);

	void Render(CommandList& directCommandList, const RenderTarget& inputRenderTarget, const RenderTarget& outputRenderTarget);

private:
	OutlinePSO* m_OutlinePSO;
	BloomEffect* m_BloomEffect;

	// Intermediate texture to draw blurred outline to, this will be overlayed onto the output render target
	RenderTarget m_OutlineRT;
};

