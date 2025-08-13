#include "Skybox.h"
#include <DirectXMath.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <memory>
#include <array>

#include "Device.h"
#include "RootSignature.h"
#include "RenderTarget.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "ShaderResourceView.h"
#include "Mesh.h"
#include "Camera.h"
#include "Helpers.h"

using namespace DirectX;

namespace {
	Microsoft::WRL::ComPtr<ID3D12PipelineState> s_SkyboxPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> s_ConvolutionPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> s_PrefilterPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> s_BRDF_LUT_PSO;

	std::shared_ptr<RootSignature> s_SkyboxRootSignature; // used by skybox render and irradiance convolution
	std::shared_ptr<RootSignature> s_BRDF_LUT_RootSignature;
	std::shared_ptr<RootSignature> s_PrefilterRootSignature;

	RenderTarget s_BRDF_LUT_RT {};

	D3D12_RECT s_DefaultScissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

	std::unique_ptr<Mesh> s_SkyboxCubeMesh;
	bool s_IsInitialized = false;

	/// TEMP
	bool s_IsBRDFPrecomputed = false;
	bool s_IsIrradiancePrecomputed = false;
	bool s_IsPrefilterPrecomputed = false;
	/// 

	constexpr int sk_DefaultSkyboxIndex         = 0;
	constexpr int sk_CubeFaceResolution         = 2048;
	constexpr int sk_CubemapMipLevels           = 9; // must match MAX_REFLECTION_LOD in PBR pixel shader
	constexpr int sk_IrradianceMapResolution    = 32;
	constexpr int sk_FullPrefilterMapResolution = 512;
	constexpr int sk_PrecomputedBRDFResolution  = 512;

	constexpr XMFLOAT3 float3_000  {0.0f,   0.0f,  0.0f};
	constexpr XMFLOAT3 float3_100  {1.0f,   0.0f,  0.0f};
	constexpr XMFLOAT3 float3_010  {0.0f,   1.0f,  0.0f};
	constexpr XMFLOAT3 float3_n100 {-1.0f,  0.0f,  0.0f};
	constexpr XMFLOAT3 float3_00n1 {0.0f,   0.0f, -1.0f};
	constexpr XMFLOAT3 float3_0n10 {0.0f,  -1.0f,  0.0f};
	constexpr XMFLOAT3 float3_001  {0.0f,   0.0f,  1.0f};

	const std::array<XMMATRIX, 6> s_CubeMapCaptureViewMats = {
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_100),  XMLoadFloat3(&float3_010)),
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_n100), XMLoadFloat3(&float3_010)),
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_010),  XMLoadFloat3(&float3_00n1)),
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_0n10),	XMLoadFloat3(&float3_001)),
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_001),	XMLoadFloat3(&float3_010)),
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_00n1), XMLoadFloat3(&float3_010)),
	};

}

