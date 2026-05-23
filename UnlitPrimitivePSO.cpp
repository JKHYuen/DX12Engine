#include "UnlitPrimitivePSO.h"

#include "DX12EngineCore/Commandlist.h"
#include "DX12EngineCore/Device.h"
#include "DX12EngineCore/RootSignature.h"

#include "Helpers.h"
#include "AssetImporter.h"
#include "PBRObjectPSO.h"

#include <d3dx12.h>
#include <wrl/client.h>

using namespace DirectX;
using namespace Microsoft::WRL;

UnlitPrimitivePSO::UnlitPrimitivePSO(Device& device, D3D12_RT_FORMAT_ARRAY rtvFormats, std::shared_ptr<RootSignature> objectRootSignature)
	: m_ObjectRootSignature(objectRootSignature) {
	CD3DX12_RASTERIZER_DESC rasterDesc { D3D12_DEFAULT };

	struct UnlitPipelineStateStream {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterDesc;
	} pipelineStateStream;

	pipelineStateStream.pRootSignature = m_ObjectRootSignature->GetD3D12RootSignature().Get();
	pipelineStateStream.InputLayout = VertexInput::Get_POS_NORM_TAN_BIT_UV_InputLayout();
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.VS = AssetImporter::Get().GetCompiledShaderFromFile(L"UnlitPrimitive_VS.cso");
	pipelineStateStream.PS = AssetImporter::Get().GetCompiledShaderFromFile(L"Unlit_PS.cso");
	pipelineStateStream.RTVFormats = rtvFormats;
	pipelineStateStream.RasterDesc = rasterDesc;

	device.CreatePipelineState(pipelineStateStream, m_PipelineState);

	// Wireframe render PSO
	rasterDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
	//rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
	pipelineStateStream.RasterDesc = rasterDesc;
	device.CreatePipelineState(pipelineStateStream, m_WireframePipelineState);

}

void UnlitPrimitivePSO::SetPipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_PipelineState);
	directCommandList.SetGraphicsRootSignature(m_ObjectRootSignature);
}

void UnlitPrimitivePSO::SetWireframePipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_WireframePipelineState);
	directCommandList.SetGraphicsRootSignature(m_ObjectRootSignature);
}

void UnlitPrimitivePSO::UpdateResources(CommandList& directCommandList, const PBRVertexProps& vertexProps) const {
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::VertexCB, vertexProps);
}
