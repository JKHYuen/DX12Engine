#pragma once

// Wrapper for Dear ImGui
// Note: Seperate free list allocator and static SRV descriptor heap used instead of DynamicDescriptorHeap in main engine for simplicity

/// TODO: this class is effectively a singleton with static calls (oh no), constructor should only be called once, will clean this up later

#include "imgui.h"

#include <d3dx12.h>
#include <wrl/client.h>

class Device;
class CommandList;
class Resource;

class EditorGui {
public:
	EditorGui(Device& device, DXGI_FORMAT RTVformat, int bufferCount, HWND hwnd);
	~EditorGui();

	// Disable copy and move
	EditorGui(const EditorGui&)       = delete;
	EditorGui& operator=(EditorGui&)  = delete;
	EditorGui(EditorGui&&)            = delete;
	EditorGui& operator=(EditorGui&&) = delete;

	// Called at start of frame
	void NewFrame();

	// Called after rendering 3D elements before present, 
	// it is assumed that screen render target is set to pipeline already
	void Render(CommandList& directCommandList);

	struct GuiDescriptorAllocation {
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	};

	// This must be manually added to for every type of texture that is shown on debug GUI.
	// Do this instead of using hardcoded strings, so I don't have to memorize them in the future
	// (Good enough for now)
	enum GuiSRVIndex {
		DirectionalShadowMap,
		BloomPrefilter,

		NumGuiSRVIndex
	};

	// Allocate to s_D3DSrvDescHeapAllocator
	static void AllocateImageSRV(Device& device, const std::shared_ptr<Resource>& resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, GuiSRVIndex srvIndex);

	static void FreeImageSRV(GuiDescriptorAllocation alloc);

	static inline GuiDescriptorAllocation GetImageSRV(GuiSRVIndex index) { return s_ImageSRVs[index]; }

	static inline bool sb_ShowImGuiWindow = false;
	static inline void ToggleImGuiVisibilityState() { sb_ShowImGuiWindow = !sb_ShowImGuiWindow; }

private:
	const int mk_SRVHeapSize = 64;

	// Simple free list based allocator
	// Source: https://github.com/ocornut/imgui/blob/master/examples/example_win32_directx12/main.cpp
	struct DescriptorHeapAllocator {
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Heap = nullptr;
		D3D12_DESCRIPTOR_HEAP_TYPE  HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
		D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
		UINT                        HeapHandleIncrement;
		ImVector<int>               FreeIndices;

		void Initialize(ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap) {
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

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_D3DSrvDescHeap;

	// static so it can be used in ImGui callback lambdas
	static inline DescriptorHeapAllocator s_D3DSrvDescHeapAllocator {};

	// Stores all created GuiDescriptorAllocations created by "AllocateImageSRV()" 
	// Static member for easy access; primarily for debug menu. Indices are enum "GuiSRVIndex"
	// Freed allocations will never be removed since all allocated will always be used in current implementation
	static inline std::vector<GuiDescriptorAllocation> s_ImageSRVs{ GuiSRVIndex::NumGuiSRVIndex };
};

