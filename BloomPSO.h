#pragma once

// This class manages/creates all pipeline states related to Bloom effect

#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

using namespace Microsoft::WRL;

class Device;
class RenderTarget;
class RootSignature;

class BloomPSO {
public:
	enum BloomRenderType {
		Prefilter,
		Downsample,
		Upsample,
		Combine,

		NumBloomRenderType
	};

	BloomPSO(Device& device, const RenderTarget& renderTarget);

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

