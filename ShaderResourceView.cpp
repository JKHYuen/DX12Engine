#include <DX12LibPCH.h>

#include "ShaderResourceView.h"
#include "Device.h"
#include "Resource.h"

ShaderResourceView::ShaderResourceView(Device& device, const std::shared_ptr<Resource>& resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srv)
    : m_Resource(resource) {
    assert(resource || srv);

    auto d3d12Resource = m_Resource ? m_Resource->GetD3D12Resource() : nullptr;

    m_Descriptor = device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    device.GetD3D12Device()->CreateShaderResourceView(d3d12Resource.Get(), srv, m_Descriptor.GetDescriptorHandle());
}
