#include <DX12LibPCH.h>
#include "Device.h"
#include "DescriptorAllocator.h"
#include "CommandQueue.h"

/// TODO: figure out something less flimsy
#include "../StringHelpers.h"

namespace {
    constexpr D3D_FEATURE_LEVEL targetFeatureLevel = D3D_FEATURE_LEVEL_12_1;
}

Device::Device(DXGI_GPU_PREFERENCE gpuPreference, bool useWarp) {
    if(!m_DxgiAdapter) {
        ComPtr<IDXGIFactory6> dxgiFactory6;
        ComPtr<IDXGIAdapter>  dxgiAdapter;

        UINT createFactoryFlags = 0;
#if defined( _DEBUG )
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        ThrowIfFailed(::CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory6)));

        if(useWarp) {
            ThrowIfFailed(dxgiFactory6->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter)));
            ThrowIfFailed(dxgiAdapter.As(&m_DxgiAdapter));
        }
        else {
            for(UINT i = 0; dxgiFactory6->EnumAdapterByGpuPreference(i, gpuPreference, IID_PPV_ARGS(&dxgiAdapter)) !=
                DXGI_ERROR_NOT_FOUND;
                ++i) {
                if(SUCCEEDED(D3D12CreateDevice(dxgiAdapter.Get(), targetFeatureLevel, __uuidof(ID3D12Device), nullptr))) {
                    ThrowIfFailed(dxgiAdapter.As(&m_DxgiAdapter));
                    break;
                }
            }
        }
        
        if(m_DxgiAdapter) {
            ThrowIfFailed(m_DxgiAdapter->GetDesc3(&m_AdapterDesc));
        }
        assert(m_DxgiAdapter);

        StringConvert::WideString_To_String(m_AdapterDesc.Description, m_AdapterName);
    }

    ThrowIfFailed(D3D12CreateDevice(m_DxgiAdapter.Get(), targetFeatureLevel, IID_PPV_ARGS(&m_D3d12Device)));

    // Enable debug messages (only works if the debug layer has already been enabled).
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if(SUCCEEDED(m_D3d12Device.As(&pInfoQueue))) {
        //pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE );
        //pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR, TRUE );
        //pInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_WARNING, TRUE );

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] = {D3D12_MESSAGE_SEVERITY_INFO};

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, 

            // These warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,  
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }

    m_DirectCommandQueue = std::make_unique<CommandQueue>(*this, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_ComputeCommandQueue = std::make_unique<CommandQueue>(*this, D3D12_COMMAND_LIST_TYPE_COMPUTE);
    m_CopyCommandQueue = std::make_unique<CommandQueue>(*this, D3D12_COMMAND_LIST_TYPE_COPY);

    // Create one descriptor allocator per (CPU) descriptor heap type for global use
    for(int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
        m_DescriptorAllocators[i] = std::make_unique<DescriptorAllocator>(*this, static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
    }

    // Check features.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData;
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if(FAILED(m_D3d12Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData,
            sizeof(D3D12_FEATURE_DATA_ROOT_SIGNATURE)))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }
        m_HighestRootSignatureVersion = featureData.HighestVersion;
    }
}

DXGI_QUERY_VIDEO_MEMORY_INFO Device::GetVRAMUsed() const {
    DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
    m_DxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo);
    return videoMemoryInfo;
}

CommandQueue& Device::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type) {
    CommandQueue* commandQueue {};
    switch(type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
        commandQueue = m_DirectCommandQueue.get();
        break;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        commandQueue = m_ComputeCommandQueue.get();
        break;
    case D3D12_COMMAND_LIST_TYPE_COPY:
        commandQueue = m_CopyCommandQueue.get();
        break;
    default:
        assert(false && "Invalid command queue type.");
    }

    return *commandQueue;
}

DXGI_SAMPLE_DESC Device::GetMultisampleQualityLevels(DXGI_FORMAT format, UINT numSamples,
    D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags) const {
    DXGI_SAMPLE_DESC sampleDesc = {1, 0};

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels;
    qualityLevels.Format = format;
    qualityLevels.SampleCount = 1;
    qualityLevels.Flags = flags;
    qualityLevels.NumQualityLevels = 0;

    while(
        qualityLevels.SampleCount <= numSamples &&
        SUCCEEDED(m_D3d12Device->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
            &qualityLevels,
            sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS))
        ) &&
        qualityLevels.NumQualityLevels > 0) {
        // That works...
        sampleDesc.Count = qualityLevels.SampleCount;
        sampleDesc.Quality = qualityLevels.NumQualityLevels - 1;

        // But can we do better?
        qualityLevels.SampleCount *= 2;
    }

    return sampleDesc;
}

void Device::FlushWait() {
    m_DirectCommandQueue->FlushWait();
    m_ComputeCommandQueue->FlushWait();
    m_CopyCommandQueue->FlushWait();
}

DescriptorAllocation Device::AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors) {
    return m_DescriptorAllocators[type]->Allocate(numDescriptors);
}

void Device::ReleaseStaleDescriptors() {
    for(int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) { m_DescriptorAllocators[i]->ReleaseStaleDescriptors(); }
}


