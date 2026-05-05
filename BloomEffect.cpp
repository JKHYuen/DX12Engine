#include "BloomEffect.h"
#include "Device.h"
#include "Texture.h"
#include "RenderTarget.h"
#include "CommandList.h"
#include "BloomPSO.h"
#include "EditorGui.h"
#include "Colors.h"
#include "Logger.h"

#include <format>

BloomEffect::BloomEffect(Device& device, const RenderTarget& screenRenderTarget, BloomPSO* pso, int maxIterations, float intensity, float threshold, float softThreshold) :
	m_TextureFormat     { screenRenderTarget.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0] },
	m_MaxIterationCount { maxIterations },
	m_IterationCount    { maxIterations },
	m_Intensity         { intensity },
	m_Threshold         { threshold },
	m_SoftThreshold     { softThreshold },
	m_PSO               { pso },
	m_Device            { device }
{
	uint32_t textureWidth  = screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetWidth();
	uint32_t textureHeight = screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetHeight();

	// Initialize SRV
	m_SRVDesc.Format = m_TextureFormat;
	m_SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	m_SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	m_SRVDesc.Texture2D.MostDetailedMip = 0;
	m_SRVDesc.Texture2D.MipLevels = 1;

	// Create render targets
	{
		m_SamplingRenderTargets.resize(m_MaxIterationCount);
		for(size_t i = 0; i < m_MaxIterationCount; i++) {
			textureWidth /= 2;
			textureHeight /= 2;

			CreateSamplingRenderTarget(i, textureWidth, textureHeight);

			// Assume height is less than width
			if(textureHeight < 2) {
				m_IterationCount = (int)i + 1;
				break;
			}
		}
	}

	/// TODO: this doesn't work for multiple instances of bloom (currently using BloomEffect for outlining as well)
	// Create Debug SRV
	EditorGui::Get().RegisterImageSRV(device, m_SamplingRenderTargets[0].GetTexture(AttachmentPoint::Color0), &m_SRVDesc, EditorGui::GuiSRVIndex::BloomPrefilter);
}

void BloomEffect::CreateSamplingRenderTarget(size_t idx, uint32_t textureWidth, uint32_t textureHeight) {
	auto sampleTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		m_TextureFormat, textureWidth, textureHeight,
		1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
	);

	auto samplingTexture = std::make_shared<Texture>(m_Device, sampleTextureDesc, nullptr, true); // RTVs created here
	samplingTexture->SetName(std::format(L"Bloom Sample Texture {}", idx));
	m_SamplingRenderTargets[idx].AttachTexture(AttachmentPoint::Color0, samplingTexture);

	samplingTexture->CreateShaderResourceView(m_SRVDesc);
}

