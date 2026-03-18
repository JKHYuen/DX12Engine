#pragma once

/*
 *  Copyright(c) 2020 Jeremiah van Oosten
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files(the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions :
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

 /**
  *  @file Device.h
  *  @date October 9, 2020
  *  @author Jeremiah van Oosten
  *
  *  @brief A wrapper for the D3D12Device.
  */

// Removed adapter patterns from Jeremiah's version - KHY

#include <dxgi1_6.h>
#include <wrl/client.h>

#include "Helpers.h"
#include "DescriptorAllocator.h"

class CommandQueue;
class DescriptorAllocation;

class Device {
public:
    Device(DXGI_GPU_PREFERENCE gpuPreference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, bool useWarp = false);

    /**
     * Get a description of the adapter that was used to create the device.
     */
    const std::wstring GetAdapterDescription() const {
        return m_AdapterDesc.Description;
    }

    /**
     * Allocate a number of CPU visible descriptors.
     */
    DescriptorAllocation AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors = 1);

    /**
     * Gets the size of the handle increment for the given type of descriptor heap.
     */
    UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const {
        return m_D3d12Device->GetDescriptorHandleIncrementSize(type);
    }

    /**
     * Flush all command queues.
     */
    void Flush();

    /**
     * Release stale descriptors. This should only be called with a completed frame counter.
     */
    void ReleaseStaleDescriptors();

    Microsoft::WRL::ComPtr<IDXGIAdapter4> GetAdapter() const {
        return m_DxgiAdapter;
    }

    /**
     * Get a command queue. Valid types are:
     * - D3D12_COMMAND_LIST_TYPE_DIRECT : Can be used for draw, dispatch, or copy commands.
     * - D3D12_COMMAND_LIST_TYPE_COMPUTE: Can be used for dispatch or copy commands.
     * - D3D12_COMMAND_LIST_TYPE_COPY   : Can be used for copy commands.
     * By default, a D3D12_COMMAND_LIST_TYPE_DIRECT queue is returned.
     */
    CommandQueue& GetCommandQueue(D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

    Microsoft::WRL::ComPtr<ID3D12Device2> GetD3D12Device() const {
        return m_D3d12Device;
    }

    D3D_ROOT_SIGNATURE_VERSION GetHighestRootSignatureVersion() const {
        return m_HighestRootSignatureVersion;
    }

    /**
     * Check if the requested multisample quality is supported for the given format.
     */
    DXGI_SAMPLE_DESC GetMultisampleQualityLevels(
        DXGI_FORMAT format,
        UINT numSamples = D3D12_MAX_MULTISAMPLE_SAMPLE_COUNT, 
        D3D12_MULTISAMPLE_QUALITY_LEVEL_FLAGS flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE
    ) const;

    template<class T>
    void CreatePipelineState( T& pipelineStateStream, Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipelineState) {
        D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = { sizeof(T), &pipelineStateStream};
        ThrowIfFailed(m_D3d12Device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));
    }

private:
    Microsoft::WRL::ComPtr<ID3D12Device2> m_D3d12Device;

    Microsoft::WRL::ComPtr<IDXGIAdapter4> m_DxgiAdapter;
    DXGI_ADAPTER_DESC3 m_AdapterDesc;

    // Default command queues.
    std::unique_ptr<CommandQueue> m_DirectCommandQueue;
    std::unique_ptr<CommandQueue> m_ComputeCommandQueue;
    std::unique_ptr<CommandQueue> m_CopyCommandQueue;

    // Descriptor allocators.
    std::unique_ptr<DescriptorAllocator> m_DescriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    D3D_ROOT_SIGNATURE_VERSION m_HighestRootSignatureVersion;

};

