#include "EditorGui.h"
#include "Device.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "Resource.h"

#include "imgui.h"
#include "implot.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include <wrl/client.h>

namespace {
	constexpr int sk_SRVHeapSize = 64;
}

EditorGui::EditorGui(Device& device, DXGI_FORMAT RTVformat, int bufferCount, HWND hwnd) {

	// Create ImGui SRV Heap and initialize free list allocator
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = sk_SRVHeapSize;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if(FAILED(device.GetD3D12Device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_D3DSrvDescHeap)))) {
			throw std::exception("Failed to create ImGui SRV Heap");
		}

		s_D3DSrvDescHeapAllocator.Initialize(device.GetD3D12Device().Get(), m_D3DSrvDescHeap);
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();

	//ImGuiIO& io = ImGui::GetIO();
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	
	ImGui_ImplDX12_InitInfo init_info = {};
	init_info.Device = device.GetD3D12Device().Get();
	// Current engine implementation only has one direct command queue per device
	init_info.CommandQueue = device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT).GetD3D12CommandQueue().Get();
	init_info.NumFramesInFlight = bufferCount;
	init_info.RTVFormat = RTVformat;

	// Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
	init_info.SrvDescriptorHeap = m_D3DSrvDescHeap.Get();
	
	init_info.SrvDescriptorAllocFn = 
		[](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_CpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* out_GpuHandle) {
			s_D3DSrvDescHeapAllocator.Alloc(out_CpuHandle, out_GpuHandle);
		};

	init_info.SrvDescriptorFreeFn = 
		[](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
			s_D3DSrvDescHeapAllocator.Free(cpuHandle, gpuHandle);
		};

	ImGui_ImplDX12_Init(&init_info);
}

EditorGui::GuiDescriptorAllocation EditorGui::AllocateImageSRV(Device& device, const std::shared_ptr<Resource>& resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc) {
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle {};
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle {};
	s_D3DSrvDescHeapAllocator.Alloc(&cpuHandle, &gpuHandle);
	device.GetD3D12Device()->CreateShaderResourceView(resource->GetD3D12Resource().Get(), srvDesc, cpuHandle);

	return {cpuHandle, gpuHandle};
}

void EditorGui::FreeImageSRV(EditorGui::GuiDescriptorAllocation alloc) {
	s_D3DSrvDescHeapAllocator.Free(alloc.cpuHandle, alloc.gpuHandle);
}

EditorGui::~EditorGui() {
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	ImPlot::DestroyContext();
}

void EditorGui::NewFrame() {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void EditorGui::Render(CommandList& directCommandList) {
	ImGui::Render();
	directCommandList.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_D3DSrvDescHeap.Get());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), directCommandList.GetD3D12CommandList().Get());
}
