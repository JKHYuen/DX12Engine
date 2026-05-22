#include <DX12LibPCH.h>
#include <memory>

#include "StructuredBuffer.h"
#include "Device.h"

// Note: maybe we don't need to create a counter until we need it
StructuredBuffer::StructuredBuffer(Device& device, size_t numElements, size_t elementSize)
    : Buffer(device, CD3DX12_RESOURCE_DESC::Buffer(numElements * elementSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
    , m_NumElements(numElements)
    , m_ElementSize(elementSize) {
    m_CounterBuffer = std::make_shared<ByteAddressBuffer>(device, 4);
}

StructuredBuffer::StructuredBuffer(Device& device, Microsoft::WRL::ComPtr<ID3D12Resource> resource, size_t numElements,
    size_t elementSize)
    : Buffer(device, resource)
    , m_NumElements(numElements)
    , m_ElementSize(elementSize) {
    m_CounterBuffer = std::make_shared<ByteAddressBuffer>(device, 4);
}