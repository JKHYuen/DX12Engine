#include "Skybox.h"
#include <DirectXMath.h>
#include <d3dx12.h>
#include <memory>

#include "DX12EngineCore/Device.h"
#include "DX12EngineCore/RenderTarget.h"
#include "DX12EngineCore/Texture.h"
#include "DX12EngineCore/CommandQueue.h"
#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/ShaderResourceView.h"
#include "DX12EngineCore/Mesh.h"

#include "Camera.h"
#include "ImageBasedLightingPSO.h"
#include "Colors.h"
#include "AssetImporter.h"
#include "Logger.h"

using namespace DirectX;

namespace {
	constexpr int sk_CubeFaceResolution         = 2048;
	constexpr int sk_CubemapMipLevels           = 9;   // must match MAX_REFLECTION_LOD in PBR pixel shader
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

	const XMMATRIX s_CubeMapCaptureViewMats[] = {
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_100),  XMLoadFloat3(&float3_010)),
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_n100), XMLoadFloat3(&float3_010)),
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_010),  XMLoadFloat3(&float3_00n1)),
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_0n10),	XMLoadFloat3(&float3_001)),
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_001),	XMLoadFloat3(&float3_010)),
		XMMatrixLookAtLH(XMLoadFloat3(&float3_000), XMLoadFloat3(&float3_00n1), XMLoadFloat3(&float3_010)),
	};
}

/// TODO: TEST FUNCTION, UNUSED
void Skybox::SetCubemap(CommandList& copyCommandList, CommandList& computeCommandList, const std::wstring& hdrTextureName) {
	m_HDRPanoTexture = copyCommandList.LoadTextureFromFile(AssetImporter::Get().GetAssetPath() / L"cubemaps" / hdrTextureName, true);
	computeCommandList.PanoToCubemapCompute(m_SkyCubemapTexture, m_HDRPanoTexture);
}

