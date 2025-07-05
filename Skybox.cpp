#include "Skybox.h"
#include <DirectXMath.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <memory>

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
	Microsoft::WRL::ComPtr<ID3D12PipelineState> s_IntegrateBRDF_PSO;

	std::shared_ptr<RootSignature> s_IntegrateBRDFRootSignature;
	std::shared_ptr<RootSignature> s_ConvolutionSignature;
	std::shared_ptr<RootSignature> s_PrefilterRootSignature;

	RenderTarget s_PrecomputedBRDF_RT {};
	//std::shared_ptr<Texture> s_PrecomputedBRDFTexture;

	D3D12_RECT s_DefaultScissorRect = CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX);

	std::unique_ptr<Mesh> s_SkyboxCubeMesh;
	bool s_IsInitialized = false;
	bool s_IsBRDFPrecomputed = false;

	constexpr int sk_DefaultSkyboxIndex         = 0;
	constexpr int sk_CubeFaceResolution         = 2048;
	constexpr int sk_CubeMapMipLevels           = 9;
	constexpr int sk_IrradianceMapResolution    = 32;
	constexpr int sk_FullPrefilterMapResolution = 512;
	constexpr int sk_PrecomputedBRDFResolution  = 512;
}

// TODO: seperate initialization that requires COPY CommandList for less error prone construction
Skybox::Skybox(Device& device, CommandList& copyCommandList, std::wstring hdrTextureName, std::unique_ptr<Mesh> cubeMesh, RenderTarget& hdrRenderTarget) {

	s_SkyboxCubeMesh = std::move(cubeMesh);
	m_HDRPanoTexture = copyCommandList.LoadTextureFromFile(L"assets/" + hdrTextureName + L".hdr", true);

	auto cubemapDesc = m_HDRPanoTexture->GetD3D12ResourceDesc();
	// TODO: make face resolution tweakable
	cubemapDesc.Width = cubemapDesc.Height = sk_CubeFaceResolution;
	cubemapDesc.DepthOrArraySize = 6;
	cubemapDesc.MipLevels = 0;
	
	m_SkyCubemapTexture = std::make_shared<Texture>(device, cubemapDesc);
	m_SkyCubemapTexture->SetName(hdrTextureName + L" Skybox Cubemap");

	copyCommandList.PanoToCubemap(m_SkyCubemapTexture, m_HDRPanoTexture);

	D3D12_SHADER_RESOURCE_VIEW_DESC cubeMapSRVDesc = {};
	cubeMapSRVDesc.Format = cubemapDesc.Format;
	cubeMapSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	cubeMapSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	cubeMapSRVDesc.TextureCube.MipLevels = (UINT)-1;  // Use all mips.

	m_SkyCubemapSRV = std::make_shared<ShaderResourceView>(device, m_SkyCubemapTexture, &cubeMapSRVDesc);

	// Static variable initialization (PSO and Root Signatures)
	if(!s_IsInitialized) {
		s_IsInitialized = true;

		/// Skybox Rendering
		Microsoft::WRL::ComPtr<ID3DBlob> vs;
		Microsoft::WRL::ComPtr<ID3DBlob> ps;
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Skybox_VS.cso", &vs));
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Skybox_PS.cso", &ps));

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

		CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(
			0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription {};
		rootSignatureDescription.Init_1_1(2, rootParameters, 1, &linearClampSampler, rootSignatureFlags_VSPS);
		m_SkyboxRootSignature = std::make_shared<RootSignature>(device, rootSignatureDescription.Desc_1_1);

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

		skyboxPipelineStateStream.pRootSignature = m_SkyboxRootSignature->GetD3D12RootSignature().Get();
		skyboxPipelineStateStream.InputLayout = {inputLayout, 1};
		skyboxPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		skyboxPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		skyboxPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		skyboxPipelineStateStream.RTVFormats = hdrRenderTarget.GetRenderTargetFormats();
		skyboxPipelineStateStream.SampleDesc = hdrRenderTarget.GetSampleDesc();
		skyboxPipelineStateStream.RasterizerDesc = rasterizerDesc;

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {sizeof(SkyboxPipelineState), &skyboxPipelineStateStream};
		ThrowIfFailed(device.GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&s_SkyboxPSO)));


		/// BRDF Integration
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/IntegrateBRDF_PS.cso", &ps));
		// TODO: this is loaded a second time in DemoGame, combine these
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/ScreenRender_VS.cso", &vs));

		// Create render texture for precomputed BRDF
		auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16_FLOAT, sk_PrecomputedBRDFResolution, sk_PrecomputedBRDFResolution,
			1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		auto precomputeBRDFTexture = std::make_shared<Texture>(device, colorDesc, nullptr);
		precomputeBRDFTexture->SetName(L"Integrated BRDF Texture");
		s_PrecomputedBRDF_RT.AttachTexture(AttachmentPoint::Color0, precomputeBRDFTexture);

		// BRDF Root Signature
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC integrateBRDFRootSignatureDescription {};
		integrateBRDFRootSignatureDescription.Init_1_1(0, nullptr, 0, nullptr, rootSignatureFlags_VSPS);
		s_IntegrateBRDFRootSignature = std::make_shared<RootSignature>(device, integrateBRDFRootSignatureDescription.Desc_1_1);

		// BRDF Precompute Pipeline State
		skyboxPipelineStateStream.pRootSignature = s_IntegrateBRDFRootSignature->GetD3D12RootSignature().Get();
		skyboxPipelineStateStream.InputLayout = CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT();
		skyboxPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		skyboxPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		skyboxPipelineStateStream.RTVFormats = s_PrecomputedBRDF_RT.GetRenderTargetFormats();
		skyboxPipelineStateStream.SampleDesc = s_PrecomputedBRDF_RT.GetSampleDesc();
		pipelineStateStreamDesc = {sizeof(SkyboxPipelineState), &skyboxPipelineStateStream};
		ThrowIfFailed(device.GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&s_IntegrateBRDF_PSO)));

		/// BRDF Integration

	}
}

