// EditorGui is currently only hardcoded to work with the "DemoGame" class

#include "EditorGui.h"

#include <DX12LibPCH.h>

#include "DX12EngineCore/Application.h"
#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/CommandQueue.h"
#include "DX12EngineCore/Device.h"
#include "DX12EngineCore/Resource.h"
#include "DX12EngineCore/SwapChain.h"

#include "BloomEffect.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "GameObject.h"
#include "PBRGameObject.h"
#include "OutlineEffect.h"
#include "Picker.h"
#include "Scene.h"
#include "Skybox.h"
#include "StringHelpers.h"

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "implot.h"
#include "imGuizmo.h"

// IGame specific
#include "DemoGame.h"

#include <vector>
#include <Logger.h>
#include "PointLight.h"
#include <string>

using namespace DirectX;

namespace {
	EditorGui* sp_Singleton = nullptr;

	constexpr ImGuiSliderFlags kSliderFlags = ImGuiSliderFlags_AlwaysClamp;
	// HDR color picker is WIP in ImGui, color picker disabled since it doesn't support HDR. 
	// We will render preview box manually since the values need to be normalized.
	constexpr ImGuiColorEditFlags kHDRColorEditFlags = ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoSmallPreview;
	
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
	std::vector<EditorGui::GuiDescriptorAllocation> s_ImageSRVs { EditorGui::ImGuiDebugSRVIndex::NumGuiSRVIndex };