// TODO: seperate initialization that requires COPY CommandList for less error prone construction
Skybox::Skybox(Device& device, CommandList& copyCommandList, std::wstring hdrTextureName, std::unique_ptr<Mesh> cubeMesh, RenderTarget& hdrRenderTarget) {

	s_SkyboxCubeMesh = std::move(cubeMesh);
	m_HDRPanoTexture = copyCommandList.LoadTextureFromFile(L"assets/cubemaps/" + hdrTextureName + L".hdr", true);

	// Convert hdr panoramic texture to cubemap
	auto cubemapDesc = m_HDRPanoTexture->GetD3D12ResourceDesc();
	cubemapDesc.Width = cubemapDesc.Height = sk_CubeFaceResolution;
	cubemapDesc.DepthOrArraySize = 6;
	cubemapDesc.MipLevels = sk_CubemapMipLevels;
	
	m_SkyCubemapTexture = std::make_shared<Texture>(device, cubemapDesc, nullptr, false);
	m_SkyCubemapTexture->SetName(hdrTextureName + L"Skybox Cubemap");

	// PanoToCubemapCompute function will switch to compute queue when called by a COPY command list
	copyCommandList.PanoToCubemapCompute(m_SkyCubemapTexture, m_HDRPanoTexture);

	// Create cubemap SRV 
	D3D12_SHADER_RESOURCE_VIEW_DESC cubeMapSRVDesc = {};
	cubeMapSRVDesc.Format = cubemapDesc.Format;
	cubeMapSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	cubeMapSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	cubeMapSRVDesc.TextureCube.MipLevels = -1;
	m_SkyCubemapSRV = std::make_shared<ShaderResourceView>(device, m_SkyCubemapTexture, &cubeMapSRVDesc);

	// Static variable initialization (PSO and Root Signatures)
	if(!s_IsInitialized) {
		s_IsInitialized = true;

		///
		/// Skybox Rendering
		///
		Microsoft::WRL::ComPtr<ID3DBlob> skybox_vs;
		Microsoft::WRL::ComPtr<ID3DBlob> skybox_ps;
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Skybox_VS.cso", &skybox_vs));
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Skybox_PS.cso", &skybox_ps));

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags_VSPS =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		D3D12_INPUT_ELEMENT_DESC inputLayout[1] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[1].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		//CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc {};
		rootSignatureDesc.Init_1_1(2, rootParameters, 1, &linearClampSampler, rootSignatureFlags_VSPS);
		s_SkyboxRootSignature = std::make_shared<RootSignature>(device, rootSignatureDesc.Desc_1_1);

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

		CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
		rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;

		skyboxPipelineStateStream.pRootSignature = s_SkyboxRootSignature->GetD3D12RootSignature().Get();
		skyboxPipelineStateStream.InputLayout = {inputLayout, 1};
		skyboxPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		skyboxPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(skybox_vs.Get());
		skyboxPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(skybox_ps.Get());
		skyboxPipelineStateStream.RTVFormats = hdrRenderTarget.GetRenderTargetFormats();
		skyboxPipelineStateStream.SampleDesc = hdrRenderTarget.GetSampleDesc();
		skyboxPipelineStateStream.RasterizerDesc = rasterizerDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {sizeof(SkyboxPipelineState), &skyboxPipelineStateStream};
		ThrowIfFailed(device.GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&s_SkyboxPSO)));

		///
		/// Irradiance Convolution (cubemap)
		///
		// Skybox_VS.cso vertex shader used
		Microsoft::WRL::ComPtr<ID3DBlob> convoluteCubemap_ps;
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/ConvoluteCubeMap_PS.cso", &convoluteCubemap_ps));

		// Same root sig as skybox rendering (one inline matrix, one SRV)
		skyboxPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(convoluteCubemap_ps.Get());
		skyboxPipelineStateStream.SampleDesc = {1, 0};

		pipelineStateStreamDesc = {sizeof(SkyboxPipelineState), &skyboxPipelineStateStream};
		ThrowIfFailed(device.GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&s_ConvolutionPSO)));

		// Create cubemap render texture for irradiance convolution
		auto convolutionCubemapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16B16A16_FLOAT, sk_IrradianceMapResolution, sk_IrradianceMapResolution,
			6, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		auto irradianceConvolutionCubemap = std::make_shared<Texture>(device, convolutionCubemapDesc, nullptr, false);
		irradianceConvolutionCubemap->SetName(L"Skybox Irradiance Convolution Cubemap");

		m_IrradianceConvolutionCubemap_RT.AttachTexture(AttachmentPoint::Color0, irradianceConvolutionCubemap);

		// Create SRV for shader usage
		cubeMapSRVDesc.Format = m_IrradianceConvolutionCubemap_RT.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
		m_IrradianceCubemapSRV = std::make_shared<ShaderResourceView>(device, irradianceConvolutionCubemap, &cubeMapSRVDesc);

		///
		/// Prefilter (Specular IBL)
		///
		// Skybox_VS.cso vertex shader used
		Microsoft::WRL::ComPtr<ID3DBlob> prefilterCubemap_ps;
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PreFilterCubeMap_PS.cso", &prefilterCubemap_ps));

		CD3DX12_ROOT_PARAMETER1 prefilterRootParameters[3];
		prefilterRootParameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		prefilterRootParameters[1].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);
		prefilterRootParameters[2].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC prefilterRootSignatureDesc {};
		prefilterRootSignatureDesc.Init_1_1(3, prefilterRootParameters, 1, &linearClampSampler, rootSignatureFlags_VSPS);
		s_PrefilterRootSignature = std::make_shared<RootSignature>(device, prefilterRootSignatureDesc.Desc_1_1);

		// Same pipeline state as irradiance convolution except pixel shader and root signature
		skyboxPipelineStateStream.pRootSignature = s_PrefilterRootSignature->GetD3D12RootSignature().Get();
		skyboxPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(prefilterCubemap_ps.Get());

		pipelineStateStreamDesc = {sizeof(SkyboxPipelineState), &skyboxPipelineStateStream};
		ThrowIfFailed(device.GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&s_PrefilterPSO)));

		// Create cubemap render texture for irradiance convolution
		auto prefilterCubemapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16B16A16_FLOAT, sk_FullPrefilterMapResolution, sk_FullPrefilterMapResolution,
			//DXGI_FORMAT_R32G32B32A32_FLOAT, sk_FullPrefilterMapResolution, sk_FullPrefilterMapResolution,
			6, sk_CubemapMipLevels, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		auto prefilterCubemap = std::make_shared<Texture>(device, prefilterCubemapDesc, nullptr, false);
		prefilterCubemap->SetName(L"Skybox Prefilter Cubemap");

		m_PrefilterCubemap_RT.AttachTexture(AttachmentPoint::Color0, prefilterCubemap);

		// Create SRV for shader usage
		cubeMapSRVDesc.Format = m_PrefilterCubemap_RT.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
		m_PrefilterCubemapSRV = std::make_shared<ShaderResourceView>(device, prefilterCubemap, &cubeMapSRVDesc);

		///
		/// BRDF LUT Integration
		///
		Microsoft::WRL::ComPtr<ID3DBlob> screenRender_vs;
		Microsoft::WRL::ComPtr<ID3DBlob> integrateBRDF_ps;
		// TODO: this is loaded a second time in DemoGame, combine these
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/ScreenRender_VS.cso", &screenRender_vs));
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/IntegrateBRDF_PS.cso", &integrateBRDF_ps));

		// Create render texture for precomputed BRDF
		auto lutTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16_FLOAT, sk_PrecomputedBRDFResolution, sk_PrecomputedBRDFResolution,
			1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);
		auto BRDF_LUT_Texture = std::make_shared<Texture>(device, lutTextureDesc);

		BRDF_LUT_Texture->SetName(L"Integrated BRDF Texture");
		s_BRDF_LUT_RT.AttachTexture(AttachmentPoint::Color0, BRDF_LUT_Texture);

		// BRDF Root Signature
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC BRDF_LUT_RootSignatureDescription {};
		BRDF_LUT_RootSignatureDescription.Init_1_1(0, nullptr, 0, nullptr, rootSignatureFlags_VSPS);
		s_BRDF_LUT_RootSignature = std::make_shared<RootSignature>(device, BRDF_LUT_RootSignatureDescription.Desc_1_1);

		// BRDF Precompute Pipeline State
		skyboxPipelineStateStream.pRootSignature = s_BRDF_LUT_RootSignature->GetD3D12RootSignature().Get();
		skyboxPipelineStateStream.InputLayout = CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT();
		skyboxPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(integrateBRDF_ps.Get());
		skyboxPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(screenRender_vs.Get());
		skyboxPipelineStateStream.RTVFormats = s_BRDF_LUT_RT.GetRenderTargetFormats();
		skyboxPipelineStateStream.SampleDesc = s_BRDF_LUT_RT.GetSampleDesc();
		pipelineStateStreamDesc = {sizeof(SkyboxPipelineState), &skyboxPipelineStateStream};
		ThrowIfFailed(device.GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&s_BRDF_LUT_PSO)));

		D3D12_SHADER_RESOURCE_VIEW_DESC lutSRVDesc = {};
		lutSRVDesc.Format = lutTextureDesc.Format;
		lutSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		lutSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		lutSRVDesc.Texture2D.MipLevels = -1;
		m_BRDF_LUT_SRV = std::make_shared<ShaderResourceView>(device, BRDF_LUT_Texture, &lutSRVDesc);
	}
}

