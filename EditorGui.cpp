#include "EditorGui.h"
#include "Device.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "GameObject.h"
#include "Scene.h"
#include "Resource.h"
#include "StringHelpers.h"

#include "imgui.h"
#include "implot.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include <wrl/client.h>
#include "Logger.h"

namespace {
	EditorGui* sp_Singleton = nullptr;
	
	const int sk_SRVHeapSize = 64;

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

	DescriptorHeapAllocator s_D3DSrvDescHeapAllocator {};
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> s_D3DSrvDescHeap {};

	// Stores all created GuiDescriptorAllocations created by "AllocateImageSRV()". Indices are enum "GuiSRVIndex".
	// Freed allocations will never be removed since all allocated will always be used in current implementation
	std::vector<EditorGui::GuiDescriptorAllocation> s_ImageSRVs { EditorGui::GuiSRVIndex::NumGuiSRVIndex };
}

EditorGui::EditorGui(Device& device, DXGI_FORMAT RTVformat, int bufferCount, HWND hwnd) {
	// Create ImGui SRV Heap and initialize free list allocator
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = sk_SRVHeapSize;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if(FAILED(device.GetD3D12Device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&s_D3DSrvDescHeap)))) {
			throw std::exception("Failed to create ImGui SRV Heap");
		}

		s_D3DSrvDescHeapAllocator.Initialize(device.GetD3D12Device().Get(), s_D3DSrvDescHeap);
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();

	//ImGuiIO& io = ImGui::GetIO();
	//io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
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
	init_info.SrvDescriptorHeap = s_D3DSrvDescHeap.Get();
	
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

EditorGui& EditorGui::Create(Device& device, DXGI_FORMAT RTVformat, int bufferCount, HWND hwnd) {
	if(!sp_Singleton) {
		sp_Singleton = new EditorGui(device, RTVformat, bufferCount, hwnd);
	}
	return *sp_Singleton;
}

EditorGui& EditorGui::Get() {
	assert(sp_Singleton != nullptr);
	return *sp_Singleton;
}

void EditorGui::Destroy() {
	if(sp_Singleton) {
		delete sp_Singleton;
		sp_Singleton = nullptr;
	}
}

void EditorGui::RegisterImageSRV(Device& device, const std::shared_ptr<Resource>& resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, GuiSRVIndex srvIndex) {
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle {};
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle {};
	s_D3DSrvDescHeapAllocator.Alloc(&cpuHandle, &gpuHandle);
	device.GetD3D12Device()->CreateShaderResourceView(resource->GetD3D12Resource().Get(), srvDesc, cpuHandle);

	if(!(s_ImageSRVs[srvIndex].gpuHandle.ptr == NULL && s_ImageSRVs[srvIndex].cpuHandle.ptr == NULL)) {
		FreeImageSRV(s_ImageSRVs[srvIndex]);
	}

	s_ImageSRVs[srvIndex] = { cpuHandle, gpuHandle };
}

void EditorGui::FreeImageSRV(EditorGui::GuiDescriptorAllocation alloc) {
	s_D3DSrvDescHeapAllocator.Free(alloc.cpuHandle, alloc.gpuHandle);
}

EditorGui::GuiDescriptorAllocation EditorGui::GetImageSRVAllocation(GuiSRVIndex index) const {
	return s_ImageSRVs[index];
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
	directCommandList.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, s_D3DSrvDescHeap.Get());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), directCommandList.GetD3D12CommandList().Get());
}

