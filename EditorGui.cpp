#include "EditorGui.h"

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

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

// Game specific
#include "Application.h"
#include "DemoGame.h"
#include "BloomPass.h"

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
	float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT { 0, 0 }, MONITOR_DEFAULTTOPRIMARY)) * 0.8f;
	ImGuiStyle& style = ImGui::GetStyle();
	// Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.ScaleAllSizes(main_scale);     
	// Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
	style.FontScaleDpi = main_scale;
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

void EditorGui::DrawGameDebugUI(const DemoGame& game, Device& device, Scene& scene) {
	static const ImGuiSliderFlags kSliderFlags = ImGuiSliderFlags_AlwaysClamp;

	struct ScrollingBuffer {
		int MaxSize;
		int Offset;
		ImVector<ImVec2> Data;
		ScrollingBuffer(int max_size = 2000) {
			MaxSize = max_size;
			Offset = 0;
			Data.reserve(MaxSize);
		}
		void AddPoint(float x, float y) {
			if(Data.size() < MaxSize)
				Data.push_back(ImVec2(x, y));
			else {
				Data[Offset] = ImVec2(x, y);
				Offset = (Offset + 1) % MaxSize;
			}
		}
	};

	if(sb_ShowDebugWindow) {
		/// Main Engine UI Window Start
		{
			// b_CurrentDebugWindowState added only for x button to work
			bool b_CurrentDebugWindowState = sb_ShowDebugWindow;
			ImGui::Begin("DX12 Engine", &b_CurrentDebugWindowState, ImGuiWindowFlags_NoCollapse);
			sb_ShowDebugWindow = b_CurrentDebugWindowState;

			// Exit button
			{
				ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
				if(ImGui::Button("EXIT APP")) {
					Application::Get().Quit();
				}
				ImGui::PopStyleColor(3);
			}

			// Performance Graph 
			// Graph data update rate based on s_GraphUpdateRate, default: 60hz
			// This is to throttle the rate ScrollingBuffer records data so we don't need a huge buffer for high frame rates over a big time scale
			{
				static ScrollingBuffer s_FPSGraphBuffer;

				if(s_FPSGraphBuffer.Data.size() == 0) {
					s_FPSGraphBuffer.AddPoint((float)ImGui::GetTime(), (float)game.m_CurrentAvgFPS);
				}

				// Save axis extents for update ticks faster than s_GraphUpdateRate
				static std::pair xCurrentAxisExtents = { 0.0, 1.0 };
				static std::pair yCurrentAxisExtents = { 0.0, 1.0 };

				static const float s_GraphUpdateRate = 1.0f / 60.0f;
				static float s_Timer = s_GraphUpdateRate;
				s_Timer -= ImGui::GetIO().DeltaTime;
				if(s_Timer <= 0) {
					s_FPSGraphBuffer.AddPoint((float)ImGui::GetTime(), (float)game.m_CurrentAvgFPS);
					s_Timer = s_GraphUpdateRate;
				}

				ImGui::Text("FPS: %d", game.m_CurrentAvgFPS);

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
					if(s_Timer == s_GraphUpdateRate) {
						ImPlot::SetupAxisLimits(ImAxis_X1, ImGui::GetTime() - timeScale, ImGui::GetTime(), ImGuiCond_Always);
						xCurrentAxisExtents.first = ImGui::GetTime() - timeScale;
						xCurrentAxisExtents.second = ImGui::GetTime();

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

					ImPlot::PlotLine("FPS", &s_FPSGraphBuffer.Data[0].x, &s_FPSGraphBuffer.Data[0].y,
						s_FPSGraphBuffer.Data.size(), 0, s_FPSGraphBuffer.Offset, 2 * sizeof(float)
					);

					ImPlot::EndPlot();

					ImGui::SliderInt("Time Scale", &timeScale, 1, 30, "%ds");
				}
			}

			// FOV Slider
			float fov = scene.m_MainCamera.Get_FoV();
			ImGui::SliderFloat("FOV", &fov, 12.0f, 90.0f);
			scene.m_MainCamera.Set_FoV(fov);

			// Directional Light
			if(ImGui::CollapsingHeader("Directional Light")) {
				static DirectionalLight& sceneLight = scene.GetDirLight();
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

				if(ImGui::DragFloat2("[x, y]", s_SceneDirLightEulerAngle, 0.1f, 0.0f, 360.0f, "%.2f", kSliderFlags | ImGuiSliderFlags_WrapAround)) {
					sceneLight.SetEulerAngles(s_SceneDirLightEulerAngle[0], s_SceneDirLightEulerAngle[1], 0.0f);
				}

				if(ImGui::DragFloat3("Directional Light Color", s_SceneDirLightColor, 0.1f, 0.0f, 100.0f, "%.2f", kSliderFlags)) {
					sceneLight.SetColor(s_SceneDirLightColor[0], s_SceneDirLightColor[1], s_SceneDirLightColor[2]);
				}

				if(ImGui::DragFloat2("Shadow Near/Far Z", s_ShadowNearFarZ, 0.1f, 0.1f, 10000.0f, "%.2f", kSliderFlags)) {
					sceneLight.SetShadowNearFarZ({ s_ShadowNearFarZ[0], s_ShadowNearFarZ[1] });
				}

				if(ImGui::DragFloat("Shadow Render Distance", &s_ShadowRenderDistance, 0.1f, 0.1f, 10000.0f, "%.2f", kSliderFlags)) {
					sceneLight.SetShadowRenderDistance(s_ShadowRenderDistance);
				}

				// Shadow debug
				if(ImGui::TreeNode("Shadow Debug")) {
					static float s_ImageScale = 0.25f;
					ImGui::SliderFloat("##Directional Shadow Map Texture Scale", &s_ImageScale, 0.0, 1.0, "%.2fx");
					ImVec2 imageSize = ImVec2(1920.0f * s_ImageScale, 1080.0f * s_ImageScale);

					// Directional shadow map debug view
					ImGui::ImageWithBg(
						(ImTextureID)GetImageSRVAllocation(EditorGui::GuiSRVIndex::DirectionalShadowMap).gpuHandle.ptr,
						imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImVec4(0.0f, 0.0f, 0.0f, 1.0f)
					);
					ImGui::TreePop();
				}
			}

			// Skybox Selector
			{
				static std::wstring_view s_SelectedSkybox = scene.m_Skybox.GetTextureName();
				if(ImGui::BeginTable("Skybox Table", 3, ImGuiTableFlags_Borders)) {
					for(auto& s : game.m_SkyboxNames) {
						ImGui::TableNextColumn();

						if(ImGui::Selectable(StringConvert::WideString_To_String(s).c_str(), s == s_SelectedSkybox)) {
							auto& copyCommandQueue = device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
							auto copyCommandList = copyCommandQueue.GetCommandList();
							auto& computeCommandQueue = device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
							auto computeCommandList = computeCommandQueue.GetCommandList();

							// Load new skybox cubemap
							/// TODO: Creates new skybox object every time, do something smarter
							Skybox::SkyboxParams skyboxParams {
								s,
								game.m_IBL_PSO.get()
							};

							scene.SetSkybox(*copyCommandList, *computeCommandList, skyboxParams);

							copyCommandQueue.WaitForFenceValue(copyCommandQueue.ExecuteCommandList(copyCommandList));
							computeCommandQueue.WaitForFenceValue(computeCommandQueue.ExecuteCommandList(computeCommandList));
							///

							// Render new IBLs
							auto& directCommandQueue = device.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
							auto directCommandList = directCommandQueue.GetCommandList();
							directCommandList->SetScissorRect(game.m_DefaultScissorRect);
							scene.ComputeSkyboxIBLs(*directCommandList);
							directCommandQueue.WaitForFenceValue(directCommandQueue.ExecuteCommandList(directCommandList));

							s_SelectedSkybox = s;
						};

					}
					ImGui::EndTable();
				}
			}

			// Bloom
			if(ImGui::CollapsingHeader("Bloom")) {
				static BloomPass* bloomPass = game.m_BloomPass.get();
				static float s_BloomIntensity = bloomPass->GetIntensity();
				static float s_Threshold      = bloomPass->GetThreshold();
				static float s_SoftThreshold  = bloomPass->GetSoftThreshold();

				if(ImGui::DragFloat("Intensity", &s_BloomIntensity, 0.01f, 0.0f, 10.0f, "%.2f", kSliderFlags)) {
					bloomPass->SetIntensity(s_BloomIntensity);
				}
				if(ImGui::DragFloat("Theshold", &s_Threshold, 0.01f, 0.0f, 100.0f, "%.2f", kSliderFlags)) {
					bloomPass->SetThreshold(s_Threshold);
				}
				if(ImGui::DragFloat("Soft Theshold", &s_SoftThreshold, 0.01f, 0.0f, 100.0f, "%.2f", kSliderFlags)) {
					bloomPass->SetSoftThreshold(s_SoftThreshold);
				}

				// Bloom debug view
				// Need ImVec4(0.0f, 0.0f, 0.0f, 1.0f) background color, else some textures are see through for some reason
				// Might have something to do with alpha blending when ImGui theme has transparency
				if(ImGui::TreeNode("Prefilter Debug")) {
					static float imageScale = 0.25f;
					ImGui::SliderFloat("##Bloom Texture Scale", &imageScale, 0.0, 1.0, "%.2fx");
					ImVec2 imageSize = ImVec2(1920.0f * imageScale, 1080.0f * imageScale);

					ImGui::ImageWithBg(
						(ImTextureID)GetImageSRVAllocation(EditorGui::GuiSRVIndex::BloomPrefilter).gpuHandle.ptr,
						imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImVec4(0.0f, 0.0f, 0.0f, 1.0f)
					);
					ImGui::TreePop();
				}
			}

			/// Main Engine UI Window End
			ImGui::End();
		}
	}
}

