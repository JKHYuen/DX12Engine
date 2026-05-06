#pragma once
#include <memory>
#include "RenderTarget.h"

class Device;
class OutlinePSO;
class BloomPSO;
class BloomEffect;
class GameObject;
class CommandList;
class UpdateEventArgs;
class Scene;

class OutlineEffect {

	friend class EditorGui;

public:
	OutlineEffect(Device& device, const RenderTarget& screenRenderTarget, OutlinePSO* outlinePSO, BloomPSO* bloomPSO);

	bool Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene, const RenderTarget& blendRenderTarget, const RenderTarget& outputRenderTarget);

	void Resize(uint32_t width, uint32_t height);

private:
	std::unique_ptr<BloomEffect> m_BloomEffect;

	OutlinePSO* m_OutlinePSO;

	// Intermediate texture to draw outline, this will be blurred and overlayed onto the output render target
	RenderTarget m_OutlineSilhouetteRT;

	// Currently only used by Debug UI
	bool mb_DisableEffect = false;
};

