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
class CommandList;

class BloomPSO {
public:
	BloomPSO(Device& device, const RenderTarget& renderTarget);

	enum BloomRootParameters {
		BloomProps,
		Textures,

		NumBloomRootParameters
	};

	// Static textures only
	enum TextureIndex {
		ScreenTex,
		SourceTex,

		NumTextures
	};

	struct alignas(16) BloomProps {
		XMFLOAT4 filter;
		XMFLOAT4 bloomParams; // x: boxSampleDelta, y: intensity, z: usePrefilter [0, 1], w: useFinalPass [0, 1]
	};

	std::shared_ptr<RootSignature> GetRootSignature() const { return m_RootSignature; }
	void SetPipelineState(CommandList& directCommandList) const;

	ComPtr<ID3D12PipelineState> GetPSO() const { return m_BloomPSO; }
	ComPtr<ID3D12PipelineState> GetAdditivePSO() const { return m_BloomAdditivePSO; }


private:
	ComPtr<ID3D12PipelineState> m_BloomPSO;
	ComPtr<ID3D12PipelineState> m_BloomAdditivePSO;

	std::shared_ptr<RootSignature> m_RootSignature;
	
};