	void ImGuiHelpMarker(const char* desc, bool b_IsSameLine = true, bool b_IsWarning = false) {
		if(b_IsSameLine) ImGui::SameLine();
		if(b_IsWarning)  ImGui::TextDisabled("(!)"); else ImGui::TextDisabled("(?)");
		if(ImGui::BeginItemTooltip()) {
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(desc);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	};

	void ImGuiHDRColorEdit3Preview(std::string_view s, float col[3], ImGuiColorEditFlags flags) {
		ImGui::SameLine();
		// Normalize HDR values to estimate of color for preview box
		float colMax = std::max(col[0], std::max(col[1], col[2]));
		ImVec4 buttonCol(col[0] / colMax, col[1] / colMax, col[2] / colMax, 1.0f);
		ImGui::ColorButton(s.data(), buttonCol, flags);
		ImGui::SameLine();
		ImGui::TextDisabled("HDR");
	}

	// Not technically compatible with operations that aren't translate, rotate and/or scale, i.e. other ImGuizmo::OPERATION's
	// All params prefixed "s_" is static to object inspector and are out values
	// C arrays are used for easy use with ImGui
	void GameObjectTransform(const Scene& scene, GameObject* go, ImGuiSliderFlags kSliderFlags, float s_GizmoSRTMat[16], float s_ObjTranslation[3], float s_ObjDegreeEulerAngles[3], float s_ObjScale[3], ImGuizmo::OPERATION enabledOperationFlags = ImGuizmo::TRANSLATE | ImGuizmo::ROTATE | ImGuizmo::SCALE) {
		ImGui::SeparatorText("Transform");

		static XMFLOAT4X4 s_CamViewMat {};
		static XMFLOAT4X4 s_CamProjMat {};
		XMStoreFloat4x4(&s_CamViewMat, scene.GetMainCamera().Get_ViewMatrix());
		XMStoreFloat4x4(&s_CamProjMat, scene.GetMainCamera().Get_ProjectionMatrix());

		static ImGuizmo::OPERATION s_CurrentGizmoOperation(ImGuizmo::TRANSLATE);

		bool enableTranslate = (enabledOperationFlags & ImGuizmo::TRANSLATE) == ImGuizmo::TRANSLATE;
		bool enableRotate = (enabledOperationFlags & ImGuizmo::ROTATE) == ImGuizmo::ROTATE;
		bool enableScale = (enabledOperationFlags & ImGuizmo::SCALE) == ImGuizmo::SCALE;

		// If current operation mode (from a previous call) is not enabled for the current function call,
		// use the first one that is enabled (arbitrary order below).
		if((enabledOperationFlags & s_CurrentGizmoOperation) == 0) {
			if(enableTranslate) s_CurrentGizmoOperation = ImGuizmo::TRANSLATE;
			else if(enableRotate) s_CurrentGizmoOperation = ImGuizmo::ROTATE;
			else if(enableScale) s_CurrentGizmoOperation = ImGuizmo::SCALE;
		}

		// Use ImGui key detection instead of this engine's key detection to keep things encapsulated 
		if(ImGui::IsKeyPressed(ImGuiKey_T) && enableTranslate)
			s_CurrentGizmoOperation = ImGuizmo::TRANSLATE;
		if(ImGui::IsKeyPressed(ImGuiKey_R) && enableRotate)
			s_CurrentGizmoOperation = ImGuizmo::ROTATE;
		if(ImGui::IsKeyPressed(ImGuiKey_F) && enableScale)
			s_CurrentGizmoOperation = ImGuizmo::SCALE;

		ImGui::AlignTextToFramePadding();
		ImGuiHelpMarker("On-screen transform gizmo mode.\nNote: Mouse drag is less sensitive as scale values approach zero.\n\nKey shortcuts:\nT: Translate\nR: Rotation\nF: Scale", false); ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::Text("Gizmo:");
		ImGui::SameLine();
		if(enableTranslate) {
			if(ImGui::RadioButton("Translate", s_CurrentGizmoOperation == ImGuizmo::TRANSLATE))
				s_CurrentGizmoOperation = ImGuizmo::TRANSLATE;
		}
		if(enableRotate) {
			ImGui::SameLine();
			if(ImGui::RadioButton("Rotate", s_CurrentGizmoOperation == ImGuizmo::ROTATE))
				s_CurrentGizmoOperation = ImGuizmo::ROTATE;
		}
		if(enableScale) {
			ImGui::SameLine();
			if(ImGui::RadioButton("Scale", s_CurrentGizmoOperation == ImGuizmo::SCALE))
				s_CurrentGizmoOperation = ImGuizmo::SCALE;
		}

		// Update s_GizmoSRTMat so gizmo translation and rotation is correct if values were changed
		ImGuizmo::RecomposeMatrixFromComponents(s_ObjTranslation, s_ObjDegreeEulerAngles, s_ObjScale, s_GizmoSRTMat);

		static float s_GizmoDeltaMat[16] {};
		ImGuiIO& io = ImGui::GetIO();
		ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
		if(ImGuizmo::Manipulate(*s_CamViewMat.m, *s_CamProjMat.m, s_CurrentGizmoOperation, ImGuizmo::WORLD, s_GizmoSRTMat, s_GizmoDeltaMat, NULL)) {
			// Note: gizmo currently only supports world space, world space rotations require special care using values in s_GizmoDeltaMat, 
			//       translation and scale are updated by decomposing s_GizmoSRTMat values for convenience since we need to update 
			//       s_ObjTranslation and s_ObjScale anyways
			static float rtDelta[3], dummyVec[3];
			ImGuizmo::DecomposeMatrixToComponents(s_GizmoDeltaMat, dummyVec, rtDelta, dummyVec);
			ImGuizmo::DecomposeMatrixToComponents(s_GizmoSRTMat, s_ObjTranslation, dummyVec, s_ObjScale);

			go->SetScale(s_ObjScale[0], s_ObjScale[1], s_ObjScale[2]);
			go->SetTranslation(s_ObjTranslation[0], s_ObjTranslation[1], s_ObjTranslation[2]);

			// Note: "201" order of rotation indices to convert ImGuizmo rotation order to DirectX
			go->QuatRotate(XMQuaternionRotationRollPitchYaw(
				XMConvertToRadians(rtDelta[2]), XMConvertToRadians(rtDelta[0]), XMConvertToRadians(rtDelta[1]))
			);
			XMFLOAT3 degreeEulerRotation = go->GetEulerRotation();
			degreeEulerRotation.x = XMConvertToDegrees(degreeEulerRotation.x);
			degreeEulerRotation.y = XMConvertToDegrees(degreeEulerRotation.y);
			degreeEulerRotation.z = XMConvertToDegrees(degreeEulerRotation.z);
			// Convert values from [-180, 180] range from ImGuizmo to [0, 360] to match DragFloat3 below
			// Need to manually update s_ObjDegreeEulerAngles since we're not using decomposition of s_GizmoSRTMat for rotations
			s_ObjDegreeEulerAngles[0] = degreeEulerRotation.x + (degreeEulerRotation.x < 0.0f ? 360.0f : 0.0f);
			s_ObjDegreeEulerAngles[1] = degreeEulerRotation.y + (degreeEulerRotation.y < 0.0f ? 360.0f : 0.0f);
			s_ObjDegreeEulerAngles[2] = degreeEulerRotation.z + (degreeEulerRotation.z < 0.0f ? 360.0f : 0.0f);
		}

		if(enableTranslate) {
			if(ImGui::DragFloat3("Position", s_ObjTranslation, 0.01f, -FLT_MAX, FLT_MAX, "%.2f", kSliderFlags)) {
				go->SetTranslation(s_ObjTranslation[0], s_ObjTranslation[1], s_ObjTranslation[2]);
			}
		}
		if(enableRotate) {
			// Local rotation
			if(ImGui::DragFloat3("Rotation", s_ObjDegreeEulerAngles, 0.1f, 0.0f, 360.0f, "%.2f", ImGuiSliderFlags_WrapAround)) {
				go->SetEulerRotation(XMConvertToRadians(s_ObjDegreeEulerAngles[0]), XMConvertToRadians(s_ObjDegreeEulerAngles[1]), XMConvertToRadians(s_ObjDegreeEulerAngles[2]));
			}
			ImGuiHelpMarker("Local euler rotation (degrees), use rotation gizmo for world rotation.");
		}
		if(enableScale) {
			if(ImGui::DragFloat3("Scale", s_ObjScale, 0.01f, -FLT_MAX, FLT_MAX, "%.2f", kSliderFlags)) {
				go->SetScale(s_ObjScale[0], s_ObjScale[1], s_ObjScale[2]);
			}
			ImGui::SameLine();
			/// Extremely hacky way to add a uniform scale drag, can't think of another way right now
			ImGui::PushItemWidth(30);
			static float _ {};
			static float lastMousePos {};
			if(ImGui::DragFloat("##ScaleDrag", &_, 0.001f, 0.0f, 1.0f, "<->", ImGuiSliderFlags_WrapAround)) {
				if(ImGui::GetMousePos().x > lastMousePos) {
					go->Scale(1.05f, 1.05f, 1.05f);
				}
				else {
					go->Scale(0.95f, 0.95f, 0.95f);
				}
				XMFLOAT3 scale = go->GetScale();
				memcpy(s_ObjScale, &scale, sizeof(float) * 3);

				lastMousePos = ImGui::GetMousePos().x;
			}
			ImGui::PopItemWidth();
		}
	}
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

	// Allocating SRV descriptors (for debug textures) 
	// This uses a different allocataor than the 3D render engine in this project
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

	/// TODO: test this on 1080p monitor
	//// Source: https://github.com/ocornut/imgui/blob/master/examples/example_win32_directx11/main.cpp#L34
	ImGui_ImplWin32_EnableDpiAwareness();
	// Arbitrary multiplier added
	float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT { 0, 0 }, MONITOR_DEFAULTTOPRIMARY)) * 0.9f; 
	ImGuiStyle& style = ImGui::GetStyle();
	// Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.ScaleAllSizes(main_scale);     
	//style.FontScaleMain = main_scale;
	// Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
	style.FontScaleDpi = main_scale;

	// Custom Theme
	style.WindowMinSize     = ImVec2(160, 20);
	style.FramePadding      = ImVec2(4, 5);
	style.ItemSpacing       = ImVec2(7, 5);
	style.ItemInnerSpacing  = ImVec2(7, 4);
	style.WindowRounding    = 5.0f;
	style.FrameRounding     = 2.0f;
	style.IndentSpacing     = 6.0f;
	style.ItemInnerSpacing  = ImVec2(4, 4);
	style.ColumnsMinSpacing = 50.0f;
	style.ScrollbarSize     = 12.0f;
	style.ScrollbarRounding = 16.0f;
	style.WindowBorderSize  = 0.0f;

	ImVec4 accentColor1   = ImVec4(0.1f, 0.13f, 0.2f, 1.0f);
	ImVec4 accentColor1_h = ImVec4(0.12f, 0.15f, 0.22f, 1.0f);
	ImVec4 accentColor3   = ImVec4(0.15f, 0.18f, 0.25f, 1.0f);
	ImVec4 accentColor3_h = ImVec4(0.17f, 0.20f, 0.27f, 1.0f);
	//style.Colors[ImGuiCol_WindowBg]             = accentColor1;
	//style.Colors[ImGuiCol_Border]               = ImVec4(0.31f, 0.31f, 1.00f, 0.00f);
	//style.Colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg]              = ImVec4(0.1f, 0.1f, 0.11f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered]       = accentColor1_h;
	style.Colors[ImGuiCol_FrameBgActive]        = accentColor1;
	//style.Colors[ImGuiCol_TitleBg]              = accentColor1_h;
	//style.Colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.20f, 0.22f, 0.27f, 0.75f);
	style.Colors[ImGuiCol_TitleBgActive]        = accentColor1;
	//style.Colors[ImGuiCol_MenuBarBg]            = ImVec4(0.20f, 0.22f, 0.27f, 0.47f);
	//style.Colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.20f, 0.22f, 0.27f, 1.00f);
	//style.Colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.09f, 0.15f, 0.16f, 1.00f);
	//style.Colors[ImGuiCol_ScrollbarGrabHovered] = accentColor1_h;
	//style.Colors[ImGuiCol_ScrollbarGrabActive]  = accentColor1;
	//style.Colors[ImGuiCol_CheckMark]            = accentColor1;
	style.Colors[ImGuiCol_SliderGrab]            = accentColor3;
	//style.Colors[ImGuiCol_SliderGrabActive]      = accentColor3;
	//style.Colors[ImGuiCol_Button]               = ImVec4(0.47f, 0.77f, 0.83f, 0.14f);
	//style.Colors[ImGuiCol_ButtonHovered]        = accentColor1;
	//style.Colors[ImGuiCol_ButtonActive]         = accentColor1;
	style.Colors[ImGuiCol_Header]               = accentColor3;
	style.Colors[ImGuiCol_HeaderHovered]        = accentColor3_h;
	style.Colors[ImGuiCol_HeaderActive]         = accentColor3_h;
	//style.Colors[ImGuiCol_Separator]            = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
	style.Colors[ImGuiCol_SeparatorHovered]     = accentColor1_h;
	style.Colors[ImGuiCol_SeparatorActive]      = accentColor1;
	//style.Colors[ImGuiCol_ResizeGrip]           = ImVec4(0.47f, 0.77f, 0.83f, 0.04f);
	style.Colors[ImGuiCol_ResizeGripHovered]    = accentColor1_h;
	style.Colors[ImGuiCol_ResizeGripActive]     = accentColor1;
	//style.Colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.92f, 0.18f, 0.29f, 0.43f);
	//style.Colors[ImGuiCol_PopupBg]              = ImVec4(0.20f, 0.22f, 0.27f, 0.9f);
}

