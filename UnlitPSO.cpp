#include "UnlitPSO.h"

#include "DX12EngineCore/Commandlist.h"
#include "DX12EngineCore/Device.h"
#include "DX12EngineCore/RootSignature.h"
#include "DX12EngineCore/VertexInput.h"

#include "AssetImporter.h"
#include "d3d12.h"
#include "d3dcommon.h"
#include "d3dx12_core.h"
#include "d3dx12_default.h"
#include "d3dx12_pipeline_state_stream.h"
#include "dxgiformat.h"
#include "PBRObjectPSO.h"
#include "RenderConstants.h"

#include <memory>
#include <wrl/client.h>
#include <unordered_map>

using namespace RenderEnums;
using namespace DirectX;
using namespace Microsoft::WRL;

namespace {
	std::unordered_map<RenderFlags, ComPtr<ID3D12PipelineState>> s_PSOMap {};
}

UnlitPSO::UnlitPSO(Device& device, DXGI_SAMPLE_DESC sampleDesc, D3D12_RT_FORMAT_ARRAY rtvFormats, std::shared_ptr<RootSignature> objectRootSignature, DXGI_FORMAT depthStencilFormat)
	: m_RootSignature(objectRootSignature)
{
	struct UnlitPipelineStateStream {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_HS HS;
		CD3DX12_PIPELINE_STATE_STREAM_DS DS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencilDesc;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterDesc;
	} pipelineStateStream;

	pipelineStateStream.pRootSignature = m_RootSignature->GetD3D12RootSignature().Get();
	pipelineStateStream.InputLayout = VertexInput::Get_POS_NORM_TAN_BIT_UV_InputLayout();
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	pipelineStateStream.VS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_VS.cso");
	pipelineStateStream.HS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_HS.cso");
	pipelineStateStream.DS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_DS.cso");
	pipelineStateStream.PS = AssetImporter::Get().GetCompiledShaderFromFile(L"Unlit_PS.cso");
	pipelineStateStream.RTVFormats = rtvFormats;
	pipelineStateStream.DSVFormat = depthStencilFormat;
	pipelineStateStream.SampleDesc = sampleDesc;
	auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC { D3D12_DEFAULT };
	pipelineStateStream.DepthStencilDesc = depthStencilDesc;
	CD3DX12_RASTERIZER_DESC rasterDesc { D3D12_DEFAULT };
	pipelineStateStream.RasterDesc = rasterDesc;

	RenderFlags flags {};
	ComPtr<ID3D12PipelineState> pso {};

	/// Back Cull, depth enabled
	flags = RenderFlags_None;
	device.CreatePipelineState(pipelineStateStream, s_PSOMap[flags]);
	
	/// None Cull, depth disabled
	// For outline effect 
	// NOTE: disable MSAA because outline is rendered in post processing step
	flags = RenderFlags_CullModeNone | RenderFlags_DepthDisable;
	rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
	pipelineStateStream.RasterDesc = rasterDesc;
	depthStencilDesc.DepthEnable = FALSE;
	pipelineStateStream.DepthStencilDesc = depthStencilDesc;
	pipelineStateStream.SampleDesc = {1, 0};
	device.CreatePipelineState(pipelineStateStream, s_PSOMap[flags]);
}

void UnlitPSO::SetPipelineState(CommandList& directCommandList, RenderFlags renderFlags) const {
	assert(s_PSOMap.find(renderFlags) != s_PSOMap.end() && "Invalid unlit render flags.");

	directCommandList.SetPipelineState(s_PSOMap[renderFlags]);
	directCommandList.SetGraphicsRootSignature(m_RootSignature);
	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
}

void UnlitPSO::UpdateResources(CommandList& directCommandList, const PBRVertexProps& vertexProps, const PBRTessellationProps& tessProps) const {
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::VertexCB, vertexProps);
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::TessellationCB, tessProps);
}
