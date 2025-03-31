#include <DX12LibPCH.h>

#include "ByteAddressBuffer.h"

ByteAddressBuffer::ByteAddressBuffer(const D3D12_RESOURCE_DESC& resDesc)
	: Buffer(resDesc) {}

ByteAddressBuffer::ByteAddressBuffer(ComPtr<ID3D12Resource> resource)
	: Buffer(resource) {}