#include "OutlineEffect.h"
#include "OutlinePSO.h"
#include "BloomEffect.h"
#include "RenderTarget.h"
#include "Texture.h"
#include "d3dx12.h"

OutlineEffect::OutlineEffect(Device& device, const RenderTarget& screenRenderTarget, OutlinePSO* outlinePSO, BloomEffect* bloomRenderPass)
	: m_OutlinePSO(outlinePSO)
	, m_BloomEffect(bloomRenderPass) 
{
	auto outlineTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		screenRenderTarget.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0],
		screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetWidth(),
		screenRenderTarget.GetTexture(AttachmentPoint::Color0)->GetHeight(),
		1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
	);

	auto outlineTexture = std::make_shared<Texture>(device, outlineTextureDesc, nullptr, true); // RTVs created here
	outlineTexture->SetName(L"Outline Texture");
	m_OutlineRT.AttachTexture(AttachmentPoint::Color0, outlineTexture);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
	srvDesc.Format = screenRenderTarget.GetRenderTargetFormats().RTFormats[AttachmentPoint::Color0];
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	outlineTexture->CreateShaderResourceView(srvDesc);

	/// Is this allowed?
	m_OutlineRT.AttachTexture(AttachmentPoint::DepthStencil, screenRenderTarget.GetTexture(AttachmentPoint::DepthStencil));
}

void OutlineEffect::Render(CommandList& directCommandList, const RenderTarget& screenRenderTarget, const RenderTarget& outputRenderTarget) {

}