void Skybox::Render(CommandList& directCommandList, const Camera& camera) {
	auto viewMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(camera.get_Rotation()));
	auto projMatrix = camera.get_ProjectionMatrix();
	auto viewProjMatrix = viewMatrix * projMatrix;

	directCommandList.SetPipelineState(s_SkyboxPSO);
	directCommandList.SetGraphicsRootSignature(s_SkyboxRootSignature);
					 
	directCommandList.SetGraphics32BitConstants(0, viewProjMatrix);
	directCommandList.SetShaderResourceView(1, 0, m_SkyCubemapSRV, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	s_SkyboxCubeMesh->Draw(directCommandList);
}

// TODO: remove switch maybe
void Skybox::Precompute(CommandList& directCommandList, const Camera& camera, ComputeMode mode) {
	XMMATRIX cubemapProjectionMat = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), 1.0f, 0.1f, 10.0f);
	switch(mode) {
	case ComputeMode::kIntegrateBRDFRender:
		/// TEMP
		if(s_IsBRDFPrecomputed) return;
		s_IsBRDFPrecomputed = true;
		directCommandList.SetRenderTarget(s_BRDF_LUT_RT);
		directCommandList.SetViewport(s_BRDF_LUT_RT.GetViewport());
		directCommandList.SetScissorRect(s_DefaultScissorRect);
		directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		directCommandList.SetPipelineState(s_BRDF_LUT_PSO);
		directCommandList.SetGraphicsRootSignature(s_BRDF_LUT_RootSignature);
		directCommandList.Draw(3);
		break;

	case ComputeMode::kConvolutionRender:
		/// TEMP
		if(s_IsIrradiancePrecomputed) return;
		s_IsIrradiancePrecomputed = true;

		directCommandList.SetViewport(m_IrradianceConvolutionCubemap_RT.GetViewport());
		directCommandList.SetScissorRect(s_DefaultScissorRect);
		directCommandList.SetPipelineState(s_ConvolutionPSO);
		directCommandList.SetGraphicsRootSignature(s_SkyboxRootSignature);
		directCommandList.SetShaderResourceView(1, 0, m_SkyCubemapSRV, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		for(int i = 0; i < 6; i++) {
			auto viewMatrix = s_CubeMapCaptureViewMats[i];
			auto viewProjMatrix = viewMatrix * cubemapProjectionMat;

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
			rtvDesc.Format = m_IrradianceConvolutionCubemap_RT.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.FirstArraySlice = i;
			rtvDesc.Texture2DArray.ArraySize = 1;
			m_IrradianceConvolutionCubemap_RT.GetTexture(AttachmentPoint::Color0)->CreateRenderTargetView(rtvDesc);
			directCommandList.SetRenderTarget(m_IrradianceConvolutionCubemap_RT);

			directCommandList.SetGraphics32BitConstants(0, viewProjMatrix);
			s_SkyboxCubeMesh->Draw(directCommandList);
		}
		break;

	case ComputeMode::kPrefilterRender:
		/// TEMP
		if(s_IsPrefilterPrecomputed) return;
		s_IsPrefilterPrecomputed = true;

		directCommandList.SetScissorRect(s_DefaultScissorRect);
		directCommandList.SetPipelineState(s_PrefilterPSO);
		directCommandList.SetGraphicsRootSignature(s_PrefilterRootSignature);
		directCommandList.SetShaderResourceView(1, 0, m_SkyCubemapSRV, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		// Capture 6 cubemap directions with mips for prefiltered map
		for(int mipSlice = 0; mipSlice < sk_CubemapMipLevels; mipSlice++) {
			double currMipScale = std::pow(0.5, mipSlice);
			for(int i = 0; i < 6; i++) {
				auto viewMatrix = s_CubeMapCaptureViewMats[i];
				auto viewProjMatrix = viewMatrix * cubemapProjectionMat;

				D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
				rtvDesc.Format = m_PrefilterCubemap_RT.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtvDesc.Texture2DArray.MipSlice = mipSlice;
				rtvDesc.Texture2DArray.FirstArraySlice = i;
				rtvDesc.Texture2DArray.ArraySize = 1;
				m_PrefilterCubemap_RT.GetTexture(AttachmentPoint::Color0)->CreateRenderTargetView(rtvDesc);
				directCommandList.SetRenderTarget(m_PrefilterCubemap_RT);

				directCommandList.SetViewport(m_PrefilterCubemap_RT.GetViewport({(float)currMipScale, (float)currMipScale}));

				directCommandList.SetGraphics32BitConstants(0, viewProjMatrix);
				float roughness = (float)mipSlice / (float)(sk_CubemapMipLevels - 1);
				directCommandList.SetGraphics32BitConstants(2, roughness);

				s_SkyboxCubeMesh->Draw(directCommandList);
			}
		}
		break;

	default:
		assert(false && "Invalid ComputeMode.");
	}
}

