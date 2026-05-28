#pragma once

#include <cstdint>
#include <memory>

class Device;
class RenderTarget;
class UnlitPSO;
class BloomPSO;
class BloomEffect;
class GameObject;
class CommandList;
class UpdateEventArgs;
class Scene;

class OutlineEffect {

	friend class EditorGui;

public:
	OutlineEffect(Device& device, const RenderTarget& screenRenderTarget, UnlitPSO* outlinePSO, BloomPSO* bloomPSO);

	// blendRenderTarget is prerendered frame, outline is rendered on top of this
	bool Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene, const RenderTarget& blendRenderTarget, const RenderTarget& outputRenderTarget);

	void ResizeRenderTargets(uint32_t width, uint32_t height);

private:
	std::unique_ptr<BloomEffect> m_BloomEffect;

	UnlitPSO* m_UnlitPSO;

	// Intermediate texture to draw outline, this will be blurred and overlayed onto the output render target
	std::unique_ptr<RenderTarget> m_OutlineSilhouetteRT;

	// Currently only used by Debug UI
	bool mb_DisableEffect = false;
};

