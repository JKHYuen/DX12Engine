#pragma once

// This class manages/creates all pipeline states related to Bloom effect

#include <DirectXMath.h>
#include <memory>
#include <wrl/client.h>

using namespace Microsoft::WRL;
using namespace DirectX;

class Device;
class RenderTarget;
class RootSignature;
class CommandList;
struct ID3D12PipelineState;

class BloomPSO {
public:
	BloomPSO(Device& device, const RenderTarget& renderTarget);

	enum BloomRootParameters {
		BloomCB,
		Textures,

		NumBloomRootParameters
	};

	// Static textures only
	enum TextureIndex {
		SourceTex,
		ScreenTex,
		maskTex,

		NumTextures
	};

	struct alignas(16) BloomProps {
		XMFLOAT4 colorMultiply;
		XMFLOAT4 filter;
		float boxSampleDelta;
		float intensity;
		float usePrefilter; // 0.0f or 1.0f
		float useFinalPass; // 0.0f or 1.0f
	};

	void SetPipelineState(CommandList& directCommandList) const;
	void SetAdditivePipelineState(CommandList& directCommandList) const;

private:
	ComPtr<ID3D12PipelineState> m_BloomPSO;
	ComPtr<ID3D12PipelineState> m_BloomAdditivePSO;

	std::shared_ptr<RootSignature> m_RootSignature;
};

