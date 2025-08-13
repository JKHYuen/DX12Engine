#pragma once

// Wrapper for Dear ImGui
// Note: Seperate free list allocator and static SRV descriptor heap used instead of DynamicDescriptorHeap in main engine for simplicity

#include "imgui.h"
#include <d3dx12.h>
#include <wrl/client.h>

class Device;
class CommandList;

class EditorGui {
public:
	EditorGui(Device& device, DXGI_FORMAT RTVformat, int bufferCount, HWND hwnd);
	~EditorGui();

	// Called at start of frame
	void NewFrame();

	// Called after rendering 3D elements
	void Render(CommandList& directCommandList);

private:
	// Simple free list based allocator from https://github.com/ocornut/imgui/blob/master/examples/example_win32_directx12/main.cpp
	struct DescriptorHeapAllocator {
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Heap = nullptr;
		D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
		D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
		UINT                        HeapHandleIncrement;
		ImVector<int>               FreeIndices;

		void Create(ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap) {
			IM_ASSERT(Heap == nullptr && FreeIndices.empty());
			Heap = heap;
			D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
			HeapType = desc.Type;
			HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
			HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
			HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
			FreeIndices.reserve((int)desc.NumDescriptors);
			for(int n = desc.NumDescriptors; n > 0; n--)
				FreeIndices.push_back(n - 1);
		}

		void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle) {
			IM_ASSERT(FreeIndices.Size > 0);
			int idx = FreeIndices.back();
			FreeIndices.pop_back();
			out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
			out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
		}

		void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle) {
			int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
			int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
			IM_ASSERT(cpu_idx == gpu_idx);
			FreeIndices.push_back(cpu_idx);
		}
	};

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_D3DSrvDescHeap = nullptr;

	static inline DescriptorHeapAllocator s_D3DSrvDescHeapAlloc;
};

