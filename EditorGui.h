#pragma once

// Wrapper for Dear ImGui Debug UI
// Note: Seperate free list allocator and static SRV descriptor heap used instead of DynamicDescriptorHeap in main engine for simplicity

// Singleton implementation for easy access to RegisterImageSRV() and FreeImageSRV() methods (used to display debug textures), too much dependency injection otherwise. Not static class for iniitialization control.
// Encapsulation will inevitably break for debug UI, any class that needs to be displayed in debug UI will use "friend class EditorGui". This is so there is a normalized way to expose variables and avoids getters/setters/dependecy injection everywhere just for the debug UI.
// This class should eventually support IGame instance switching, but it is currently coupled with DemoGame.h implementation.
// Note that the stored descriptors in this class is only used for ImGui debug textures.

#include "d3d12.h"
#include "dxgiformat.h"
#include <memory>

class Device;
class CommandList;
class Resource;
class Scene;
class DemoGame;

/// Singleton
class EditorGui {
public:
	struct GuiDescriptorAllocation {
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	};

	// This must be manually added to for every type of texture that is shown on debug GUI.
	// Doing this instead of using hardcoded strings, so it's better documented.
	// (Good enough for now)
	enum ImGuiDebugSRVIndex {
		None, // "Null" value to give this enum a default value, useful if storing these enums
		DirectionalShadowMap,
		BloomPrefilter,
		OutlineBloomPrefilter,

		NumGuiSRVIndex
	};

	/// Singleton
	~EditorGui();

	EditorGui() = delete;
	EditorGui(const EditorGui&)			    = delete;
	EditorGui& operator=(const EditorGui&)  = delete;
	EditorGui(EditorGui&&)                  = delete;
	EditorGui& operator=(EditorGui&&)       = delete;

	// Currently called in DemoGame ctor only
	static EditorGui& Initialize(Device& device, DXGI_FORMAT RTVformat, int bufferCount, HWND hwnd);
	static EditorGui& Get();
	static void Destroy();
	///

	// Called at start of frame
	void NewFrame();

	// Called after rendering 3D elements before present, 
	// it is assumed that screen render target is set to pipeline already
	void Render(CommandList& directCommandList);

	void ToggleDebugWindowState() { sb_ShowDebugWindow = !sb_ShowDebugWindow; }

	void SetObjectInspectorState(bool state) { sb_ObjectInspectorState = state; }
	
	// sb_PickerEnabled only exists to make GetUIVisibilityState() return true
	// Cursor visibility is tied to GetUIVisibilityState() and is needed for manual activation of picker
	// Currently only a key press uses this funciton.
	void SetPickerState(bool state) { sb_PickerEnabled = state; }

	bool GetUIVisibilityState() const { return sb_ObjectInspectorState || sb_ShowDebugWindow || sb_PickerEnabled; }

	// Allocate to internal descriptor heap SRV with s_D3DSrvDescHeapAllocator
	// Use this to allocate a SRV to be displayed on ImGui
	void RegisterImageSRV(Device& device, const std::shared_ptr<Resource>& resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, ImGuiDebugSRVIndex srvIndex);
	void FreeImageSRV(GuiDescriptorAllocation alloc);
	GuiDescriptorAllocation GetImageSRVAllocation(ImGuiDebugSRVIndex index) const;

	// Different "IGame"'s will need to write different versions of this function.
	// For game switching support, this function probably has to exist elsewhere.
	void DrawGameDebugUI(Device& device, Scene& scene, const DemoGame& game);
	void DrawObjectInspector(Device& device, const Scene& scene);

private:
	EditorGui(Device& device, DXGI_FORMAT RTVformat, int bufferCount, HWND hwnd);

	bool sb_ShowDebugWindow = false;
	bool sb_ObjectInspectorState = false;
	bool sb_PickerEnabled = false; // See comment on SetPickerState()
};

