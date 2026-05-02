#include "OutlinePSO.h"
#include "Commandlist.h"
#include "Device.h"
#include "RootSignature.h"
#include "Helpers.h"
#include "AssetImporter.h"
#include "PBRObjectPSO.h"

#include <d3dx12.h>
#include <wrl/client.h>

using namespace DirectX;
using namespace Microsoft::WRL;

OutlinePSO::OutlinePSO(Device& device, D3D12_RT_FORMAT_ARRAY rtvFormats, DXGI_FORMAT depthFormat, std::shared_ptr<RootSignature> objectRootSignature)
	: m_ObjectRootSignature(objectRootSignature)
{
	CD3DX12_RASTERIZER_DESC rasterDesc { D3D12_DEFAULT };
	rasterDesc.CullMode = D3D12_CULL_MODE_BACK;

	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc { D3D12_DEFAULT };

	struct OutlinePipelineStateStream {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencilDesc;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterDesc;
	} outlinePipelineStateStream;

	outlinePipelineStateStream.pRootSignature = m_ObjectRootSignature->GetD3D12RootSignature().Get();
	outlinePipelineStateStream.InputLayout = VertexInput::Get_POS_NORM_TAN_BIT_UV_InputLayout();
	outlinePipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	outlinePipelineStateStream.VS = AssetImporter::GetCompiledShaderFromFile(L"PBR_VS.cso");
	outlinePipelineStateStream.PS = AssetImporter::GetCompiledShaderFromFile(L"Outline_PS.cso");
	outlinePipelineStateStream.DepthStencilDesc = depthStencilDesc;
	outlinePipelineStateStream.DSVFormat = depthFormat;
	outlinePipelineStateStream.RTVFormats = rtvFormats;
	outlinePipelineStateStream.RasterDesc = rasterDesc;

	device.CreatePipelineState(outlinePipelineStateStream, m_D3d12PipelineState);

	// Create stencil write PSO
	depthStencilDesc.DepthEnable   = FALSE;
	depthStencilDesc.StencilEnable = TRUE;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_REPLACE;
	depthStencilDesc.FrontFace.StencilPassOp      = D3D12_STENCIL_OP_REPLACE;
	depthStencilDesc.FrontFace.StencilFunc        = D3D12_COMPARISON_FUNC_ALWAYS;
	outlinePipelineStateStream.DepthStencilDesc = depthStencilDesc;

	device.CreatePipelineState(outlinePipelineStateStream, m_StencilWrite_D3d12PipelineState);
}

void OutlinePSO::SetPipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_D3d12PipelineState);
	directCommandList.SetGraphicsRootSignature(m_ObjectRootSignature);
}

void OutlinePSO::SetStencilWritePipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_StencilWrite_D3d12PipelineState);
	directCommandList.SetGraphicsRootSignature(m_ObjectRootSignature);
}

void OutlinePSO::UpdateResources(CommandList& directCommandList, PBRObjectPSO::VertexProps vertexProps, PBRObjectPSO::LightProps lightProps) {
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::VertexCB, vertexProps);
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::LightCB, lightProps);
}
