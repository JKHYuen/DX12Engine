#include "DX12EngineCore/RenderTarget.h"
#include "DX12EngineCore/Texture.h"
#include "DX12EngineCore/CommandList.h"
#include "GameObject.h"
#include "OutlineEffect.h"
#include "UnlitPSO.h"
#include "BloomEffect.h"

#include "Texture.h"
#include "Scene.h"
#include "Colors.h"
#include "d3dx12.h"

OutlineEffect::OutlineEffect(Device& device, const RenderTarget& screenRenderTarget, UnlitPSO* outlinePSO, BloomPSO* bloomPSO)
	: m_UnlitPSO(outlinePSO)
{
	m_OutlineSilhouetteRT = std::make_unique<RenderTarget>();

	uint32_t outputWidth = screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetWidth();
	uint32_t outputHeight = screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetHeight();

	// Color texture for outline RT
	{
		auto outlineTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			screenRenderTarget.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0],
			outputWidth, outputHeight,
			1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		auto outlineTexture = std::make_shared<Texture>(device, outlineTextureDesc, nullptr, true); // RTVs created here
		outlineTexture->SetName(L"Outline Texture");
		m_OutlineSilhouetteRT->AttachTexture(AttachmentPoint::Color0, outlineTexture);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
		srvDesc.Format = screenRenderTarget.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		outlineTexture->CreateShaderResourceView(srvDesc);
	}
	
	// Note: starting outline color is hardcoded here, can be modified with in app GUI after initialization
	XMFLOAT4 defaultOutlineColor { 15.0f, 10.0f, 0.0f, 1.0f }; // HDR yellow color
	m_BloomEffect = std::make_unique<BloomEffect>(device, screenRenderTarget, bloomPSO, 2, 1.0f, 1.0f, 1.5f, defaultOutlineColor, EditorGui::OutlineBloomPrefilter);
}

void OutlineEffect::ResizeRenderTargets(uint32_t width, uint32_t height) {
	m_BloomEffect->ResizeRenderTargets(width, height);
	m_OutlineSilhouetteRT->Resize(width, height);
}

bool OutlineEffect::Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene, const RenderTarget& blendRenderTarget, const RenderTarget& outputRenderTarget) {
	if(mb_DisableEffect) return false;

	/// TODO: support outlining multiple objects
	GameObject* outlineObject = scene.GetPicker()->GetPickedObject();
	if(outlineObject == nullptr) return false;

	m_UnlitPSO->SetPipelineState(directCommandList);
	directCommandList.SetViewport(m_OutlineSilhouetteRT->GetViewport());
	directCommandList.SetRenderTarget(*m_OutlineSilhouetteRT);
	directCommandList.ClearTexture(m_OutlineSilhouetteRT->GetTexture(AttachmentPoint::Color0), Colors::Clear);

	// Render object silhouette to intermediate RT
	// Note: unlit color has to be white, outline color added in bloom stage
	outlineObject->RenderSilhouette(directCommandList, e, m_UnlitPSO, XMFLOAT4(1.0, 1.0, 1.0, 1.0));

	// Bloom (blur) rendered silhoutte
	m_BloomEffect->Render(directCommandList, *m_OutlineSilhouetteRT, outputRenderTarget, & blendRenderTarget, true);

	return true;
}