EditorGui& EditorGui::Initialize(Device& device, DXGI_FORMAT RTVformat, int bufferCount, HWND hwnd) {
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

void EditorGui::RegisterImageSRV(Device& device, const std::shared_ptr<Resource>& resource, const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, ImGuiDebugSRVIndex srvIndex) {
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle {};
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle {};
	s_D3DSrvDescHeapAllocator.Alloc(&cpuHandle, &gpuHandle);
	device.GetD3D12Device()->CreateShaderResourceView(resource->GetD3D12Resource().Get(), srvDesc, cpuHandle);

	// If allocation for this slot already exists, free the existing SRV from descriptor heap
	if(!(s_ImageSRVs[srvIndex].gpuHandle.ptr == NULL && s_ImageSRVs[srvIndex].cpuHandle.ptr == NULL)) {
		FreeImageSRV(s_ImageSRVs[srvIndex]);
	}

	s_ImageSRVs[srvIndex] = { cpuHandle, gpuHandle };
}

void EditorGui::FreeImageSRV(EditorGui::GuiDescriptorAllocation alloc) {
	s_D3DSrvDescHeapAllocator.Free(alloc.cpuHandle, alloc.gpuHandle);
}

EditorGui::GuiDescriptorAllocation EditorGui::GetImageSRVAllocation(ImGuiDebugSRVIndex srvIndex) const {
	return s_ImageSRVs[srvIndex];
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
	ImGuizmo::BeginFrame();
}

void EditorGui::Render(CommandList& directCommandList) {
	ImGui::Render();
	directCommandList.SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, s_D3DSrvDescHeap.Get());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), directCommandList.GetD3D12CommandList().Get());
}

