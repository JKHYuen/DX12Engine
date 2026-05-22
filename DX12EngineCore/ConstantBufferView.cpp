#include <DX12LibPCH.h>

#include "ConstantBufferView.h"
#include "ConstantBuffer.h"
#include "Device.h"

ConstantBufferView::ConstantBufferView(Device& device, const std::shared_ptr<ConstantBuffer>& constantBuffer, size_t offset)
    : m_ConstantBuffer(constantBuffer) {
    assert(constantBuffer);

    auto d3d12Resource = m_ConstantBuffer->GetD3D12Resource();

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv;
    cbv.BufferLocation = d3d12Resource->GetGPUVirtualAddress() + offset;
    // Constant buffers must be aligned for hardware requirements.
    cbv.SizeInBytes = static_cast<UINT>(Math::AlignUp(m_ConstantBuffer->GetSizeInBytes(), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));  

    m_Descriptor = device.AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    device.GetD3D12Device()->CreateConstantBufferView(&cbv, m_Descriptor.GetDescriptorHandle());
}
