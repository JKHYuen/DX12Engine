#pragma once

#include "d3dx12.h"
#include <dxgi1_6.h>
#include <wrl/client.h>

class CommandQueue;
class DescriptorAllocator;

class Device {
public:
	Device(DXGI_GPU_PREFERENCE gpuPreference, bool useWarp);

private:
    Microsoft::WRL::ComPtr<ID3D12Device2> m_D3d12Device;

    Microsoft::WRL::ComPtr<IDXGIAdapter4> m_DxgiAdapter;
    DXGI_ADAPTER_DESC3 m_Desc;

    // Default command queues.
    std::unique_ptr<CommandQueue> m_DirectCommandQueue;
    std::unique_ptr<CommandQueue> m_ComputeCommandQueue;
    std::unique_ptr<CommandQueue> m_CopyCommandQueue;

    // Descriptor allocators.
    std::unique_ptr<DescriptorAllocator> m_DescriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    D3D_ROOT_SIGNATURE_VERSION m_HighestRootSignatureVersion;

};

