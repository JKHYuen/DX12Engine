#include <DX12LibPCH.h>

#include "Buffer.h"

Buffer::Buffer(const D3D12_RESOURCE_DESC& resDesc)
	: Resource(resDesc) {}

Buffer::Buffer(Microsoft::WRL::ComPtr<ID3D12Resource> resource)
	: Resource(resource) {}