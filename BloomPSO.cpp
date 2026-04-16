#include "BloomPSO.h"
#include "RenderTarget.h"
#include "d3dx12.h"
#include "Device.h"
#include "VertexInput.h"
#include "RootSignature.h"
#include "AssetImporter.h"

BloomPSO::BloomPSO(Device& device, const RenderTarget& renderTarget) {
	// Bloom pipeline state
	struct BloomPipelineState {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC           SampleDesc;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC            BlendDesc;
	} bloomPipelineStateStream;

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

	CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags_VSPS =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_PARAMETER1 rootParameters[BloomRootParameters::NumBloomRootParameters] {};
	{
		rootParameters[BloomRootParameters::BloomCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
		rootParameters[BloomRootParameters::Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
	}

	CD3DX12_STATIC_SAMPLER_DESC samplers[] = { linearClampSampler };

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription {};
	rootSignatureDescription.Init_1_1(BloomRootParameters::NumBloomRootParameters, rootParameters, 1, samplers, rootSignatureFlags_VSPS);
	m_RootSignature = std::make_shared<RootSignature>(device, rootSignatureDescription.Desc_1_1);

	// PSO for Prefilter, Downsample and combine rendering 
	// (Disable blending)
	auto blendDesc = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT());
	bloomPipelineStateStream.pRootSignature = m_RootSignature->GetD3D12RootSignature().Get();
	bloomPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	bloomPipelineStateStream.VS = AssetImporter::GetCompiledShaderFromFile(L"ScreenRender_VS.cso");
	bloomPipelineStateStream.PS = AssetImporter::GetCompiledShaderFromFile(L"Bloom_PS.cso");
	bloomPipelineStateStream.RTVFormats = renderTarget.GetRenderTargetFormats();
	bloomPipelineStateStream.SampleDesc = renderTarget.GetSampleDesc();
	bloomPipelineStateStream.BlendDesc = blendDesc;

	device.CreatePipelineState(bloomPipelineStateStream, m_BloomPSO);

	// PSO for upsampling
	// (Additive blend)
	blendDesc.RenderTarget[AttachmentPoint::Color0].BlendEnable    = TRUE;
	blendDesc.RenderTarget[AttachmentPoint::Color0].DestBlend      = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[AttachmentPoint::Color0].BlendOp        = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[AttachmentPoint::Color0].DestBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[AttachmentPoint::Color0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
	bloomPipelineStateStream.BlendDesc = blendDesc;
	
	device.CreatePipelineState(bloomPipelineStateStream, m_BloomAdditivePSO);
}

void BloomPSO::SetPipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_BloomPSO);
	directCommandList.SetGraphicsRootSignature(m_RootSignature);
}

void BloomPSO::SetAdditivePipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_BloomAdditivePSO);
	directCommandList.SetGraphicsRootSignature(m_RootSignature);
}