void EditorGui::RenderObjectInspector(Device& device, const Scene& scene) {
	GameObject* picked = scene.m_Picker->GetPickedObject();
	if(picked == nullptr) return;

	static std::string s_ObjectName {}; // can't be string view, needs null terminated string for ImGui::Text
	static std::wstring_view s_SelectedMat {};
	static float s_ObjTranslation[3] {};
	static float s_ObjEulerAngles[3] {};
	static float s_ObjScale[3] {};
	static float s_UVScale[2] {};
	static float s_HeightMapMagnitude {};
	static float s_ParallaxMagnitude {};
	static bool  s_UseParallaxShadows {};
	static int s_MinParallaxLayers {};
	static int s_MaxParallaxLayers {};
	static GameObject* s_LastPickedObject {};

	// If newly picked object, update variables
	if(picked != s_LastPickedObject) {
		s_ObjectName = std::string { picked->GetName() };
		s_ObjectName += "###ObjectInspector"; // appending this decouples window title and window ID

		s_SelectedMat = picked->m_RenderProps.pbrMatName;

		XMFLOAT3 translation = picked->GetTranslation();
		XMFLOAT3 eulerRotation = picked->GetEulerRotation();
		XMFLOAT3 scale = picked->GetScale();
		memcpy(s_ObjTranslation, &translation, sizeof(float) * 3);
		memcpy(s_ObjEulerAngles, &eulerRotation, sizeof(float) * 3);
		memcpy(s_ObjScale, &scale, sizeof(float) * 3);

		XMFLOAT2 uvScale = picked->m_RenderProps.uvScale;
		memcpy(s_UVScale, &uvScale, sizeof(float) * 2);

		s_HeightMapMagnitude = picked->m_RenderProps.heightMapMagnitude;
		s_ParallaxMagnitude  = picked->m_RenderProps.parallaxMagnitude;
		s_UseParallaxShadows = picked->m_RenderProps.useParallaxShadow;
		s_MinParallaxLayers  = picked->m_RenderProps.minParallaxLayers;
		s_MaxParallaxLayers  = picked->m_RenderProps.maxParallaxLayers;
	}
	s_LastPickedObject = picked;

	static const ImGuiSliderFlags kSliderFlags = ImGuiSliderFlags_AlwaysClamp;

	ImGui::Begin(s_ObjectName.c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
	{
		if(ImGui::BeginTable("PBR Material Table", 4, ImGuiTableFlags_Borders)) {
			for(const auto& s : scene.m_MaterialNames) {
				ImGui::TableNextColumn();

				if(ImGui::Selectable(StringConvert::WideString_To_String(s).c_str(), s == s_SelectedMat)) {
					auto& copyCommandQueue = device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
					auto copyCommandList = copyCommandQueue.GetCommandList();

					picked->UpdatePBRShaderResources(*copyCommandList, s);

					copyCommandQueue.ExecuteCommandList(copyCommandList);
					copyCommandQueue.FlushWait();
					s_SelectedMat = s;
				};
			}
			ImGui::EndTable();
		}

		if(ImGui::DragFloat3("Position", s_ObjTranslation, 0.01f, -1000.0f, 1000.0f, "%.2f", kSliderFlags)) {
			picked->SetTranslation(s_ObjTranslation[0], s_ObjTranslation[1], s_ObjTranslation[2]);
		}

		if(ImGui::DragFloat3("Rotation", s_ObjEulerAngles, 0.01f, -1000.0f, 1000.0f, "%.2f", kSliderFlags)) {
			picked->SetEulerRotation(s_ObjEulerAngles[0], s_ObjEulerAngles[1], s_ObjEulerAngles[2]);
		}

		if(ImGui::DragFloat3("Scale", s_ObjScale, 0.01f, -1000.0f, 1000.0f, "%.2f", kSliderFlags)) {
			picked->SetScale(s_ObjScale[0], s_ObjScale[1], s_ObjScale[2]);
		}
		ImGui::SameLine();
		if(ImGui::Button("+###+Scale")) {
			picked->Scale(1.1f, 1.1f, 1.1f);
		}
		ImGui::SameLine();
		if(ImGui::Button("-###-Scale")) {
			picked->Scale(0.9f, 0.9f, 0.9f);
		}

		if(ImGui::DragFloat2("UV Scale", s_UVScale, 0.01f, 0.0f, 1000.0f, "%.2f", kSliderFlags)) {
			picked->m_RenderProps.uvScale = { s_UVScale[0], s_UVScale[1] };
		}
		ImGui::SameLine();
		if(ImGui::Button("+###+UVScale")) {
		}
		ImGui::SameLine();
		if(ImGui::Button("-###-UVScale")) {
		}

		if(ImGui::DragFloat("Height Map Magnitude", &s_HeightMapMagnitude, 0.01f, 0.0f, 1000.0f, "%.2f", kSliderFlags)) {
			picked->m_RenderProps.heightMapMagnitude = s_HeightMapMagnitude;
		}

		// Parallax Occlusion Mapping
		{
			if(ImGui::DragFloat("Parallax Magnitude", &s_ParallaxMagnitude, 0.001f, 0.0f, 1.0f, "%.3f", kSliderFlags)) {
				picked->m_RenderProps.parallaxMagnitude = s_ParallaxMagnitude;
			}

			if(ImGui::Checkbox("Enable Parallax Self Shadows", &s_UseParallaxShadows)) {
				picked->m_RenderProps.useParallaxShadow = s_UseParallaxShadows;
			}

			if(ImGui::DragInt("Min Parallax Layers", &s_MinParallaxLayers, 1.0f, 0, 100, "%d", kSliderFlags)) {
				picked->m_RenderProps.minParallaxLayers = s_MinParallaxLayers;
			}
			if(ImGui::DragInt("Max Parallax Layers", &s_MaxParallaxLayers, 1.0f, 0, 100, "%d", kSliderFlags)) {
				picked->m_RenderProps.maxParallaxLayers = s_MaxParallaxLayers;
			}
		}

		/// Object Inspector Window End
		ImGui::End();
	}

}
