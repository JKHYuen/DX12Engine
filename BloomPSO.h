#pragma once

// This class manages/creates all pipeline states related to Bloom effect

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>

using namespace Microsoft::WRL;
using namespace DirectX;

class Device;
class RenderTarget;
class RootSignature;

class BloomPSO {
public:
	BloomPSO(Device& device, const RenderTarget& renderTarget);

	enum BloomRenderType {
		Prefilter,
		Downsample,
		Upsample,
		Combine,

		NumBloomRenderType
	};

	// Static textures only
	enum TextureIndex {
		ScreenTex,
		SourceTex,

		NumTextures
	};

	struct alignas(16) MaterialProps {
		XMFLOAT4 filter;
		XMFLOAT4 bloomParams; // x: boxSampleDelta, y: intensity, z: usePrefilter [0, 1], w: useFinalPass [0, 1]
	};

private:

	ComPtr<ID3D12PipelineState> m_PrefilterPSO;
	ComPtr<ID3D12PipelineState> m_DownsamplePSO;
	ComPtr<ID3D12PipelineState> m_UpsamplePSO;
	ComPtr<ID3D12PipelineState> m_CombinePSO;

	std::shared_ptr<RootSignature> m_PrefilterRootSignature;
	std::shared_ptr<RootSignature> m_DownsampleRootSignature;
	std::shared_ptr<RootSignature> m_UpsampleRootSignature;
	std::shared_ptr<RootSignature> m_CombineRootSignature;
	
};

