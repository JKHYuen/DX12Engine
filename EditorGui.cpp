#include "EditorGui.h"
#include "Device.h"
#include "CommandQueue.h"
#include "CommandList.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include <wrl/client.h>

namespace {
	constexpr int sk_SRVHeapSize = 64;
}

EditorGui::EditorGui(Device& device, DXGI_FORMAT RTVformat, int bufferCount, HWND hwnd) {

	// ImGui SRV Heap with free list allocation
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = sk_SRVHeapSize;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		if(FAILED(device.GetD3D12Device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_D3DSrvDescHeap)))) {
			throw std::exception("Failed to create ImGui SRV Heap");
		}

		s_D3DSrvDescHeapAlloc.Create(device.GetD3D12Device().Get(), m_D3DSrvDescHeap);
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
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
		[](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) {
			s_D3DSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle);
		};

	init_info.SrvDescriptorFreeFn = 
		[](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
			s_D3DSrvDescHeapAlloc.Free(cpu_handle, gpu_handle);
		};

	ImGui_ImplDX12_Init(&init_info);
}

EditorGui::~EditorGui() {
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void EditorGui::NewFrame() {
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	/// TEMP
	ImGui::ShowDemoWindow();

	{
		static float f = 0.0f;
		static int counter = 0;

		ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)

		static char buf1[32] = "";
		ImGui::InputText("default", buf1, IM_ARRAYSIZE(buf1));

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f

		if(ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}
}

void EditorGui::Render(CommandList& directCommandList) {
	ImGui::Render();
	directCommandList.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_D3DSrvDescHeap.Get());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), directCommandList.GetD3D12CommandList().Get());
}