void EditorGui::DrawGameDebugUI(Device& device, Scene& scene, const DemoGame& game) {
	struct ScrollingBuffer {
		int MaxSize;
		int Offset;

		// These are kept seperate for easier use with ImPlot
		std::vector<float> tData;
		std::vector<float> frameTimeData;

		ScrollingBuffer(int max_size = 2000) {
			MaxSize = max_size;
			Offset = 0;
			tData.reserve(MaxSize);
			frameTimeData.reserve(MaxSize);
		}

		void AddPoint(float x, float y) {
			if(tData.size() < MaxSize) {
				tData.push_back(x);
				frameTimeData.push_back(y);
			}
			else {
				tData[Offset] = x;
				frameTimeData[Offset] = y;
				Offset = (Offset + 1) % MaxSize;
			}
		}
	};

	if(sb_ShowDebugWindow) {
		// b_CurrentDebugWindowState added only for x button to work
		bool b_CurrentDebugWindowState = sb_ShowDebugWindow;
		ImGui::Begin("DX12 Engine", &b_CurrentDebugWindowState, ImGuiWindowFlags_NoCollapse);
		sb_ShowDebugWindow = b_CurrentDebugWindowState;

		// Exit button / Font scaler
		{
			ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
			if(ImGui::Button("EXIT APP")) {
				Application::Get().Quit();
			}
			ImGui::PopStyleColor(3);

			ImGui::SameLine();
			ImGuiStyle& style = ImGui::GetStyle();
			ImGui::DragFloat("UI Scale", &style.FontScaleDpi, 0.02f, 0.5f, 4.0f, "%.2fx");

			ImGui::Spacing();
			if(ImGui::TreeNode("Controls")) {
				ImGui::Text(
"\
- Click and drag to change numeric values, double click to type in values\n\
- Hover mouse over (?) or (!) icons for additional information\n\
- Cursor and movement is disabled when menus are open\n\
- Hold RIGHT CLICK when menus are open to reenable camera look and movement\n\
- LEFT CLICK objects with menus open to enable object inspector\n\
  Note: click raycasts/AABBs currently do not account for height maps\n\
- Hold LALT to enable cursor to click on objects without menus\n\n\
   ESC: (First press) Unlock cursor\n\
        (Second press) Quit app\n\
    F1: Toggle this menu\n\
    F2: Toggle wireframe render mode\n\
   F11: Toggle fullscreen\n\
     V: Toggle Vsync\n\
     X: Deselect clicked object\n\
  WASD: Camera movement\n\
    QE: Move camera up/down\n\
LSHIFT: Move fast\n\
 LCTRL: Move slow\n\
"
				);
				ImGui::TreePop();
			}
			ImGui::Spacing();
		}

		if(ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::TextDisabled("Resolution: %d x %d", game.GetWindowWidth(), game.GetWindowHeight());
			const DXGI_ADAPTER_DESC3& adapterDesc = device.GetAdapterDesc();
			ImGui::TextDisabled("%s %d MB", device.GetAdapterName().c_str(), adapterDesc.DedicatedVideoMemory / 1024 / 1024);
			ImGui::TextDisabled("Shared Memory %d MB", adapterDesc.SharedSystemMemory / 1024 / 1024);
			DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo = device.GetVRAMUsed();
			ImGui::Text("VRAM: %d MB / %d MB", videoMemoryInfo.CurrentUsage / 1024 / 1024, videoMemoryInfo.Budget / 1024 / 1024);
			if(game.m_SwapChain->GetVSync()) {
				ImGui::Text("VSync: ON");
			}
			else {
				ImGui::Text("VSync: OFF");
			}

			// Performance Graph 
			// Graph data update rate based on s_GraphUpdateRate, default: 60hz
			// This is to throttle the rate ScrollingBuffer records data so we don't need a huge buffer for high frame rates over a big time scale
			{
				static ScrollingBuffer s_FPSGraphBuffer {};

				// Save axis extents for update ticks faster than s_GraphUpdateRate
				static std::pair xCurrentAxisExtents = { 0.0, 1.0 };
				static std::pair yCurrentAxisExtents = { 0.0, 1.0 };

				// 60 times a second is probably overkill, note that time scale at 1 second doesn't render properly at 30 times a second
				static const float s_GraphUpdateRate = 1.0f / 60.0f;
				static float s_UpdateTimer = s_GraphUpdateRate;
				s_UpdateTimer -= ImGui::GetIO().DeltaTime;
				if(s_UpdateTimer <= 0) {
					s_FPSGraphBuffer.AddPoint((float)ImGui::GetTime(), (float)game.m_CurrentAvgFPS);
					s_UpdateTimer = s_GraphUpdateRate;
				}

				ImGuiHelpMarker("Average of last 128 frames.", false);
				ImGui::SameLine();
				ImGui::Text("FPS: %d", game.m_CurrentAvgFPS);
				ImGui::Text("    %.0f ms", (1.0f / game.m_CurrentAvgFPS) * 1000.0f);

				static int timeScale = 5;
				if(ImPlot::BeginPlot("##FPS Graph", ImVec2(-1, 100), ImPlotFlags_NoFrame | ImPlotFlags_NoLegend | ImPlotFlags_NoInputs)) {
					ImPlot::SetupAxes(
						nullptr, nullptr,
						// x axis flags
						ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_Lock,
						// y axis flags
						ImPlotAxisFlags_LockMin
					);

					// Update axis extents
					// We don't use ImPlot autofit because it doesn't leave headroom
					if(s_UpdateTimer == s_GraphUpdateRate) {
						double newXMin = ImGui::GetTime() - timeScale;
						double newXMax = ImGui::GetTime();
						ImPlot::SetupAxisLimits(ImAxis_X1, newXMin, newXMax, ImGuiCond_Always);
						xCurrentAxisExtents.first = newXMin;
						xCurrentAxisExtents.second = newXMax;

						if(game.m_CurrentAvgFPS >= yCurrentAxisExtents.second || game.m_CurrentAvgFPS * 2.0f <= yCurrentAxisExtents.second) {
							float newYMax = std::max(120.0f, game.m_CurrentAvgFPS * 1.5f);
							ImPlot::SetupAxisLimits(ImAxis_Y1, 0, newYMax, ImGuiCond_Always);
							yCurrentAxisExtents.second = newYMax;
						}
					}
					else {
						ImPlot::SetupAxisLimits(ImAxis_X1, xCurrentAxisExtents.first, xCurrentAxisExtents.second, ImGuiCond_Always);
						ImPlot::SetupAxisLimits(ImAxis_Y1, 0, yCurrentAxisExtents.second, ImGuiCond_Always);
					}

					static ImPlotSpec s_Spec {};
					s_Spec.Offset = s_FPSGraphBuffer.Offset;
					ImPlot::PlotLine("FPS", s_FPSGraphBuffer.tData.data(), s_FPSGraphBuffer.frameTimeData.data(), (int)s_FPSGraphBuffer.tData.size(), s_Spec);
					ImPlot::EndPlot();

					ImGui::SliderInt("Time Scale", &timeScale, 1, 30, "%ds");
				}
			}
		}

		if(ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Wireframe Render Mode",  &scene.mb_WireframeRender);

			ImGui::AlignTextToFramePadding();
			ImGuiHelpMarker("Axis aligned bounding boxes (AABBs) account for rotations by rebuilding a new AABB that encompasses the old one. This creates a margin of error (amount depends on geometry), but is guaranteed to encompass the object and is much more efficient than recalculating from every vertex of the mesh. However, it does not currently account for height maps.\n\nAABBs are used for mouse object picking and frustum culling.", false); ImGui::SameLine();
			static int aabbRadioIdx = 0;
			ImGui::AlignTextToFramePadding();
			ImGui::Text("AABB Render Mode:"); ImGui::SameLine();
			ImGui::RadioButton("Off", &aabbRadioIdx, 0); ImGui::SameLine();
			ImGui::RadioButton("All", &aabbRadioIdx, 1); ImGui::SameLine();
			ImGui::RadioButton("Picked Only", &aabbRadioIdx, 2);
			scene.SetAABBRenderMode((Scene::AABBRenderMode)aabbRadioIdx);

			// FOV Slider
			static float s_FOV = scene.GetMainCamera().Get_FoV();
			if(ImGui::SliderFloat("Vertical FOV", &s_FOV, 12.0f, 90.0f)) {
				scene.GetMainCamera().Set_FoV(s_FOV);
			}
		}
		
		if(ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
			// Skybox Selector
			ImGui::SetNextItemOpen(true, ImGuiTreeNodeFlags_DefaultOpen);
			if(ImGui::TreeNode("Skybox")) {
				static std::wstring_view s_SelectedSkybox = scene.m_Skybox->GetTextureName();
				if(ImGui::BeginTable("Skybox Table", 3, ImGuiTableFlags_Borders)) {
					for(auto& s : game.m_SkyboxNames) {
						ImGui::TableNextColumn();

						static std::string fileName {};
						StringConvert::WideString_To_String(s, fileName);
						if(ImGui::Selectable(fileName.c_str(), s == s_SelectedSkybox)) {
							// Load new skybox cubemap
							auto& copyCommandQueue = device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
							auto copyCommandList = copyCommandQueue.GetCommandList();
							auto& computeCommandQueue = device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
							auto computeCommandList = computeCommandQueue.GetCommandList();

							scene.SetSkybox(*copyCommandList, *computeCommandList, s);
							copyCommandQueue.WaitForFenceValue(copyCommandQueue.ExecuteCommandList(copyCommandList));
							computeCommandQueue.WaitForFenceValue(computeCommandQueue.ExecuteCommandList(computeCommandList));

							// Render new IBLs
							auto& directCommandQueue = device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
							auto directCommandList = directCommandQueue.GetCommandList();
							directCommandList->SetScissorRect(game.m_DefaultScissorRect);
							scene.ComputeSkyboxIBLs(*directCommandList);
							directCommandQueue.ExecuteCommandList(directCommandList);

							s_SelectedSkybox = s;
						};

					}
					ImGui::EndTable();
				}
				ImGui::TreePop();

				ImGui::Spacing();
			}

			ImGui::SetNextItemOpen(true, ImGuiTreeNodeFlags_DefaultOpen);
			if(ImGui::TreeNode("Directional Light")) {
				static DirectionalLight& sceneLight = *(scene.m_DirectionalLight);
				static float s_SceneDirLightEulerAngle[2];
				static float s_SceneDirLightColor[3];
				static float s_ShadowNearFarZ[2];
				static float s_ShadowRenderDistance;

				static bool sb_DirInit = false;
				if(!sb_DirInit) {
					sb_DirInit = true;

					XMFLOAT3 startingDirAngles = sceneLight.GetEulerAngles();
					memcpy(s_SceneDirLightEulerAngle, &startingDirAngles, sizeof(float) * 2);

					XMFLOAT4 startingSceneLightColor = sceneLight.GetColor();
					memcpy(s_SceneDirLightColor, &startingSceneLightColor, sizeof(float) * 3);

					XMFLOAT2 nearFarZ = sceneLight.GetShadowNearFarZ();
					memcpy(s_ShadowNearFarZ, &nearFarZ, sizeof(float) * 2);

					s_ShadowRenderDistance = sceneLight.GetShadowRenderDistance();
				}

				if(ImGui::DragFloat2("Direction", s_SceneDirLightEulerAngle, 0.1f, 0.0f, 360.0f, "%.2f", kSliderFlags | ImGuiSliderFlags_WrapAround)) {
					sceneLight.SetEulerAngles(s_SceneDirLightEulerAngle[0], s_SceneDirLightEulerAngle[1], 0.0f);
				}

				if(ImGui::ColorEdit3("Light Color", s_SceneDirLightColor, kHDRColorEditFlags)) {
					sceneLight.SetColor(s_SceneDirLightColor[0], s_SceneDirLightColor[1], s_SceneDirLightColor[2]);
				}
				ImGuiHDRColorEdit3Preview("##LightColor", s_SceneDirLightColor, kHDRColorEditFlags);

				if(ImGui::DragFloat2("Shadow Near/Far Z", s_ShadowNearFarZ, 0.1f, 0.1f, 10000.0f, "%.2f", kSliderFlags)) {
					sceneLight.SetShadowNearFarZ({ s_ShadowNearFarZ[0], s_ShadowNearFarZ[1] });
				}

				if(ImGui::DragFloat("Shadow Render Distance", &s_ShadowRenderDistance, 0.1f, 0.1f, 10000.0f, "%.2f", kSliderFlags)) {
					sceneLight.SetShadowRenderDistance(s_ShadowRenderDistance);
				}

				// Shadow debug
				if(ImGui::TreeNode("Shadow Debug")) {
					static float s_ImageScale = 0.25f;
					ImGui::SliderFloat("##DirectionalShadowMapTextureScale", &s_ImageScale, 0.0, 1.0, "%.2fx");
					ImVec2 imageSize = ImVec2(1920.0f * s_ImageScale, 1080.0f * s_ImageScale);

					// Directional shadow map debug view
					ImGui::Image(
						(ImTextureID)GetImageSRVAllocation(EditorGui::ImGuiDebugSRVIndex::DirectionalShadowMap).gpuHandle.ptr,
						imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f)
					);
					ImGui::TreePop();
				}

				ImGui::TreePop();
			}
		}

		// Bloom
		if(ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
			static BloomEffect* bloomPass = game.m_BloomEffect.get();

			ImGui::DragFloat("Intensity", &bloomPass->m_Intensity, 0.01f, 0.0f, 10.0f, "%.2f", kSliderFlags);
			ImGui::DragFloat("Theshold", &bloomPass->m_Threshold, 0.01f, 0.0f, 100.0f, "%.2f", kSliderFlags);
			ImGui::DragFloat("Soft Theshold", &bloomPass->m_SoftThreshold, 0.01f, 0.0f, 100.0f, "%.2f", kSliderFlags);

			// Bloom debug view
			// Need ImVec4(0.0f, 0.0f, 0.0f, 1.0f) background color, else some textures are see through for some reason
			// Might have something to do with alpha blending when ImGui theme has transparency
			if(ImGui::TreeNode("Prefilter Debug")) {
				static float imageScale = 0.25f;
				ImGui::SliderFloat("##Bloom Texture Scale", &imageScale, 0.0, 1.0, "%.2fx");
				ImVec2 imageSize = ImVec2(1920.0f * imageScale, 1080.0f * imageScale);

				ImGui::Image(
					(ImTextureID)GetImageSRVAllocation(EditorGui::ImGuiDebugSRVIndex::BloomPrefilter).gpuHandle.ptr,
					imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f)
				);
				ImGui::TreePop();
			}
		}

		if(ImGui::CollapsingHeader("Picker Outline ", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SameLine();
			ImGuiHelpMarker("Bloom based outline effect for picked (clicked) objects. This post processing method gives geometry independent outlines that are thicker when far away and thinner when closer to the camera, which are desired properties for a scene editor effect.\n\nThis effect does not have the downsides of other common methods like inverted hull; which struggle with sharp angles and back culled quads. \n\nHowever, it does cost an additional screen space texture, one half resolution texture, one quarter resolution texture, and extra draw calls. The quality of the bloom effect can be adjusted to lower the additional cost.");

			static BloomEffect* pickerBloomEffect = game.m_OutlineEffect->m_BloomEffect.get();
			static float s_OutlineCol[3];
			static bool b_PickerInit = false;
			if(!b_PickerInit) {
				b_PickerInit = true;
				memcpy(s_OutlineCol, &pickerBloomEffect->m_ColorMultiply, sizeof(float) * 3);
			}

			ImGui::Checkbox("Disable Outline", &game.m_OutlineEffect->mb_DisableEffect);

			if(ImGui::ColorEdit3("Outline Color", s_OutlineCol, kHDRColorEditFlags)) {
				pickerBloomEffect->SetColorMultiply(s_OutlineCol[0], s_OutlineCol[1], s_OutlineCol[2]);
			}
			ImGuiHDRColorEdit3Preview("##OutlineColor", s_OutlineCol, kHDRColorEditFlags);

			ImGui::DragFloat("Intensity##Picker", &pickerBloomEffect->m_Intensity, 0.01f, 0.0f, 10.0f, "%.2f", kSliderFlags);
			ImGui::DragFloat("Threshold##Picker", &pickerBloomEffect->m_Threshold, 0.01f, 0.0f, 100.0f, "%.2f", kSliderFlags);
			ImGui::DragFloat("Soft Theshold##Picker", &pickerBloomEffect->m_SoftThreshold, 0.01f, 0.0f, 100.0f, "%.2f", kSliderFlags);
		}

		/// Main Engine UI Window End
		ImGui::End();
	}
}

