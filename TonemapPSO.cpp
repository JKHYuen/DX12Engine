#include "TonemapPSO.h"

#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/Device.h"
#include "DX12EngineCore/RenderTarget.h"
#include "DX12EngineCore/RootSignature.h"

#include "AssetImporter.h"
#include "d3d12.h"
#include "d3dcommon.h"
#include "d3dx12_core.h"
#include "d3dx12_default.h"
#include "d3dx12_pipeline_state_stream.h"
#include "d3dx12_root_signature.h"
#include <cstdlib>
#include <memory>

TonemapPSO::TonemapPSO(Device& device, const RenderTarget& renderTarget) {
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags_VSPS =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[1] {};
	rootParameters[0].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC pointClampSampler(
		0, D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &pointClampSampler, rootSignatureFlags_VSPS);
	m_RootSignature = std::make_shared<RootSignature>(device, rootSignatureDescription.Desc_1_1);

	// Note: cull front, triangle created in ScreenRender_VS faces away from camera
	CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;

	struct PostProcessPipelineStateStream {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            Rasterizer;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
	} postProcessPipelineStateStream;

	postProcessPipelineStateStream.pRootSignature = m_RootSignature->GetD3D12RootSignature().Get();
	postProcessPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	postProcessPipelineStateStream.VS = AssetImporter::Get().GetCompiledShaderFromFile(L"ScreenRender_VS.cso");
	postProcessPipelineStateStream.PS = AssetImporter::Get().GetCompiledShaderFromFile(L"Tonemap_PS.cso");

	postProcessPipelineStateStream.Rasterizer = rasterizerDesc;
	postProcessPipelineStateStream.RTVFormats = renderTarget.GetRenderTargetFormats();

	device.CreatePipelineState(postProcessPipelineStateStream, m_PipelineState);
}

void TonemapPSO::SetPipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_PipelineState);
	directCommandList.SetGraphicsRootSignature(m_RootSignature);
	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
