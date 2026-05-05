#include "OutlineEffect.h"
#include "OutlinePSO.h"
#include "BloomEffect.h"
#include "RenderTarget.h"
#include "GameObject.h"
#include "CommandList.h"
#include "Texture.h"
#include "Scene.h"
#include "Colors.h"
#include "d3dx12.h"

OutlineEffect::OutlineEffect(Device& device, const RenderTarget& screenRenderTarget, OutlinePSO* outlinePSO, BloomPSO* bloomPSO)
	: m_OutlinePSO(outlinePSO)
{
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
		m_OutlineSilhouetteRT.AttachTexture(AttachmentPoint::Color0, outlineTexture);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
		srvDesc.Format = screenRenderTarget.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		outlineTexture->CreateShaderResourceView(srvDesc);
	}
	
	m_BloomEffect = std::make_unique<BloomEffect>(device, screenRenderTarget, bloomPSO, 2, 1.0f, 1.0f, 0.9f);
}

void OutlineEffect::Resize(uint32_t width, uint32_t height) {
	m_BloomEffect->ResizeRenderTargets(width, height);
	m_OutlineSilhouetteRT.Resize(width, height);
}

bool OutlineEffect::Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene, const RenderTarget& blendRenderTarget, const RenderTarget& outputRenderTarget) {
	/// TODO: support outlining multiple objects
	const GameObject* outlineObject = scene.GetPicker()->GetPickedObject();
	if(outlineObject == nullptr) return false;

	m_OutlinePSO->SetPipelineState(directCommandList);
	directCommandList.SetViewport(m_OutlineSilhouetteRT.GetViewport());
	directCommandList.SetRenderTarget(m_OutlineSilhouetteRT);
	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	directCommandList.ClearTexture(m_OutlineSilhouetteRT.GetTexture(AttachmentPoint::Color0), Colors::Clear);

	// Render object silhouette to intermediate RT
	outlineObject->RenderSilhouette(directCommandList, e, scene, m_OutlinePSO);

	// Bloom (blur) silhoutte
	m_BloomEffect->Render(directCommandList, m_OutlineSilhouetteRT, outputRenderTarget, XMFLOAT4 { 10.0f, 10.0f, 0.0f, 1.0f }, & blendRenderTarget, true);

	return true;
}