// Note: All GameObject inspector code is inlined in this function right now, this will be impractical with more types of game objects, it's ok for now for convenience
void EditorGui::DrawObjectInspector(Device& device, const Scene& scene) {
	if(!sb_ObjectInspectorState) return;

	GameObject* pickedGameObject = scene.m_Picker->m_PickedObject;
	if(pickedGameObject == nullptr) return;

	/// Note: Following code is very boiler plate. Some values need preprocessing before use in ImGui.
	///       ImGui needs C arrays which requires extra work, single numerical values rely on friend class access. (not ideal)
	// GameObject (base class) vars 
	static std::string s_ObjectName {}; // can't be string view, needs null terminated string for ImGui::Text
	static std::wstring_view s_SelectedMat {};
	static float s_ObjTranslation[3] {};
	static float s_ObjDegreeEulerAngles[3] {};
	static float s_ObjScale[3] {};
	static float s_GizmoSRTMat[16] {};

	// PBRGameObject vars
	static float s_UVScale[2] {};
	static int s_TessRadioIdx = 0;

	// PointLight vars
	static float s_PointLightColor[3] {};

	static GameObject* s_LastPickedObject {};

	PointLight* pickedPointLight = dynamic_cast<PointLight*>(scene.m_Picker->m_PickedObject);
	PBRGameObject* pickedPBRGameObject = dynamic_cast<PBRGameObject*>(scene.m_Picker->m_PickedObject);

	// If newly picked object, update variables
	if(pickedGameObject != s_LastPickedObject) {
		// GameObject (base class) vars 
		{
			s_ObjectName = std::string{ pickedGameObject->GetName()};
			s_ObjectName += "##ObjectInspector"; // appending this decouples window title and window ID

			// Transform
			XMFLOAT3 translation = pickedGameObject->GetTranslation();
			XMFLOAT3 degreeEulerRotation = pickedGameObject->GetEulerRotation();
			degreeEulerRotation.x = XMConvertToDegrees(degreeEulerRotation.x);
			degreeEulerRotation.y = XMConvertToDegrees(degreeEulerRotation.y);
			degreeEulerRotation.z = XMConvertToDegrees(degreeEulerRotation.z);
			XMFLOAT3 scale = pickedGameObject->GetScale();
			memcpy(s_ObjTranslation, &translation, sizeof(float) * 3);
			memcpy(s_ObjDegreeEulerAngles, &degreeEulerRotation, sizeof(float) * 3);
			memcpy(s_ObjScale, &scale, sizeof(float) * 3);

			ImGuizmo::RecomposeMatrixFromComponents(s_ObjTranslation, s_ObjDegreeEulerAngles, s_ObjScale, s_GizmoSRTMat);
		}

		// PBRGameObject vars
		if(pickedPBRGameObject != nullptr) {
			s_SelectedMat = pickedPBRGameObject->m_RenderProps.pbrMatName;
			XMFLOAT2 uvScale = pickedPBRGameObject->m_RenderProps.uvScale;
			memcpy(s_UVScale, &uvScale, sizeof(float) * 2);

			if(((pickedPBRGameObject->m_RenderProps.tessellationModeFlag) & RenderFlags_UniformTessellation) != 0) {
				s_TessRadioIdx = 1;
			}
			else if(((pickedPBRGameObject->m_RenderProps.tessellationModeFlag) & RenderFlags_EdgeTessellation) != 0) {
				s_TessRadioIdx = 2;
			}
			else {
				s_TessRadioIdx = 0;
			}
		}

		// PointLight vars
		if(pickedPointLight) {
			XMFLOAT3 color = pickedPointLight->GetColor();
			memcpy(s_PointLightColor, &color, sizeof(float) * 3);
		}

	}
	s_LastPickedObject = pickedGameObject;

	// Default object inspector location
	ImGui::SetNextWindowPos({ (float)scene.GetWindowWidth() * 0.8f, (float)scene.GetWindowHeight() * 0.05f }, ImGuiCond_Once);
	ImGui::SetNextWindowSize({450.0f, 0.0f}, ImGuiCond_Once);

	// sb_ObjectInspectorState var is just to make inspector window x button work
	ImGui::Begin(s_ObjectName.c_str(), &sb_ObjectInspectorState, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
	if(!sb_ObjectInspectorState) {
		scene.m_Picker->ClearPickedObject();
		ImGui::End();
		return;
	}
	
	/// Object inspector if picked object is a point light
	if(pickedPointLight != nullptr) {
		GameObjectTransform(scene, pickedPointLight, kSliderFlags, s_GizmoSRTMat, s_ObjTranslation, s_ObjDegreeEulerAngles, s_ObjScale, ImGuizmo::TRANSLATE);

		if(ImGui::ColorEdit3("Light Color##PointLight", s_PointLightColor, kHDRColorEditFlags)) {
			pickedPointLight->SetColor(s_PointLightColor[0], s_PointLightColor[1], s_PointLightColor[2]);
		}
		ImGui::DragFloat("Radius##PointLight", &pickedPointLight->m_Radius, 0.1f, 0.0f, FLT_MAX, "%.1f", kSliderFlags);

		ImGui::SeparatorText("Visualization (?)");
		if(ImGui::BeginItemTooltip()) {
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted("Glowing spheres are not real scene objects, they serve as a visual indication of where point lights are located during scene editing. The following values edit these visualization objects.");
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::DragFloat("Glow Intensity##PointLight", &pickedPointLight->m_VisualIntensity, 0.1f, 0.0f, 100.0f, "%.1f", kSliderFlags);
		ImGui::DragFloat("Scale##PointLight", &pickedPointLight->m_VisualMeshScale, 0.1f, 0.0f, FLT_MAX, "%.1f", kSliderFlags);
		
		ImGui::End();
		return;
	}
	///

	ImGui::SeparatorText("PBR Material");
	if(ImGui::BeginTable("PBR Material Table", 4, ImGuiTableFlags_Borders)) {
		for(const auto& s : scene.m_MaterialNames) {
			ImGui::TableNextColumn();

			static std::string fileName {};
			StringConvert::WideString_To_String(s, fileName);
			if(ImGui::Selectable(fileName.c_str(), s == s_SelectedMat)) {
				auto& copyCommandQueue = device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
				auto copyCommandList = copyCommandQueue.GetCommandList();

				pickedPBRGameObject->UpdatePBRShaderResourcesFromFile(*copyCommandList, s);

				copyCommandQueue.ExecuteCommandList(copyCommandList);
				copyCommandQueue.FlushWait();
				s_SelectedMat = s;
			};
		}
		ImGui::EndTable();
	}

	// Transform component
	GameObjectTransform(scene, pickedPBRGameObject, kSliderFlags, s_GizmoSRTMat, s_ObjTranslation, s_ObjDegreeEulerAngles, s_ObjScale);

	if(ImGui::CollapsingHeader("Shader Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
		{
			if(ImGui::DragFloat2("UV Scale", s_UVScale, 0.01f, 0.0f, 1000.0f, "%.2f", kSliderFlags)) {
				pickedPBRGameObject->m_RenderProps.uvScale = { s_UVScale[0], s_UVScale[1] };
			}
			ImGui::SameLine();

			/// Extremely hacky way to add a uniform scale drag, can't think of another way right now
			ImGui::PushItemWidth(30);
			static float _ {};
			static float lastMousePos {};
			if(ImGui::DragFloat("##UVScaleDrag", &_, 0.001f, 0.0f, 1.0f, "<->", ImGuiSliderFlags_WrapAround)) {
				if(ImGui::GetMousePos().x > lastMousePos) {
					s_UVScale[0] = pickedPBRGameObject->m_RenderProps.uvScale.x += 0.05f;
					s_UVScale[1] = pickedPBRGameObject->m_RenderProps.uvScale.y += 0.05f;
				}
				else {
					s_UVScale[0] = pickedPBRGameObject->m_RenderProps.uvScale.x -= 0.05f;
					s_UVScale[1] = pickedPBRGameObject->m_RenderProps.uvScale.y -= 0.05f;
				}

				lastMousePos = ImGui::GetMousePos().x;
			}
			ImGui::PopItemWidth();
		}

		ImGui::DragFloat("Height Map Magnitude", &pickedPBRGameObject->m_RenderProps.heightMapMagnitude, 0.01f, 0.0f, 1000.0f, "%.2f", kSliderFlags);
		
		ImGui::SeparatorText("Tessellation");
		{
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Mode:"); ImGui::SameLine();
			if(ImGui::RadioButton("Off", &s_TessRadioIdx, 0)) {
				pickedPBRGameObject->m_RenderProps.tessellationModeFlag = RenderFlags_NoTessellation;
			}
			ImGui::SameLine();
			if(ImGui::RadioButton("Uniform", &s_TessRadioIdx, 1)) {
				pickedPBRGameObject->m_RenderProps.tessellationModeFlag = RenderFlags_UniformTessellation;
			}
			ImGui::SameLine();
			if(ImGui::RadioButton("Distance Based Edge", &s_TessRadioIdx, 2)) {
				pickedPBRGameObject->m_RenderProps.tessellationModeFlag = RenderFlags_EdgeTessellation;
			}

			if(s_TessRadioIdx == 1) {
				if(ImGui::DragFloat("Tessellation Magnitude", &pickedPBRGameObject->m_RenderProps.tessellationMagnitude, 0.01f, 1.0f, 1000.0f, "%.2f", kSliderFlags)) {}
			}
			else if(s_TessRadioIdx == 2) {
				if(ImGui::DragFloat("Tessellation Edge Length", &pickedPBRGameObject->m_RenderProps.tessellationEdgeLength, 1.0f, 1.0f, 1000.0f, "%.1f", kSliderFlags)) {}
			}
		}

		ImGui::SeparatorText("Parallax Occlusion Mapping");
		{
			ImGui::DragFloat("Parallax Magnitude", &pickedPBRGameObject->m_RenderProps.parallaxMagnitude, 0.001f, 0.0f, 1.0f, "%.3f", kSliderFlags);
			ImGui::Checkbox("Enable Parallax Self Shadows", &pickedPBRGameObject->m_RenderProps.useParallaxShadow);
			ImGui::DragInt("Min Parallax Layers", &pickedPBRGameObject->m_RenderProps.minParallaxLayers, 1.0f, 0, 100, "%d", kSliderFlags);
			ImGui::DragInt("Max Parallax Layers", &pickedPBRGameObject->m_RenderProps.maxParallaxLayers, 1.0f, 0, 100, "%d", kSliderFlags);
		}
	}

	/// Object Inspector Window End
	ImGui::End();
}