void EditorGui::DrawObjectInspector(Device& device, const Scene& scene) {
	GameObject* picked = scene.m_Picker->GetPickedObject();
	if(!sb_ObjectInspectorState) return;

	static const ImGuiSliderFlags kSliderFlags = ImGuiSliderFlags_AlwaysClamp;

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
		s_ObjectName += "##ObjectInspector"; // appending this decouples window title and window ID

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

	ImGui::Begin(s_ObjectName.c_str(), &sb_ObjectInspectorState, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
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

		{
			if(ImGui::DragFloat3("Scale", s_ObjScale, 0.01f, -1000.0f, 1000.0f, "%.2f", kSliderFlags)) {
				picked->SetScale(s_ObjScale[0], s_ObjScale[1], s_ObjScale[2]);
			}
			ImGui::SameLine();
			if(ImGui::Button("+##+Scale")) {
				picked->Scale(1.1f, 1.1f, 1.1f);
				XMFLOAT3 scale = picked->GetScale();
				memcpy(s_ObjScale, &scale, sizeof(float) * 3);
			}
			ImGui::SameLine();
			if(ImGui::Button("-##-Scale")) {
				picked->Scale(0.9f, 0.9f, 0.9f);
				XMFLOAT3 scale = picked->GetScale();
				memcpy(s_ObjScale, &scale, sizeof(float) * 3);
			}
		}

		{
			if(ImGui::DragFloat2("UV Scale", s_UVScale, 0.01f, 0.0f, 1000.0f, "%.2f", kSliderFlags)) {
				picked->m_RenderProps.uvScale = { s_UVScale[0], s_UVScale[1] };
			}
			ImGui::SameLine();
			if(ImGui::Button("+##+UVScale")) {
				s_UVScale[0] = picked->m_RenderProps.uvScale.x += 1.0f;
				s_UVScale[1] = picked->m_RenderProps.uvScale.y += 1.0f;
			}
			ImGui::SameLine();
			if(ImGui::Button("-##-UVScale")) {
				s_UVScale[0] = picked->m_RenderProps.uvScale.x -= 1.0f;
				s_UVScale[1] = picked->m_RenderProps.uvScale.y -= 1.0f;
			}
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