Skybox::Skybox(Device& device, CommandList& copyCommandList, CommandList& computeCommandList, const SkyboxParams& params)
	: m_IBL_PSO(params.iblPSO)
	, m_SkyboxTextureName(params.hdrTextureName) {

	m_IrradianceConvolutionCubemap_RT = std::make_unique<RenderTarget>();
	m_PrefilterCubemap_RT = std::make_unique<RenderTarget>();
	m_BRDF_LUT_RT = std::make_unique<RenderTarget>();

	DXGI_FORMAT cubemapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	m_SkyboxCubeMesh = copyCommandList.GetCubePrimitive();
	m_HDRPanoTexture = copyCommandList.LoadTextureFromFile(AssetImporter::Get().GetAssetPath() / L"cubemaps" / m_SkyboxTextureName, true);

	// Convert hdr panoramic texture to cubemap
	auto skyboxCubemapDesc = m_HDRPanoTexture->GetD3D12ResourceDesc();
	skyboxCubemapDesc.Width = skyboxCubemapDesc.Height = sk_CubeFaceResolution;
	skyboxCubemapDesc.DepthOrArraySize = 6;
	skyboxCubemapDesc.MipLevels = sk_CubemapMipLevels;
	
	m_SkyCubemapTexture = std::make_shared<Texture>(device, skyboxCubemapDesc, nullptr, false);
	m_SkyCubemapTexture->SetName(m_SkyboxTextureName + L" Skybox Cubemap");

	// PanoToCubemapCompute function will switch to compute queue when called by a COPY command list
	computeCommandList.PanoToCubemapCompute(m_SkyCubemapTexture, m_HDRPanoTexture);

	// Create cubemap SRV 
	D3D12_SHADER_RESOURCE_VIEW_DESC cubeMapSRVDesc = {};
	cubeMapSRVDesc.Format = skyboxCubemapDesc.Format;
	cubeMapSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	cubeMapSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	cubeMapSRVDesc.TextureCube.MipLevels = -1;

	m_SkyCubemapTexture->CreateShaderResourceView(cubeMapSRVDesc);

	/// Create Render Textures
	// Create cubemap render texture for irradiance convolution
	{
		auto irradianceCubemapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			cubemapFormat, sk_IrradianceMapResolution, sk_IrradianceMapResolution,
			6, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		auto irradianceConvolutionCubemap = std::make_shared<Texture>(device, irradianceCubemapDesc, nullptr, false);
		irradianceConvolutionCubemap->SetName(L"Skybox Irradiance Convolution Cubemap - " + m_SkyboxTextureName);

		m_IrradianceConvolutionCubemap_RT->AttachTexture(AttachmentPoint::Color0, irradianceConvolutionCubemap);

		cubeMapSRVDesc.Format = m_IrradianceConvolutionCubemap_RT->GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
		irradianceConvolutionCubemap->CreateShaderResourceView(cubeMapSRVDesc);
	}

	// Create cubemap render texture for Prefilter map (specular)
	{
		auto prefilterCubemapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			cubemapFormat, sk_FullPrefilterMapResolution, sk_FullPrefilterMapResolution,
			6, sk_CubemapMipLevels, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		auto prefilterCubemap = std::make_shared<Texture>(device, prefilterCubemapDesc, nullptr, false);
		prefilterCubemap->SetName(L"Skybox Prefilter Cubemap - " + m_SkyboxTextureName);

		m_PrefilterCubemap_RT->AttachTexture(AttachmentPoint::Color0, prefilterCubemap);

		cubeMapSRVDesc.Format = m_PrefilterCubemap_RT->GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
		prefilterCubemap->CreateShaderResourceView(cubeMapSRVDesc);
	}

	// Create render texture for precomputed BRDF
	{
		auto lutTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16_FLOAT, sk_PrecomputedBRDFResolution, sk_PrecomputedBRDFResolution,
			1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);
		// NOTE: b_CreatedefaultView is true to make render target view
		auto BRDF_LUT_Texture = std::make_shared<Texture>(device, lutTextureDesc);
		BRDF_LUT_Texture->SetName(L"Integrated BRDF Texture");

		m_BRDF_LUT_RT->AttachTexture(AttachmentPoint::Color0, BRDF_LUT_Texture);

		D3D12_SHADER_RESOURCE_VIEW_DESC lutSRVDesc = {};
		lutSRVDesc.Format = DXGI_FORMAT_R16G16_FLOAT; // DO NOT CHANGE, must match format in ImageBasedLightingPSO
		lutSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		lutSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		lutSRVDesc.Texture2D.MipLevels = -1;
		m_BRDF_LUT_RT->GetTexture(AttachmentPoint::Color0)->CreateShaderResourceView(lutSRVDesc);
	}
}

void Skybox::Render(CommandList& directCommandList, const Camera& camera) {
	auto viewMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(camera.Get_Rotation()));
	auto projMatrix = camera.Get_ProjectionMatrix();
	auto viewProjMatrix = viewMatrix * projMatrix;

	m_IBL_PSO->SetPipelineState(directCommandList, ImageBasedLightingPSO::Skybox);
					 
	directCommandList.SetGraphics32BitConstants(0, viewProjMatrix);
	directCommandList.SetShaderResourceView(1, 0, m_SkyCubemapTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	m_SkyboxCubeMesh->Draw(directCommandList);
}

