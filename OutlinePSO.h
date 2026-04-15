#pragma once

/*
	PSO for outline effect (front cull hull shader).
	This and other "PSO" classes are ad hoc and a more generalized implementation will be needed in the future.
*/

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

using namespace DirectX;
using namespace Microsoft::WRL;

class Device;
class RootSignature;
class CommandList;

class OutlinePSO {
public:
	OutlinePSO(Device& device, DXGI_SAMPLE_DESC sampleDesc, D3D12_RT_FORMAT_ARRAY rtvFormats, DXGI_FORMAT depthFormat);

	enum OutlineRootParameters {
		VertexCB,         // ConstantBuffer<Mat> VertexCB		 : register(b0);
		MaterialCB,       // ConstantBuffer<Material> MaterialCB : register(b0, space1);
		NumOutlineRootParameters
	};

	struct alignas(16) VertexProps {
		XMFLOAT4X4 SRT;
		XMFLOAT4X4 MVP;
		XMFLOAT4   screenParams;
	};

	struct alignas(16) MaterialProps {
		XMFLOAT4   outlineColor;
	};

	std::shared_ptr<RootSignature> GetRootSignature() const { return m_RootSignature; }

	void SetPipelineState(CommandList& directCommandList) const;

	void UpdateResources(CommandList& directCommandList, VertexProps vertexProps, MaterialProps materialProps);

private:
	std::shared_ptr<RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_D3d12PipelineState;
};

