#include "BloomPass.h"
#include "Device.h"
#include "Texture.h"
#include "RenderTarget.h"
#include "CommandList.h"
#include "BloomPSO.h"
#include "EditorGui.h"
#include "Colors.h"
#include "Logger.h"

#include <format>

BloomPass::BloomPass(Device& device, const RenderTarget& screenRenderTarget, BloomPSO* pso, int maxIterations) :
	m_IterationCount{maxIterations},
	m_PSO{pso}
{
	uint32_t textureWidth  = screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetWidth();
	uint32_t textureHeight = screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetHeight();
	DXGI_FORMAT screenTextureFormat = screenRenderTarget.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];

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
			1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		// Intermediate Render Targets
		m_SamplingRenderTargets.reserve(maxIterations);
		for(size_t i = 0; i < maxIterations; i++) {
			textureWidth /= 2;
			textureHeight /= 2;

			m_SamplingRenderTargets.emplace_back();

			auto sampleTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				screenTextureFormat, textureWidth, textureHeight,
				1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
			);

			auto samplingTexture = std::make_shared<Texture>(device, sampleTextureDesc, nullptr, true); // RTVs created here
			samplingTexture->SetName(std::format(L"Bloom Sample Texture {}", i));
			m_SamplingRenderTargets[i].AttachTexture(AttachmentPoint::Color0, samplingTexture);

			samplingTexture->CreateShaderResourceView(srvDesc);

			// Assume height is less than width
			if(textureHeight < 2) {
				m_IterationCount = (int)i + 1;
				break;
			}
		}
	}

	/// Create Debug SRV
	EditorGui::AllocateImageSRV(device, m_SamplingRenderTargets[0].GetTexture(AttachmentPoint::Color0), &srvDesc, EditorGui::GuiSRVIndex::BloomPrefilter);

}

void BloomPass::Render(CommandList& directCommandList, const RenderTarget& inputRenderTarget, const RenderTarget& outputRenderTarget) {
	m_PSO->SetPipelineState(directCommandList);

	// First downsample + prefilter
	BloomPSO::BloomProps bloomProps {};
	float knee = m_Threshold * m_SoftThreshold;
	bloomProps.filter = { m_Threshold, m_Threshold - knee, 2.0f * knee, 0.25f / (knee + 0.00001f) };
	bloomProps.boxSampleDelta = 1.0f;
	bloomProps.intensity      = m_Intensity;
	bloomProps.usePrefilter   = 1.0f;
	bloomProps.useFinalPass   = 0.0f;

	directCommandList.SetGraphicsDynamicConstantBuffer(BloomPSO::BloomRootParameters::BloomCB, bloomProps);
	directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 0, inputRenderTarget.GetTexture(AttachmentPoint::Color0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 1, nullptr);

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
		directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 1, nullptr);

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
		directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 1, nullptr);

		directCommandList.Draw(3);
	}

	// Final Pass
	m_PSO->SetPipelineState(directCommandList); // disable additive blending from progressive upsampling
	bloomProps.useFinalPass = 1.0f;
	directCommandList.SetGraphicsDynamicConstantBuffer(BloomPSO::BloomRootParameters::BloomCB, bloomProps);
	directCommandList.SetRenderTarget(outputRenderTarget);
	directCommandList.SetViewport(outputRenderTarget.GetViewport());
	directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 0, m_SamplingRenderTargets[0].GetTexture(AttachmentPoint::Color0));
	directCommandList.SetShaderResourceView(BloomPSO::BloomRootParameters::Textures, 1, inputRenderTarget.GetTexture(AttachmentPoint::Color0));

	directCommandList.Draw(3);
}