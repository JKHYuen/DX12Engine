#include "BloomPass.h"
#include "Device.h"
#include "Texture.h"
#include "RenderTarget.h"

#include <format>


BloomPass::BloomPass(Device& device, const RenderTarget& screenRenderTarget, int maxIterations) :
	m_IterationCount{maxIterations}
{
	uint32_t textureWidth  = screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetWidth();
	uint32_t textureHeight = screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetHeight();
	DXGI_FORMAT screenTextureFormat = screenRenderTarget.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
	DXGI_SAMPLE_DESC screenSampleDesc = screenRenderTarget.GetSampleDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = screenTextureFormat;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;

	// Create render targets
	{
		auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			screenTextureFormat, textureWidth, textureHeight,
			1, 1, screenSampleDesc.Count, screenSampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		auto bloomOutputTexture = std::make_shared<Texture>(device, textureDesc, nullptr, false);
		bloomOutputTexture->SetName(L"Bloom Output");

		m_BloomOutputRT.AttachTexture(AttachmentPoint::Color0, bloomOutputTexture);
		bloomOutputTexture->CreateShaderResourceView(srvDesc);

		// Intermediate Render Targets
		m_SamplingRenderTargets.reserve(maxIterations);
		for(size_t i = 0; i < maxIterations; i++) {
			textureWidth /= 2;
			textureHeight /= 2;

			m_SamplingRenderTargets.emplace_back();

			auto sampleTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				screenTextureFormat, textureWidth, textureHeight,
				1, 1, screenSampleDesc.Count, screenSampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
			);

			auto samplingTexture = std::make_shared<Texture>(device, sampleTextureDesc, nullptr, false);
			samplingTexture->SetName(std::format(L"Sample Texture {}", i));
			m_SamplingRenderTargets[i].AttachTexture(AttachmentPoint::Color0, samplingTexture);
			samplingTexture->CreateShaderResourceView(srvDesc);

			// Assume height is less than width
			if(textureHeight < 2) {
				m_IterationCount = i + 1;
				break;
			}
		}



		//D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
		//rtvDesc.Format = screenTextureFormat;
		//rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		//rtvDesc.Texture2D.MipSlice = 0;
		//m_SamplingRenderTargets[i - 1].GetTexture(AttachmentPoint::Color0)->CreateRenderTargetView(rtvDesc);
		


	}
}