void Skybox::ComputeIBLMaps(CommandList& directCommandList) {
	XMMATRIX cubemapProjectionMat = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), 1.0f, 0.1f, 10.0f);

	// BRDF Integration Map
	{
		/// TODO: This could be only calculated once per runtime, or saved to disk
		m_IBL_PSO->SetPipelineState(directCommandList, ImageBasedLightingPSO::BRDF_LUT);

		directCommandList.ClearTexture(m_BRDF_LUT_RT->GetTexture(AttachmentPoint::Color0), Colors::DebugMagenta);
		directCommandList.SetRenderTarget(*m_BRDF_LUT_RT);
		directCommandList.SetViewport(m_BRDF_LUT_RT->GetViewport());

		directCommandList.Draw(3);
	}

	// Irradiance Convolution Map
	{
		m_IBL_PSO->SetPipelineState(directCommandList, ImageBasedLightingPSO::Convolution);

		directCommandList.SetShaderResourceView(1, 0, m_SkyCubemapTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		for(int i = 0; i < 6; i++) {
			auto viewMatrix = s_CubeMapCaptureViewMats[i];
			auto viewProjMatrix = viewMatrix * cubemapProjectionMat;

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
			rtvDesc.Format = m_IrradianceConvolutionCubemap_RT->GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.FirstArraySlice = i;
			rtvDesc.Texture2DArray.ArraySize = 1;
			m_IrradianceConvolutionCubemap_RT->GetTexture(AttachmentPoint::Color0)->CreateRenderTargetView(rtvDesc);

			directCommandList.ClearTexture(m_IrradianceConvolutionCubemap_RT->GetTexture(AttachmentPoint::Color0), Colors::DebugMagenta);
			directCommandList.SetViewport(m_IrradianceConvolutionCubemap_RT->GetViewport());
			directCommandList.SetRenderTarget(*m_IrradianceConvolutionCubemap_RT);

			directCommandList.SetGraphics32BitConstants(0, viewProjMatrix);
			m_SkyboxCubeMesh->Draw(directCommandList);
		}
	}

	// Prefiltered Convolution Map
	{
		m_IBL_PSO->SetPipelineState(directCommandList, ImageBasedLightingPSO::Prefilter);
		directCommandList.SetShaderResourceView(1, 0, m_SkyCubemapTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		// Capture 6 cubemap directions with mips for prefiltered map
		for(int mipSlice = 0; mipSlice < sk_CubemapMipLevels; mipSlice++) {
			double currMipScale = std::pow(0.5, mipSlice);
			for(int i = 0; i < 6; i++) {
				auto viewMatrix = s_CubeMapCaptureViewMats[i];
				auto viewProjMatrix = viewMatrix * cubemapProjectionMat;

				D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
				rtvDesc.Format = m_PrefilterCubemap_RT->GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
				rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
				rtvDesc.Texture2DArray.MipSlice = mipSlice;
				rtvDesc.Texture2DArray.FirstArraySlice = i;
				rtvDesc.Texture2DArray.ArraySize = 1;
				m_PrefilterCubemap_RT->GetTexture(AttachmentPoint::Color0)->CreateRenderTargetView(rtvDesc);

				directCommandList.ClearTexture(m_PrefilterCubemap_RT->GetTexture(AttachmentPoint::Color0), Colors::DebugMagenta);
				directCommandList.SetViewport(m_PrefilterCubemap_RT->GetViewport({ (float)currMipScale, (float)currMipScale }));
				directCommandList.SetRenderTarget(*m_PrefilterCubemap_RT);

				directCommandList.SetGraphics32BitConstants(0, viewProjMatrix);
				float roughness = (float)mipSlice / (float)(sk_CubemapMipLevels - 1);
				directCommandList.SetGraphics32BitConstants(2, roughness);

				m_SkyboxCubeMesh->Draw(directCommandList);
			}
		}
	}
}

std::shared_ptr<Texture> Skybox::GetIrradianceTexture() const { return m_IrradianceConvolutionCubemap_RT->GetTexture(AttachmentPoint::Color0); };
std::shared_ptr<Texture> Skybox::GetPrefilterTexture()  const { return m_PrefilterCubemap_RT->GetTexture(AttachmentPoint::Color0); };
std::shared_ptr<Texture> Skybox::Get_BRDF_LUT_Texture() const { return m_BRDF_LUT_RT->GetTexture(AttachmentPoint::Color0); };

