#include "PBRObjectPSO.h"

#include "DX12EngineCore/Device.h"
#include "DX12EngineCore/RootSignature.h"
#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/VertexInput.h"
#include "DX12EngineCore/RenderTarget.h"
#include "DX12EngineCore/ShaderResourceView.h"

#include "Helpers.h"
#include "AssetImporter.h"
#include "Logger.h"

#include <d3dx12.h>
#include <wrl/client.h>

using namespace DirectX;
using namespace Microsoft::WRL;

PBRObjectPSO::PBRObjectPSO(Device& device, DXGI_SAMPLE_DESC sampleDesc, D3D12_RT_FORMAT_ARRAY rtvFormat, DXGI_FORMAT depthStencilFormat) {
	// Create PBR root signature
	{
		CD3DX12_ROOT_PARAMETER1 rootParameters[PBRRootParameters::NumPBRRootParameters] {};
		rootParameters[PBRRootParameters::VertexCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[PBRRootParameters::MaterialCB].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[PBRRootParameters::LightCB].InitAsConstantBufferView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[PBRRootParameters::TessellationCB].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_HULL);

		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, TextureIndex::NumTextures, 0);
		/// used in vertex and pixel shaders
		rootParameters[PBRRootParameters::Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_ALL);

		CD3DX12_STATIC_SAMPLER_DESC anisotropicWrapSampler(0, D3D12_FILTER_ANISOTROPIC);
		CD3DX12_STATIC_SAMPLER_DESC trilinearClampSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		CD3DX12_STATIC_SAMPLER_DESC trilinearBorderSampler(2, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_TEXTURE_ADDRESS_MODE_BORDER, 0.0f, 0, D3D12_COMPARISON_FUNC_LESS);
		CD3DX12_STATIC_SAMPLER_DESC samplers[] = { anisotropicWrapSampler, trilinearClampSampler, trilinearBorderSampler };

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(PBRRootParameters::NumPBRRootParameters, rootParameters, 3, samplers, 
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);
		m_RootSignature = std::make_shared<RootSignature>(device, rootSignatureDescription.Desc_1_1);
	}

	struct HDRPipelineStateStream {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_HS HS;
		CD3DX12_PIPELINE_STATE_STREAM_DS DS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencilDesc;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterDesc;
	} hdrPipelineStateStream;

	hdrPipelineStateStream.pRootSignature = m_RootSignature->GetD3D12RootSignature().Get();
	hdrPipelineStateStream.InputLayout = VertexInput::Get_POS_NORM_TAN_BIT_UV_InputLayout();
	hdrPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
	hdrPipelineStateStream.VS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_VS.cso");
	hdrPipelineStateStream.HS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_HS.cso");
	hdrPipelineStateStream.DS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_DS.cso");
	hdrPipelineStateStream.PS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_PS.cso");
	hdrPipelineStateStream.DSVFormat = depthStencilFormat;
	hdrPipelineStateStream.RTVFormats = rtvFormat;
	hdrPipelineStateStream.SampleDesc = sampleDesc;
	auto depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	hdrPipelineStateStream.DepthStencilDesc = depthStencilDesc;
	CD3DX12_RASTERIZER_DESC rasterDesc { D3D12_DEFAULT };
	hdrPipelineStateStream.RasterDesc = rasterDesc;

	device.CreatePipelineState(hdrPipelineStateStream, m_PipelineState);

	rasterDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
	hdrPipelineStateStream.RasterDesc = rasterDesc;
	device.CreatePipelineState(hdrPipelineStateStream, m_WireFramePipelineState);
}

void PBRObjectPSO::SetPipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_PipelineState);
	directCommandList.SetGraphicsRootSignature(m_RootSignature);
	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
}

void PBRObjectPSO::SetWireframePipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_WireFramePipelineState);
	directCommandList.SetGraphicsRootSignature(m_RootSignature);
	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
}

void PBRObjectPSO::UpdateResources(CommandList& directCommandList, const std::vector<std::shared_ptr<Texture>>& pbrTextures, const PBRVertexProps& vertexProps, const PBRTessellationProps& tessProps, const PBRMaterialProps& materialProps, const PBRLightProps& lightProps) {
	assert((pbrTextures.size() == TextureIndex::NumTextures) && "Incorrect number of PBR textures.");

	// Note: may have performance increase if these are only updated when needed
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRRootParameters::VertexCB, vertexProps);
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRRootParameters::TessellationCB, tessProps);
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRRootParameters::MaterialCB, materialProps);
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRRootParameters::LightCB, lightProps);

	for(int i = 0; i < TextureIndex::NumTextures; i++) {
		directCommandList.SetShaderResourceView(PBRRootParameters::Textures, i, pbrTextures[i], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
	}
}
