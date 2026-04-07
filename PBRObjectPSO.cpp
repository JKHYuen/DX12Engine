#include "PBRObjectPSO.h"
#include "Helpers.h"
#include "Device.h"
#include "RootSignature.h"
#include "CommandList.h"
#include "VertexTypes.h"
#include "RenderTarget.h"
#include "ShaderResourceView.h"
#include "Logger.h"
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

using namespace DirectX;
using namespace Microsoft::WRL;

PBRObjectPSO::PBRObjectPSO(Device& device, DXGI_SAMPLE_DESC sampleDesc, D3D12_RT_FORMAT_ARRAY rtvFormat, DXGI_FORMAT depthFormat) {
	// Load PBR shaders
	ComPtr<ID3DBlob> vs;
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PBR_VS.cso", &vs));
	ComPtr<ID3DBlob> ps;
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PBR_PS.cso", &ps));

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags_VSPS =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	// Create PBR root signature
	{
		CD3DX12_ROOT_PARAMETER1 rootParameters[PBRRootParameters::NumPBRRootParameters];
		rootParameters[PBRRootParameters::VertexCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[PBRRootParameters::MaterialCB].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

		/// Note: descriptor range sizes are hard coded, should match enum PBRTextures, but static and volatile textures are not
		///		  differentiated in the enum right now
		// Static descriptors
		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0);
		rootParameters[PBRRootParameters::Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		// Volatile descriptors
		CD3DX12_DESCRIPTOR_RANGE1 volatileDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 0U, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
		rootParameters[PBRRootParameters::VolatileTextures].InitAsDescriptorTable(1, &volatileDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC anisotropicSampler(0, D3D12_FILTER_ANISOTROPIC);
		CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		CD3DX12_STATIC_SAMPLER_DESC samplers[] = { anisotropicSampler, linearClampSampler };

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(PBRRootParameters::NumPBRRootParameters, rootParameters, 2, samplers, rootSignatureFlags_VSPS);
		m_RootSignature = std::make_shared<RootSignature>(device, rootSignatureDescription.Desc_1_1);
	}

	struct HDRPipelineStateStream {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DSDesc;
	} hdrPipelineStateStream;

	hdrPipelineStateStream.pRootSignature = m_RootSignature->GetD3D12RootSignature().Get();
	hdrPipelineStateStream.InputLayout = VertexInput::GetInputLayout();
	hdrPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	hdrPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	hdrPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	hdrPipelineStateStream.DSVFormat = depthFormat;
	hdrPipelineStateStream.RTVFormats = rtvFormat;
	hdrPipelineStateStream.SampleDesc = sampleDesc;
	auto dscDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	hdrPipelineStateStream.DSDesc = dscDesc;

	device.CreatePipelineState(hdrPipelineStateStream, m_D3d12PipelineState);
	
	// Create stencil write PSO (currently just for outline effect, only used for PBR objects to be outlined)
	dscDesc.StencilEnable = TRUE;

	dscDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	dscDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_REPLACE;
	dscDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	dscDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	hdrPipelineStateStream.DSDesc = dscDesc;

	device.CreatePipelineState(hdrPipelineStateStream, m_StencilWrite_D3d12PipelineState);
}

void PBRObjectPSO::SetPipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_D3d12PipelineState);
	directCommandList.SetGraphicsRootSignature(m_RootSignature);
}

void PBRObjectPSO::SetStencilWritePipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_StencilWrite_D3d12PipelineState);
	directCommandList.SetGraphicsRootSignature(m_RootSignature);
}

void PBRObjectPSO::UpdateResources(CommandList& directCommandList, const std::vector<std::shared_ptr<Texture>>& pbrTextures, VertexProps vertexProps, MaterialProps materialProps) {
	assert((pbrTextures.size() == sk_NumTextures) && "Incorrect number of PBR textures.");

	directCommandList.SetGraphicsDynamicConstantBuffer(PBRRootParameters::VertexCB, vertexProps);
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRRootParameters::MaterialCB, materialProps);

	// Static Textures
	for(int i = 0; i < PBRTextureIndex::NumPBRTextures; i++) {
		directCommandList.SetShaderResourceView(PBRRootParameters::Textures, i, pbrTextures[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	// Volatile Textures
	for(int i = PBRTextureIndex::NumPBRTextures, j = 0; i < sk_NumTextures; i++, j++) {
		directCommandList.SetShaderResourceView(PBRRootParameters::VolatileTextures, j, pbrTextures[i], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
}
