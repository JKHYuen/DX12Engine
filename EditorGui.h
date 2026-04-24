#pragma once

// Wrapper for Dear ImGui
// Note: Seperate free list allocator and static SRV descriptor heap used instead of DynamicDescriptorHeap in main engine for simplicity

// Singleton implementation for easy access to RegisterImageSRV() and FreeImageSRV() methods (used to display debug textures), too much dependency injection otherwise. Every game instance should only have one EditorGui instance, if game instances are switched, we can recreate EditorGui. Note that the stored descriptors in this class is currently never destroyed during runtime.

#include "imgui.h"

#include <d3dx12.h>
#include <wrl/client.h>

class Device;
class CommandList;
class Resource;

class EditorGui {
public:
	/// Singleton
	EditorGui(Device& device, DXGI_FORMAT RTVformat, int bufferCount, HWND hwnd);
	~EditorGui();

	EditorGui(const EditorGui&)			    = delete;
	EditorGui& operator=(const EditorGui&)  = delete;
	EditorGui(EditorGui&&)                  = delete;
	EditorGui& operator=(EditorGui&&)       = delete;

	static EditorGui& Create(Device& device, DXGI_FORMAT RTVformat, int bufferCount, HWND hwnd);
	static EditorGui& Get();
	static void Destroy();
	///

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

	// Allocate to internal descriptor heap SRV with s_D3DSrvDescHeapAllocator
	// Use this to allocate a SRV to be displayed on ImGui
	void RegisterImageSRV(Device& device, const std::shared_ptr<Resource>& resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, GuiSRVIndex srvIndex);

	void FreeImageSRV(GuiDescriptorAllocation alloc);

	GuiDescriptorAllocation GetImageSRVAllocation(GuiSRVIndex index) const;

	bool sb_ShowImGuiWindow = false;
	void ToggleImGuiVisibilityState() { sb_ShowImGuiWindow = !sb_ShowImGuiWindow; }
};

