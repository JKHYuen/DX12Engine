#include "ImageBasedLightingPSO.h"
#include "Helpers.h"
#include "Device.h"
#include "RootSignature.h"
#include "RenderTarget.h"
#include "AssetImporter.h"

#include <DirectXMath.h>
#include <d3dcompiler.h>

ImageBasedLightingPSO::ImageBasedLightingPSO(Device& device, const RenderTarget& renderTarget) {
	// Skybox pipeline state
	struct SkyboxPipelineState {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC           SampleDesc;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            RasterizerDesc;
	} skyboxPipelineStateStream;

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags_VSPS =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	// Skybox Rendering
	Microsoft::WRL::ComPtr<ID3DBlob> skybox_vs;
	Microsoft::WRL::ComPtr<ID3DBlob> skybox_ps;
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Skybox_VS.cso", &skybox_vs));
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Skybox_PS.cso", &skybox_ps));

	D3D12_INPUT_ELEMENT_DESC inputLayout[1] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	CD3DX12_ROOT_PARAMETER1 rootParameters[2];
	rootParameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[1].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc {};
	rootSignatureDesc.Init_1_1(2, rootParameters, 1, &linearClampSampler, rootSignatureFlags_VSPS);
	m_SkyboxRootSignature = std::make_shared<RootSignature>(device, rootSignatureDesc.Desc_1_1);

	CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;

	skyboxPipelineStateStream.pRootSignature = m_SkyboxRootSignature->GetD3D12RootSignature().Get();
	skyboxPipelineStateStream.InputLayout = { inputLayout, 1 };
	skyboxPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	skyboxPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(skybox_vs.Get());
	skyboxPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(skybox_ps.Get());
	skyboxPipelineStateStream.RTVFormats = renderTarget.GetRenderTargetFormats();
	skyboxPipelineStateStream.SampleDesc = renderTarget.GetSampleDesc();
	skyboxPipelineStateStream.RasterizerDesc = rasterizerDesc;

	device.CreatePipelineState(skyboxPipelineStateStream, m_SkyboxPSO);
	

	// Irradiance Convolution (cubemap)
	{
		// Skybox_VS.cso vertex shader is used
		Microsoft::WRL::ComPtr<ID3DBlob> convoluteCubemap_ps;
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/ConvoluteCubeMap_PS.cso", &convoluteCubemap_ps));

		// Same root sig as skybox rendering (one inline matrix, one SRV)
		skyboxPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(convoluteCubemap_ps.Get());
		skyboxPipelineStateStream.SampleDesc = { 1, 0 };

		device.CreatePipelineState(skyboxPipelineStateStream, m_ConvolutionPSO);
	}
	
	// Prefilter (Specular IBL)
	{
		// Skybox_VS.cso vertex shader is  used
		Microsoft::WRL::ComPtr<ID3DBlob> prefilterCubemap_ps;
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PreFilterCubeMap_PS.cso", &prefilterCubemap_ps));

		CD3DX12_ROOT_PARAMETER1 prefilterRootParameters[3];
		prefilterRootParameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		prefilterRootParameters[1].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
		prefilterRootParameters[2].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC prefilterRootSignatureDesc {};
		prefilterRootSignatureDesc.Init_1_1(3, prefilterRootParameters, 1, &linearClampSampler, rootSignatureFlags_VSPS);
		m_PrefilterRootSignature = std::make_shared<RootSignature>(device, prefilterRootSignatureDesc.Desc_1_1);

		// Same pipeline state as irradiance convolution except pixel shader and root signature
		skyboxPipelineStateStream.pRootSignature = m_PrefilterRootSignature->GetD3D12RootSignature().Get();
		skyboxPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(prefilterCubemap_ps.Get());
		skyboxPipelineStateStream.SampleDesc = { 1, 0 };

		device.CreatePipelineState(skyboxPipelineStateStream, m_PrefilterPSO);
	}

	// BRDF LUT Integration
	{
		// BRDF Root Signature
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC BRDF_LUT_RootSignatureDescription {};
		BRDF_LUT_RootSignatureDescription.Init_1_1(0, nullptr, 0, &linearClampSampler, rootSignatureFlags_VSPS);
		m_BRDF_LUT_RootSignature = std::make_shared<RootSignature>(device, BRDF_LUT_RootSignatureDescription.Desc_1_1);

		// BRDF Precompute Pipeline State
		skyboxPipelineStateStream.pRootSignature = m_BRDF_LUT_RootSignature->GetD3D12RootSignature().Get();
		skyboxPipelineStateStream.InputLayout = CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT();
		skyboxPipelineStateStream.VS = AssetImporter::GetCompiledShaderFromFile(L"compiled_shaders/ScreenRender_VS.cso");
		skyboxPipelineStateStream.PS = AssetImporter::GetCompiledShaderFromFile(L"compiled_shaders/IntegrateBRDF_PS.cso");
		skyboxPipelineStateStream.SampleDesc = { 1, 0 };
		// Don't know a simpler way to write this..
		DXGI_FORMAT format[] = { DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN };
		skyboxPipelineStateStream.RTVFormats = CD3DX12_RT_FORMAT_ARRAY(format, 1);

		device.CreatePipelineState(skyboxPipelineStateStream, m_BRDF_LUT_PSO);
	}
}