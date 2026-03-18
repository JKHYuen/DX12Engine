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

OutlinePSO::OutlinePSO(Device& device, DXGI_SAMPLE_DESC sampleDesc, D3D12_RT_FORMAT_ARRAY rtvFormats, DXGI_FORMAT depthFormat) {
	// Load outline shaders
	ComPtr<ID3DBlob> vs;
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Outline_VS.cso", &vs));
	ComPtr<ID3DBlob> ps;
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Outline_PS.cso", &ps));

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


	CD3DX12_RASTERIZER_DESC rasterDesc { CD3DX12_DEFAULT() };
	rasterDesc.CullMode = D3D12_CULL_MODE_BACK;

	CD3DX12_DEPTH_STENCIL_DESC dsDesc { D3D12_DEFAULT };
	dsDesc.DepthEnable = FALSE;
	dsDesc.StencilEnable = TRUE;

	dsDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	dsDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NOT_EQUAL;

	struct OutlinePipelineStateStream {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DSDesc;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterDesc;
	} outlinePipelineStateStream;

	outlinePipelineStateStream.pRootSignature = m_RootSignature->GetD3D12RootSignature().Get();
	outlinePipelineStateStream.InputLayout = VertexInput::GetInputLayout();
	outlinePipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	outlinePipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	outlinePipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	outlinePipelineStateStream.DSDesc = dsDesc;
	outlinePipelineStateStream.DSVFormat = depthFormat;
	outlinePipelineStateStream.RTVFormats = rtvFormats;
	outlinePipelineStateStream.SampleDesc = sampleDesc;
	outlinePipelineStateStream.RasterDesc = rasterDesc;

	device.CreatePipelineState(outlinePipelineStateStream, m_D3d12PipelineState);
}

void OutlinePSO::SetPipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_D3d12PipelineState);
	directCommandList.SetGraphicsRootSignature(m_RootSignature);
}

void OutlinePSO::UpdateResources(CommandList& directCommandList, VertexProps vertexProps, MaterialProps materialProps) {
	directCommandList.SetGraphicsDynamicConstantBuffer(OutlineRootParameters::VertexCB, vertexProps);
	directCommandList.SetGraphicsDynamicConstantBuffer(OutlineRootParameters::MaterialCB, materialProps);
}

