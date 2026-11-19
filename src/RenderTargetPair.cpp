#include "RenderTargetPair.h"

#include "Colors.h"

#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/RenderTarget.h"
#include "DX12EngineCore/Texture.h"

#include <cstdint>
#include <d3d12.h>
#include <d3dx12_core.h>
#include <dxgiformat.h>
#include <format>
#include <memory>

RenderTargetPair::RenderTargetPair(Device& device, DXGI_FORMAT colorFormat, uint32_t width, uint32_t height) {
    auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        colorFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
    );

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = colorFormat;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    for(int i = 0; i < 2; i++) {
        m_RTs[i] = std::make_unique<RenderTarget>();
        auto texture = std::make_shared<Texture>(device, textureDesc, nullptr, true);
        texture->SetName(std::format(L"Post Process Render Target {}", i));
        m_RTs[i]->AttachTexture(AttachmentPoint::Color0, texture);
        texture->CreateShaderResourceView(srvDesc);
    }
}

void RenderTargetPair::Resize(uint32_t width, uint32_t height) {
    m_RTs[RTType::input]->Resize(width, height);
    m_RTs[RTType::output]->Resize(width, height);
}

void RenderTargetPair::ClearInputRT(CommandList& directCommandList) {
    directCommandList.ClearTexture(m_RTs[RTType::input]->GetTexture(AttachmentPoint::Color0), Colors::DebugMagenta);
}

void RenderTargetPair::ClearOutputRT(CommandList& directCommandList) {
    directCommandList.ClearTexture(m_RTs[RTType::output]->GetTexture(AttachmentPoint::Color0), Colors::DebugMagenta);
}

void RenderTargetPair::SwapRTs() {
    std::swap(m_RTs[RTType::input], m_RTs[RTType::output]);
}

RenderTarget& RenderTargetPair::GetInputRT() {
    return *m_RTs[RTType::input];
}

RenderTarget& RenderTargetPair::GetOutputRT() {
    return *m_RTs[RTType::output];
}

