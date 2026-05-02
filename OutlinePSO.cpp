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
	//D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags_VSPS =
	//	D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
	//	D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
	//	D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
	//	D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	// Create outline effect root signature
	//{
	//	CD3DX12_ROOT_PARAMETER1 rootParameters[OutlineRootParameters::NumOutlineRootParameters] {};
	//	rootParameters[OutlineRootParameters::VertexCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
	//	rootParameters[OutlineRootParameters::MaterialCB].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

	//	CD3DX12_STATIC_SAMPLER_DESC anisotropicWrapSampler(0, D3D12_FILTER_ANISOTROPIC);
	//	CD3DX12_STATIC_SAMPLER_DESC trilinearClampSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
	//		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	//	CD3DX12_STATIC_SAMPLER_DESC samplers[] = { anisotropicWrapSampler, trilinearClampSampler };

	//	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	//	rootSignatureDescription.Init_1_1(OutlineRootParameters::NumOutlineRootParameters, rootParameters, 2, samplers, rootSignatureFlags_VSPS);
	//	m_RootSignature = std::make_shared<RootSignature>(device, rootSignatureDescription.Desc_1_1);
	//}

	CD3DX12_RASTERIZER_DESC rasterDesc { CD3DX12_DEFAULT() };
	rasterDesc.CullMode = D3D12_CULL_MODE_BACK;

	// Stencil test to only write outside of outlined objects
	CD3DX12_DEPTH_STENCIL_DESC dsDesc { D3D12_DEFAULT };
	dsDesc.DepthEnable   = FALSE;
	dsDesc.StencilEnable = TRUE;
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
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterDesc;
	} outlinePipelineStateStream;

	outlinePipelineStateStream.pRootSignature = m_ObjectRootSignature->GetD3D12RootSignature().Get();
	outlinePipelineStateStream.InputLayout = VertexInput::Get_POS_NORM_TAN_BIT_UV_InputLayout();
	outlinePipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	outlinePipelineStateStream.VS = AssetImporter::GetCompiledShaderFromFile(L"PBR_VS.cso");
	outlinePipelineStateStream.PS = AssetImporter::GetCompiledShaderFromFile(L"Outline_PS.cso");
	outlinePipelineStateStream.DSDesc = dsDesc;
	outlinePipelineStateStream.DSVFormat = depthFormat;
	outlinePipelineStateStream.RTVFormats = rtvFormats;
	outlinePipelineStateStream.RasterDesc = rasterDesc;

	device.CreatePipelineState(outlinePipelineStateStream, m_D3d12PipelineState);
}

void OutlinePSO::SetPipelineState(CommandList& directCommandList) const {
	directCommandList.SetPipelineState(m_D3d12PipelineState);
	directCommandList.SetGraphicsRootSignature(m_ObjectRootSignature);
}

void OutlinePSO::UpdateResources(CommandList& directCommandList, PBRObjectPSO::VertexProps vertexProps, PBRObjectPSO::LightProps lightProps) {
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::VertexCB, vertexProps);
	directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::LightCB, lightProps);
}
