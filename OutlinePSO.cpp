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

OutlinePSO::OutlinePSO(Device& device, D3D12_RT_FORMAT_ARRAY rtvFormats, std::shared_ptr<RootSignature> objectRootSignature)
	: m_ObjectRootSignature(objectRootSignature)
{
	CD3DX12_RASTERIZER_DESC rasterDesc { D3D12_DEFAULT };
	rasterDesc.CullMode = D3D12_CULL_MODE_NONE;

	struct OutlinePipelineStateStream {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_HS HS;
		CD3DX12_PIPELINE_STATE_STREAM_DS DS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterDesc;
	} outlinePipelineStateStream;

	outlinePipelineStateStream.pRootSignature = m_ObjectRootSignature->GetD3D12RootSignature().Get();
	outlinePipelineStateStream.InputLayout = VertexInput::Get_POS_NORM_TAN_BIT_UV_InputLayout();
	outlinePipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	outlinePipelineStateStream.VS = AssetImporter::GetCompiledShaderFromFile(L"PBR_VS.cso");
	outlinePipelineStateStream.HS = AssetImporter::GetCompiledShaderFromFile(L"PBR_HS.cso");
	outlinePipelineStateStream.DS = AssetImporter::GetCompiledShaderFromFile(L"PBR_DS.cso");
	outlinePipelineStateStream.PS = AssetImporter::GetCompiledShaderFromFile(L"Outline_PS.cso");
	outlinePipelineStateStream.RTVFormats = rtvFormats;
	outlinePipelineStateStream.RasterDesc = rasterDesc;

	device.CreatePipelineState(outlinePipelineStateStream, m_D3d12PipelineState);
}

void OutlinePSO::SetPipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_D3d12PipelineState);
	directCommandList.SetGraphicsRootSignature(m_ObjectRootSignature);
}

void OutlinePSO::UpdateResources(CommandList& directCommandList, const PBRVertexProps& vertexProps, const PBRTessellationProps& tessProps) {
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::VertexCB, vertexProps);
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::TessellationCB, tessProps);
}
