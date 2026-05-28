#pragma once

// Implementation based on: https://catlikecoding.com/unity/tutorials/advanced-rendering/bloom/

#include <vector>
#include <DirectXMath.h>

#include "DX12EngineCore/RenderTarget.h"
#include "EditorGui.h"
#include <cstdint>

using namespace DirectX;

class Device;
class BloomPSO;
class CommandList;

class BloomEffect {

	friend class EditorGui;

public:
	BloomEffect(Device& device, const RenderTarget& screenRenderTarget, BloomPSO* pso, int maxIterations = 16, float intensity = 0.5f, float threshold = 80.0f, float softThreshold = 0.9f, XMFLOAT4 colorMultiply = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), EditorGui::ImGuiDebugSRVIndex debugID = EditorGui::ImGuiDebugSRVIndex::BloomPrefilter);

	// "blendRenderTarget" is an optional render target if we want a texture other than inputRenderTarget to be blended with on the last render pass.
	// (Last render pass uses outputRenderTarget as render target and adds m_SamplingRenderTargets[0] and blendRenderTarget/inputRenderTarget together with intensity multiplier to apply bloom)
	// if b_MaskOutInput is true, bloom is only rendered where input alpha is 0 (this is mostly just used for outline effect)
	void Render(CommandList& directCommandList, const RenderTarget& inputRenderTarget, const RenderTarget& outputRenderTarget, const RenderTarget* blendRenderTarget = nullptr, bool b_MaskOutInput = false);

	void ResizeRenderTargets(uint32_t width, uint32_t height);

	void SetColorMultiply(float r, float g, float b) { m_ColorMultiply = { r, g, b, 1.0f }; }

private:
	float m_Intensity;
	float m_Threshold;
	float m_SoftThreshold;
	XMFLOAT4 m_ColorMultiply;
	
	EditorGui::ImGuiDebugSRVIndex m_DebugID;

	// Creates new render target at index idx in m_SamplingRenderTargets, SRV and RTV created as well
	void CreateSamplingRenderTarget(size_t idx, uint32_t textureWidth, uint32_t textureHeight);

	// Resized in ctor and ResizeRenderTargets()
	std::vector<RenderTarget> m_SamplingRenderTargets;

	Device& m_Device;

	// Owned by DemoGame class
	BloomPSO* m_PSO;

	DXGI_FORMAT m_TextureFormat;

	D3D12_SHADER_RESOURCE_VIEW_DESC m_DefaultSRVDesc {}; // Same SRV desc for all textures in the bloom pass

	// Max number of iterations (resolution halved iteration)
	int m_MaxIterationCount;

	// Actual down/up iterations used, can be different from m_MaxIterationCount (minimum texture height must be > 2)
	int m_IterationCount;
};

