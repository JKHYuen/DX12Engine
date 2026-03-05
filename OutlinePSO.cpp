#include "OutlinePSO.h"
#include "Commandlist.h"
#include "Device.h"
#include "RootSignature.h"
#include "Helpers.h"


#include <d3dx12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

using namespace DirectX;
using namespace Microsoft::WRL;

OutlinePSO::OutlinePSO(Device& device, DXGI_SAMPLE_DESC sampleDesc, D3D12_RT_FORMAT_ARRAY rtvFormat, DXGI_FORMAT depthFormat) {
	// Load outline shaders
	ComPtr<ID3DBlob> vs;
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PBR_VS.cso", &vs));
	ComPtr<ID3DBlob> ps;
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PBR_PS.cso", &ps));

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags_VSPS =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	// Create outline effect root signature
	{
		CD3DX12_ROOT_PARAMETER1 rootParameters[OutlineRootParameters::NumOutlineRootParameters];
		rootParameters[OutlineRootParameters::VertexCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[OutlineRootParameters::MaterialCB].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(OutlineRootParameters::NumOutlineRootParameters, rootParameters, 0, nullptr, rootSignatureFlags_VSPS);
		m_RootSignature = std::make_shared<RootSignature>(device, rootSignatureDescription.Desc_1_1);
	}

	struct OutlinePipelineStateStream {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
	} hdrPipelineStateStream;

	hdrPipelineStateStream.pRootSignature = m_RootSignature->GetD3D12RootSignature().Get();
	hdrPipelineStateStream.InputLayout = VertexInput::GetInputLayout();
	hdrPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	hdrPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	hdrPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	hdrPipelineStateStream.DSVFormat = depthFormat;
	hdrPipelineStateStream.RTVFormats = rtvFormat;
	hdrPipelineStateStream.SampleDesc = sampleDesc;

	/// TODO: move this to device class
	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = { sizeof(OutlinePipelineStateStream), &hdrPipelineStateStream };
	ThrowIfFailed(device.GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_D3d12PipelineState)));
}

void OutlinePSO::SetPipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_D3d12PipelineState);
	directCommandList.SetGraphicsRootSignature(m_RootSignature);
}

void OutlinePSO::UpdateResources(CommandList& directCommandList, VertexProps vertexProps, MaterialProps materialProps) {

}