void BloomEffect::Render(CommandList& directCommandList, const RenderTarget& inputRenderTarget, const RenderTarget& outputRenderTarget, XMFLOAT4 colorMultiply, const RenderTarget* blendRenderTarget, bool b_MaskOutInput) {
	m_PSO->SetPipelineState(directCommandList);

	// First downsample + prefilter
	BloomPSO::BloomProps bloomProps {};
	float knee = m_Threshold * m_SoftThreshold;
	bloomProps.colorMultiply  = colorMultiply;
	bloomProps.filter         = { m_Threshold, m_Threshold - knee, 2.0f * knee, 0.25f / (knee + 0.00001f) };
	bloomProps.boxSampleDelta = 1.0f;
	bloomProps.intensity      = m_Intensity;
	bloomProps.usePrefilter   = 1.0f;
	bloomProps.useFinalPass   = 0.0f;

	directCommandList.SetGraphicsDynamicConstantBuffer(BloomPSO::BloomRootParameters::BloomCB, bloomProps);
	directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 0, inputRenderTarget.GetTexture(AttachmentPoint::Color0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	directCommandList.SetNullShaderResourceView(BloomPSO::BloomRootParameters::Textures, 1);
	directCommandList.SetNullShaderResourceView(BloomPSO::BloomRootParameters::Textures, 2);

	directCommandList.ClearTexture(m_SamplingRenderTargets[0].GetTexture(AttachmentPoint::Color0), Colors::DebugMagenta);
	directCommandList.SetRenderTarget(m_SamplingRenderTargets[0]);
	directCommandList.SetViewport(m_SamplingRenderTargets[0].GetViewport());
	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	directCommandList.Draw(3);

	// Progressive Downsampling
	bloomProps.usePrefilter = 0.0f;
	directCommandList.SetGraphicsDynamicConstantBuffer(BloomPSO::BloomRootParameters::BloomCB, bloomProps);
	int i = 1;
	for(; i < m_IterationCount; i++) {
		directCommandList.ClearTexture(m_SamplingRenderTargets[i].GetTexture(AttachmentPoint::Color0), Colors::DebugMagenta);
		directCommandList.SetRenderTarget(m_SamplingRenderTargets[i]);
		directCommandList.SetViewport(m_SamplingRenderTargets[i].GetViewport());

		directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 0, m_SamplingRenderTargets[i - 1].GetTexture(AttachmentPoint::Color0));
		directCommandList.SetNullShaderResourceView(BloomPSO::BloomRootParameters::Textures, 1);
		directCommandList.SetNullShaderResourceView(BloomPSO::BloomRootParameters::Textures, 2);

		directCommandList.Draw(3);
	}

	// Progressive Upsampling
	m_PSO->SetAdditivePipelineState(directCommandList);
	bloomProps.boxSampleDelta = 0.5f;
	directCommandList.SetGraphicsDynamicConstantBuffer(BloomPSO::BloomRootParameters::BloomCB, bloomProps);
	for(i -= 2; i >= 0; i--) {
		// NOTE: do not clear m_SamplingRenderTargets[i], we need it for additive blending
		directCommandList.SetRenderTarget(m_SamplingRenderTargets[i]);
		directCommandList.SetViewport(m_SamplingRenderTargets[i].GetViewport());

		directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 0, m_SamplingRenderTargets[i + 1].GetTexture(AttachmentPoint::Color0));
		directCommandList.SetNullShaderResourceView(BloomPSO::BloomRootParameters::Textures, 1);
		directCommandList.SetNullShaderResourceView(BloomPSO::BloomRootParameters::Textures, 2);

		directCommandList.Draw(3);
	}

	// Final Pass
	// disable additive blending from progressive upsampling
	m_PSO->SetPipelineState(directCommandList); 
	bloomProps.useFinalPass = 1.0f;
	directCommandList.SetGraphicsDynamicConstantBuffer(BloomPSO::BloomRootParameters::BloomCB, bloomProps);
	directCommandList.SetRenderTarget(outputRenderTarget);
	directCommandList.SetViewport(outputRenderTarget.GetViewport());

	directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 0, m_SamplingRenderTargets[0].GetTexture(AttachmentPoint::Color0));
	directCommandList.SetShaderResourceView(
		BloomPSO::BloomRootParameters::Textures, 1, 
		(blendRenderTarget == nullptr) ? 
			inputRenderTarget.GetTexture(AttachmentPoint::Color0) : blendRenderTarget->GetTexture(AttachmentPoint::Color0),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);

	if(b_MaskOutInput) {
		directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 2, inputRenderTarget.GetTexture(AttachmentPoint::Color0));
	}
	else {
		directCommandList.SetNullShaderResourceView(BloomPSO::BloomRootParameters::Textures, 2);
	}

	directCommandList.Draw(3);
}

void BloomEffect::ResizeRenderTargets(uint32_t width, uint32_t height) {
	if(m_SamplingRenderTargets.empty() ||
		(m_SamplingRenderTargets[0].GetTexture(AttachmentPoint::Color0)->GetWidth() * 2 == width &&
		 m_SamplingRenderTargets[0].GetTexture(AttachmentPoint::Color0)->GetHeight() * 2 == height)) {
		return;
	}

	auto textureWidth = width;
	auto textureHeight = height;

	// Call resize here, might make m_MaxIterationCount tweakable in the future
	m_IterationCount = m_MaxIterationCount;
	m_SamplingRenderTargets.resize(m_MaxIterationCount);

	for(size_t i = 0; i < m_MaxIterationCount; i++) {
		textureWidth /= 2;
		textureHeight /= 2;

		if(m_SamplingRenderTargets[i].IsEmpty(AttachmentPoint::Color0)) {
			CreateSamplingRenderTarget(i, textureWidth, textureHeight);
		}
		else {
			m_SamplingRenderTargets[i].Resize(textureWidth, textureHeight);
		}

		// Assume height is less than width
		if(textureHeight < 2) {
			m_IterationCount = (int)i + 1;
			break;
		}
	}

	EditorGui::Get().RegisterImageSRV(m_Device, m_SamplingRenderTargets[0].GetTexture(AttachmentPoint::Color0), &m_SRVDesc, EditorGui::GuiSRVIndex::BloomPrefilter);
}

