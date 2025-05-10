#include <DX12LibPCH.h>

#include "ByteAddressBuffer.h"

ByteAddressBuffer::ByteAddressBuffer(Device& device, size_t bufferSize)
	: Buffer(device, CD3DX12_RESOURCE_DESC::Buffer(Math::AlignUp(bufferSize, 4), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)) {}

ByteAddressBuffer::ByteAddressBuffer(Device& device, ComPtr<ID3D12Resource> resource)
	: Buffer(device, resource) {}