void Skybox::Render(CommandList& directCommandList, const Camera& camera) {
	auto viewMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(camera.get_Rotation()));
	auto projMatrix = camera.get_ProjectionMatrix();
	auto viewProjMatrix = viewMatrix * projMatrix;

	directCommandList.SetPipelineState(s_SkyboxPSO);
	directCommandList.SetGraphicsRootSignature(m_SkyboxRootSignature);
					 
	directCommandList.SetGraphics32BitConstants(0, viewProjMatrix);
	directCommandList.SetShaderResourceView(1, 0, m_SkyCubemapSRV, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	s_SkyboxCubeMesh->Draw(directCommandList);
}

// TODO: make sure convolution and prefilter steps are only computed once per skybox
void Skybox::Precompute(CommandList& directCommandList, ComputeMode mode) {
	switch(mode) {
	case ComputeMode::kIntegrateBRDFRender:
		if(s_IsBRDFPrecomputed) return;
		s_IsBRDFPrecomputed = true;
		directCommandList.SetRenderTarget(s_PrecomputedBRDF_RT);
		directCommandList.SetViewport(s_PrecomputedBRDF_RT.GetViewport());
		directCommandList.SetScissorRect(s_DefaultScissorRect);
		directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		directCommandList.SetPipelineState(s_IntegrateBRDF_PSO);
		directCommandList.SetGraphicsRootSignature(s_IntegrateBRDFRootSignature);
		directCommandList.Draw(3);
		break;
	case ComputeMode::kConvolutionRender:

		directCommandList.SetPipelineState(s_ConvolutionPSO);
		directCommandList.SetGraphicsRootSignature(s_ConvolutionSignature);
		break;
	case ComputeMode::kPrefilterRender:
		directCommandList.SetPipelineState(s_PrefilterPSO);
		directCommandList.SetGraphicsRootSignature(s_PrefilterRootSignature);
		break;
	default:
		assert(false && "Invalid ComputeMode.");
	}
}