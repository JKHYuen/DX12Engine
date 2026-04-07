#include "DemoGame.h"

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif
#include <algorithm> // For std::min and std::max.
#include <filesystem>

#include <d3dx12.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#include "Application.h"
#include "Device.h"
#include "SwapChain.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "RootSignature.h"
#include "Helpers.h"
#include "Window.h"
#include "Mesh.h"
#include "Skybox.h"
#include "EditorGui.h"
#include "DirectionalLight.h"
#include "ShaderResourceView.h"
#include "GameObject.h"
#include "PBRObjectPSO.h"
#include "OutlinePSO.h"
#include "ImageBasedLightingPSO.h"
#include "AssetImporter.h"
#include "Logger.h"

#include "imgui.h"
#include "implot.h"


using namespace DirectX;
using namespace Microsoft::WRL;

// static parameters
namespace {
	constexpr float sk_MouseSpeed = 0.05f;

	constexpr DXGI_FORMAT sk_HDRFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr DXGI_FORMAT sk_DepthBufferFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

	const std::wstring s_defaultSkyboxName = L"industrial_sunset_puresky_4k.hdr";

	// Directional Light Shadow params
	int   s_ShadowMapResolution = 2048;
	float s_ShadowMapNear       = 0.1f;
	float s_ShadowMapFar        = 150.0f;
	float s_ShadowDistance      = 100.0f;
	float s_ShadowBias          = 0.001f;

	float s_DefaultFOV = 45.0f;
	float s_MinFOV = 12.0f;
	float s_MaxFOV = 90.0f;

	// Projection Matrix
	float s_ZNear = 0.1f;
	float s_ZFar = 1000.0f;
}

DemoGame::DemoGame(const std::wstring& name, uint32_t windowWidth, uint32_t windowHeight, bool vSync)
	: m_DefaultScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, m_ScreenViewport(CD3DX12_VIEWPORT(0.0f, 0.0f, (float)windowWidth, (float)windowHeight))
	, m_Forward(0)
	, m_Backward(0)
	, m_Left(0)
	, m_Right(0)
	, m_Up(0)
	, m_Down(0)
	, m_Pitch(0)
	, m_Yaw(0)
	, m_IsShiftPressed(false)
	, m_IsLeftClickPressed(false)
	, m_IsRightClickPressed(false)
	, m_ShowImGuiWindow(false)
	, m_CurrentAvgFPS(0)
	, m_WindowWidth(windowWidth)
	, m_WindowHeight(windowHeight)
	, m_IsVsync(vSync)
	, m_HDR_MSAA_RenderTarget() 
	, m_FloatRenderTarget() {

	m_Window = Application::Get().CreateRenderWindow(name, windowWidth, windowHeight, *this);

	/// TODO: dont hardcode asset path
	// Get skybox names from asset folder
	// Note: wide strings from file system is supported, but it can not be properly displayed with ImGui
	for(const auto& entry : std::filesystem::directory_iterator(L"assets/cubemaps")) {
		m_SkyboxNames.emplace_back(entry.path().filename().c_str());
	}
}

uint32_t DemoGame::Run() {
	// m_Device created here
	Initialize();
	m_Window->Show();

	// Starts Windows msg loop
	// OnUpdate() called on WM_PAINT message
	uint32_t retCode = Application::Get().Run();

	// Bandaid Fix: 
	// Manually destroy DX device and adapter, ImGUI gets upset if these aren't destoyed before it calls its shutdown functions
	m_Device.reset();

	return retCode;
}

bool DemoGame::Initialize() {
	m_Device = std::make_shared<Device>();
	m_EditorGui = std::make_unique<EditorGui>(*m_Device, sk_HDRFormat, SwapChain::sk_BufferCount, m_Window->GetWindowHandle());
	// TODO: Tweakable MSAA
	DXGI_SAMPLE_DESC multiSampleDesc = m_Device->GetMultisampleQualityLevels(sk_HDRFormat);

	m_SwapChain = std::make_shared<SwapChain>(*m_Device, m_Window->GetWindowHandle(), m_IsVsync, sk_HDRFormat);

	// Create render targets
	{
		// color buffer
		auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			sk_HDRFormat, m_WindowWidth, m_WindowHeight, 1, 1, multiSampleDesc.Count, multiSampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		D3D12_CLEAR_VALUE colorClearValue;
		colorClearValue.Format = colorDesc.Format;
		colorClearValue.Color[0] = 0.6f;
		colorClearValue.Color[1] = 0.6f;
		colorClearValue.Color[2] = 0.7f;
		colorClearValue.Color[3] = 1.0f;

		auto colorTexture = std::make_shared<Texture>(*m_Device, colorDesc, &colorClearValue);
		colorTexture->SetName(L"Color Render Target");

		// depth buffer
		auto depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			sk_DepthBufferFormat, m_WindowWidth, m_WindowHeight, 1, 1, multiSampleDesc.Count, multiSampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

		D3D12_CLEAR_VALUE depthClearValue;
		depthClearValue.Format = depthDesc.Format;
		depthClearValue.DepthStencil = { 1.0f, 0 };

		auto depthTexture = std::make_shared<Texture>(*m_Device, depthDesc, &depthClearValue);
		depthTexture->SetName(L"Depth Render Target");

		m_HDR_MSAA_RenderTarget.AttachTexture(AttachmentPoint::Color0, colorTexture);
		m_HDR_MSAA_RenderTarget.AttachTexture(AttachmentPoint::DepthStencil, depthTexture);

		// Non multisampled floating point render texture,
		// multisampled HDR rendertarget will be resolved into this texture before postprocessing/tonemapping
		auto floatTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			sk_HDRFormat, m_WindowWidth, m_WindowHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		auto floatRenderTexture = std::make_shared<Texture>(*m_Device, floatTextureDesc, &colorClearValue);
		floatRenderTexture->SetName(L"Screen Floating Point Render Target");
		m_FloatRenderTarget.AttachTexture(AttachmentPoint::Color0, floatRenderTexture);
	}

	// Create PBR Pipeline State s
	m_PBR_PSO = std::make_unique<PBRObjectPSO>(*m_Device, multiSampleDesc, m_HDR_MSAA_RenderTarget.GetRenderTargetFormats(), sk_DepthBufferFormat);
	m_Outline_PSO = std::make_unique<OutlinePSO>(*m_Device, multiSampleDesc, m_HDR_MSAA_RenderTarget.GetRenderTargetFormats(), sk_DepthBufferFormat);

	m_IBL_PSO = std::make_unique<ImageBasedLightingPSO>(*m_Device, m_HDR_MSAA_RenderTarget);

	auto& copyCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	// Load Assets (COPY operations)
	{
		auto copyCommandList = copyCommandQueue.GetCommandList();

		DirectionalLight::DirectionalLightParams dirLightParams {
			m_PBR_PSO->GetRootSignature(), // reuse PBR root signature for depth render
			VertexInput::GetInputLayout(),
			XMFLOAT3(9.0f, 8.0f, 7.0f),
			XMFLOAT3(50.0f, 230.0f, 0.0f),
			s_ShadowMapResolution,
			s_ShadowDistance,
			s_ShadowMapNear,
			s_ShadowMapFar,
			s_ShadowBias
		};

		/// TODO: Skybox switching in runtime
		Skybox::SkyboxParams skyboxParams {
			s_defaultSkyboxName,
			m_IBL_PSO.get()
		};

		m_TestScene = std::make_unique<Scene>(*m_Device, *copyCommandList, dirLightParams, skyboxParams, m_WindowWidth, m_WindowHeight);

		// Wait for IBL resource creation to finish (panotocubemap in Skybox class)
		copyCommandQueue.WaitForFenceValue(copyCommandQueue.ExecuteCommandList(copyCommandList));

		// Precompute skybox IBL textures
		auto& directCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
		auto directCommandList = directCommandQueue.GetCommandList();
		m_TestScene->ComputeSkyboxIBLMaps(*directCommandList);
		directCommandQueue.ExecuteCommandList(directCommandList);

		/// TEMP TEST SCENE
		{
			GameObject::GameObjectParams goParams{
				"Sphere",
				L"stonewall",
				*m_TestScene,
				XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 1.0f),
				m_PBR_PSO.get(),
				m_Outline_PSO.get(),
			};

			goParams.translation = XMFLOAT3(0.0f, 3.0f, 0.0f);
			goParams.scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
			m_TestScene->CreateGameObject(*copyCommandList, goParams, copyCommandList->GetSpherePrimitive());

			goParams.name = "Cube";
			goParams.pbrMatName = L"metal_grid";
			goParams.translation = XMFLOAT3(4.0f, 3.0f, 0.0f);
			goParams.scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
			m_TestScene->CreateGameObject(*copyCommandList, goParams, copyCommandList->GetCubePrimitive());

			goParams.name = "Floor";
			goParams.pbrMatName = L"marble";
			goParams.translation = XMFLOAT3(0.0f, 0.0f, 0.0f);
			goParams.scale = XMFLOAT3(20.0f, 1.0f, 20.0f);
			m_TestScene->CreateGameObject(*copyCommandList, goParams, copyCommandList->GetQuadPrimitive());

			/// Test model import
			goParams.translation = XMFLOAT3(0.0f, 3.0f, 4.0f);
			goParams.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
			goParams.pbrMatName = L"viking_sword";
			auto importedMesh = AssetImporter::ImportModel(*copyCommandList, L"assets/models/" + goParams.pbrMatName + L"/" + goParams.pbrMatName +L".obj");

			//auto importedMesh = AssetImporter::ImportModel(*copyCommandList, L"assets/models/sponza/NewSponza_Main_glTF_003.gltf");
			m_TestScene->CreateGameObject(*copyCommandList, goParams, importedMesh);

			copyCommandQueue.ExecuteCommandList(copyCommandList);
		}
	}

	/// Create Post Process/Tonemap Pipeline States 
	/// Note: post process pipeline currently unused, will be used for bloom eventually, it should also be in a separate class
	{
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags_VSPS =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[1] {};
		rootParameters[0].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC pointClampSampler(
			0, D3D12_FILTER_MIN_MAG_MIP_POINT,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &pointClampSampler, rootSignatureFlags_VSPS);
		m_PostProcessRootSignature = std::make_shared<RootSignature>(*m_Device, rootSignatureDescription.Desc_1_1);

		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> ps;
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/ScreenRender_VS.cso", &vs));
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Postprocess_PS.cso", &ps));

		// Note: not sure why this is needed, ignores post processing shader without D3D12_CULL_MODE_NONE
		CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

		/// TODO: disable depth?
		struct PostProcessPipelineStateStream {
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            Rasterizer;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		} postProcessPipelineStateStream;

		postProcessPipelineStateStream.pRootSignature = m_PostProcessRootSignature->GetD3D12RootSignature().Get();
		postProcessPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		postProcessPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		postProcessPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		postProcessPipelineStateStream.Rasterizer = rasterizerDesc;
		postProcessPipelineStateStream.RTVFormats = m_SwapChain->GetRenderTarget().GetRenderTargetFormats();

		m_Device->CreatePipelineState(postProcessPipelineStateStream, m_PostprocessPSO);

		// Tonemap PSO
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Tonemap_PS.cso", &ps));
		postProcessPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		m_Device->CreatePipelineState(postProcessPipelineStateStream, m_TonemapPSO);
	}

	// Wait for loading operations to complete before rendering the first frame
	copyCommandQueue.FlushWait();  

	return true;
}

void DemoGame::OnResize(const ResizeEventArgs& e) {
	m_WindowWidth = std::max(1, e.Width);
	m_WindowHeight = std::max(1, e.Height);

	m_SwapChain->Resize(m_WindowWidth, m_WindowHeight);

	float aspectRatio = m_WindowWidth / (float)m_WindowHeight;
	/// TODO: Define default z values somewhere
	m_TestScene->m_MainCamera.Set_Projection(s_DefaultFOV, aspectRatio, s_ZNear, s_ZFar);

	m_ScreenViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_WindowWidth), static_cast<float>(m_WindowHeight));
	m_HDR_MSAA_RenderTarget.Resize(m_WindowWidth, m_WindowHeight);
	m_FloatRenderTarget.Resize(m_WindowWidth, m_WindowHeight);

	m_TestScene->SetGameWindowSize(m_WindowWidth, m_WindowHeight);
}

void DemoGame::OnUpdate(const UpdateEventArgs& e) {
	// Calculate Moving average frame rate (over 128 [sk_frameTimeSamples] frames)
	// moving average used for realtime updates to FPS graph (rather than once a second)
	static uint64_t frameHistoryIndex = 0;
	static double frameTimeSum = 0;
	frameTimeSum -= m_frameTimeHistory[frameHistoryIndex]; // subtract oldest value
	frameTimeSum += e.DeltaTime;
	m_frameTimeHistory[frameHistoryIndex] = e.DeltaTime;

	frameHistoryIndex = (frameHistoryIndex + 1) % DemoGame::sk_frameTimeSamples;
	m_CurrentAvgFPS = (int)(DemoGame::sk_frameTimeSamples / frameTimeSum);

	// Can reduce input latency
	//m_SwapChain->WaitForSwapChain();

	// Update the camera transform if ImGui is not showing, unless right click is held
	if(!m_ShowImGuiWindow || m_IsRightClickPressed) {
		float speedMultipler = m_IsShiftPressed ? 32.0f : 8.0f;
		
		// extra slow movement if using left or right click
		if(m_IsLeftClickPressed || m_IsRightClickPressed) {
			speedMultipler *= 0.05f;
		}
		
		XMVECTOR cameraTranslation = 
			XMVector3Normalize(XMVectorSet(m_Right - m_Left, 0.0f, m_Forward - m_Backward, 1.0f))
			* speedMultipler * (float)e.DeltaTime;
		XMVECTOR cameraPan = XMVectorSet(0.0f, m_Up - m_Down, 0.0f, 1.0f) 
			* speedMultipler * (float)e.DeltaTime;

		m_TestScene->m_MainCamera.Translate(cameraTranslation, Space::Local);
		m_TestScene->m_MainCamera.Translate(cameraPan, Space::Local);

		XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(-m_Pitch), XMConvertToRadians(-m_Yaw), 0.0f);
		m_TestScene->m_MainCamera.Set_Rotation(cameraRotation);
	}

	OnRender(e);
}

void DemoGame::RenderImGui(CommandList& directCommandList) {
	/// NOTE: Cursor visibility is currently solely controlled by ImGui, not ideal but works for now.
	///       Cursor is invisible anytime ImGui window is not shown.
	ImGui::SetMouseCursor(m_ShowImGuiWindow || !Application::Get().GetCursorClientAreaLockState() ? ImGuiMouseCursor_Arrow : ImGuiMouseCursor_None);

	m_EditorGui->NewFrame();

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

	if(m_ShowImGuiWindow) {
		/// Main Engine UI Window Start
		{
			ImGui::Begin("DX12 Engine", &m_ShowImGuiWindow, ImGuiWindowFlags_NoCollapse);

			// Exit button
			{
				ImGui::PushStyleColor(ImGuiCol_Button,        (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive,  (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
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
					s_FPSGraphBuffer.AddPoint((float)ImGui::GetTime(), (float)m_CurrentAvgFPS);
				}

				// Save axis extents for update ticks faster than s_GraphUpdateRate
				static std::pair xCurrentAxisExtents = { 0.0, 1.0 };
				static std::pair yCurrentAxisExtents = { 0.0, 1.0 };

				static const float s_GraphUpdateRate = 1.0f / 60.0f;
				static float timer = s_GraphUpdateRate;
				timer -= ImGui::GetIO().DeltaTime;
				if(timer <= 0) {
					s_FPSGraphBuffer.AddPoint((float)ImGui::GetTime(), (float)m_CurrentAvgFPS);
					timer = s_GraphUpdateRate;
				}

				ImGui::Text("FPS: %d", m_CurrentAvgFPS);

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
					if(timer == s_GraphUpdateRate) {
						ImPlot::SetupAxisLimits(ImAxis_X1, ImGui::GetTime() - timeScale, ImGui::GetTime(), ImGuiCond_Always);
						xCurrentAxisExtents.first = ImGui::GetTime() - timeScale;
						xCurrentAxisExtents.second = ImGui::GetTime();

						if(m_CurrentAvgFPS >= yCurrentAxisExtents.second || m_CurrentAvgFPS * 2.0f <= yCurrentAxisExtents.second) {
							float newYMax = std::max(144.0f, m_CurrentAvgFPS * 1.5f);
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
			static float fov = m_TestScene->m_MainCamera.Get_FoV();
			ImGui::SliderFloat("FOV", &fov, 12.0f, 90.0f);
			m_TestScene->m_MainCamera.Set_FoV(fov);

			// Directional Light
			static float sceneDirLightAngle[2] { 50.0f, 230.0f };
			if(ImGui::DragFloat2("[x, y]", sceneDirLightAngle, 0.1f, 0.0f, 1000.0f, "%.2f", kSliderFlags)) {
				sceneDirLightAngle[0] = std::fmod(sceneDirLightAngle[0], 360.0f);
				sceneDirLightAngle[1] = std::fmod(sceneDirLightAngle[1], 360.0f);
				m_TestScene->SetDirectionalLightAngle(sceneDirLightAngle[0], sceneDirLightAngle[1], 0.0f);
			}
			
			// Skybox Selector
			{
				static std::wstring s_SelectedSkybox = s_defaultSkyboxName;
				if(ImGui::BeginTable("Skybox Table", 3, ImGuiTableFlags_Borders)) {
					for(auto& s : m_SkyboxNames) {
						ImGui::TableNextColumn();

						/// TODO: make this work
						if(ImGui::Selectable(StringConvert::WideString_to_String(s).c_str(), s == s_SelectedSkybox)) {
							auto& copyCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
							auto copyCommandList = copyCommandQueue.GetCommandList();

							/// Do something smarter
							Skybox::SkyboxParams skyboxParams {
								s,
								m_IBL_PSO.get()
							};

							m_TestScene->SetSkybox(*copyCommandList, skyboxParams);
							///

							copyCommandQueue.ExecuteCommandList(copyCommandList);
							copyCommandQueue.FlushWait();

							/// Doesn't work, IBL render targets not being updated
							auto& directCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
							auto directCommandList = directCommandQueue.GetCommandList();

							m_TestScene->ComputeSkyboxIBLMaps(*directCommandList);
							directCommandQueue.ExecuteCommandList(directCommandList);
							directCommandQueue.FlushWait();

							s_SelectedSkybox = s;
						};

					}
					ImGui::EndTable();
				}
			}

			// Directional shadow map debug view
			{
				static float imageScale = 0.25f;
				ImGui::SliderFloat("Scale", &imageScale, 0.0, 1.0, "%.2fx");
				ImVec2 imageSize = ImVec2(1920.0f * imageScale, 1080.0f * imageScale);

				ImGui::Image(
					(ImTextureID)EditorGui::GetImageSRV(EditorGui::GuiSRVIndex::DirectionalShadowMap).gpuHandle.ptr,
					imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f)
				);
			}

			/// Main Engine UI Window End
			ImGui::End();
		}

		/// Object Inspector Window / Scene specific UI
		{
			m_TestScene->RenderImGui();
		}
	}

	m_EditorGui->Render(directCommandList);
}

void DemoGame::OnRender(const UpdateEventArgs& e) {
	auto& directCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto directCommandList = directCommandQueue.GetCommandList();

	/// Render Test Scene
	// Clear the render targets.
	float clearColor[] = {0.6f, 0.6f, 0.7f, 1.0f};
	directCommandList->ClearTexture(m_HDR_MSAA_RenderTarget.GetTexture(AttachmentPoint::Color0), clearColor);
	directCommandList->ClearDepthStencilTexture(m_HDR_MSAA_RenderTarget.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);

	// Perform HDR rendering to intermediate render target (before multisample resolve)
	m_TestScene->Render(m_HDR_MSAA_RenderTarget, m_ScreenViewport, m_DefaultScissorRect, *directCommandList, e);

	/// Post Processing
	{
		auto& swapChainRT = m_SwapChain->GetRenderTarget();
		auto  msaaResolveDstTexture = m_FloatRenderTarget.GetTexture(AttachmentPoint::Color0);
		auto  msaaHDRRenderTexture = m_HDR_MSAA_RenderTarget.GetTexture(AttachmentPoint::Color0);

		// Resolve the MSAA render target to the swapchain's backbuffer
		directCommandList->ResolveSubresource(msaaResolveDstTexture, msaaHDRRenderTexture);

		// TODO: Bloom

		// Tonemapping
		directCommandList->SetRenderTarget(swapChainRT);
		directCommandList->SetViewport(swapChainRT.GetViewport());
		directCommandList->SetPipelineState(m_TonemapPSO);
		directCommandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		directCommandList->SetGraphicsRootSignature(m_PostProcessRootSignature);
		directCommandList->SetShaderResourceView(0, 0, msaaResolveDstTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		// non indexed full screen render (see ScreenRender vertex shader)
		directCommandList->Draw(3);
	}

	/// Draw ImGui
	RenderImGui(*directCommandList);

	/// Present
	directCommandQueue.ExecuteCommandList(directCommandList);
	m_SwapChain->Present();
}

void DemoGame::OnMouseMove(const MouseMotionEventArgs& e) {
	// Record mouse rotations only if ImGui is closed or right click is held down
	if(!m_ShowImGuiWindow || m_IsRightClickPressed) {
		m_Pitch -= e.DeltaY * sk_MouseSpeed;
		m_Pitch = std::clamp(m_Pitch, -90.0f, 90.0f);
		m_Yaw -= e.DeltaX * sk_MouseSpeed;
	}
}

void DemoGame::OnMouseButtonPressed(const MouseButtonEventArgs& e) {
	if(e.Button == MouseButtonEventArgs::Left) {
		m_IsLeftClickPressed = true;
		m_Forward = 1.0f;

		// Lock cursor back to client when left click is pressed,
		// this will be ignored if user presses outside of window or cursor is already locked
		Application::Get().LockCursorToClientArea(m_Window->GetWindowHandle(), true);
	}
	else if(e.Button == MouseButtonEventArgs::Right) {
		m_IsRightClickPressed = true;

		// Holding right click moves camera back,
		// disabled for ImGui because right click is held to enable camera movement
		if(!m_ShowImGuiWindow) {
			m_Forward = -1.0f;
		}
	}
}

void DemoGame::OnMouseButtonReleased(const MouseButtonEventArgs& e) {
	m_TestScene->OnMouseButtonReleased(e);

	if(e.Button == MouseButtonEventArgs::Left) {
		m_IsLeftClickPressed = false;
		m_Forward = 0.0f;
	}
	else if(e.Button == MouseButtonEventArgs::Right) {
		m_IsRightClickPressed = false;
		m_Forward = 0.0f;
	}
}

void DemoGame::OnKeyPressed(const KeyEventArgs& e) {
	m_TestScene->OnKeyPressed(e);

	switch(e.Key) {
	case KeyCode::Up:
	case KeyCode::W:
		m_Forward = 1.0f;
		break;

	case KeyCode::Left:
	case KeyCode::A:
		m_Left = 1.0f;
		break;

	case KeyCode::Down:
	case KeyCode::S:
		m_Backward = 1.0f;
		break;

	case KeyCode::Right:
	case KeyCode::D:
		m_Right = 1.0f;
		break;

	case KeyCode::Q:
		m_Down = 1.0f;
		break;

	case KeyCode::E:
		m_Up = 1.0f;
		break;

	case KeyCode::Escape:
		if(!Application::Get().GetCursorClientAreaLockState()) {
			Application::Get().Quit();
		}

		Application::Get().LockCursorToClientArea(m_Window->GetWindowHandle(), false);
		break;

	case KeyCode::F11:
		m_Window->ToggleFullscreen();
		break;

	case KeyCode::V:
		m_SwapChain->ToggleVSync();
		break;

	case KeyCode::X:
		m_TestScene;
		break;

	case KeyCode::ShiftKey:
		m_IsShiftPressed = true;
		break;
	}
}

void DemoGame::OnKeyReleased(const KeyEventArgs& e) {
	m_TestScene->OnKeyReleased(e);

	switch(e.Key) {
	case KeyCode::Up:
	case KeyCode::W:
		m_Forward = 0.0f;
		break;

	case KeyCode::Left:
	case KeyCode::A:
		m_Left = 0.0f;
		break;

	case KeyCode::Down:
	case KeyCode::S:
		m_Backward = 0.0f;
		break;

	case KeyCode::Right:
	case KeyCode::D:
		m_Right = 0.0f;
		break;

	case KeyCode::Q:
		m_Down = 0.0f;
		break;

	case KeyCode::E:
		m_Up = 0.0f;
		break;

	case KeyCode::F1: 
		m_ShowImGuiWindow = !m_ShowImGuiWindow;
		break;

	case KeyCode::ShiftKey:
		m_IsShiftPressed = false;
		break;
	}
}

void DemoGame::OnMouseWheel(const MouseWheelEventArgs& e) {
	if(!m_ShowImGuiWindow) {
		auto fov = m_TestScene->m_MainCamera.Get_FoV();

		fov -= e.WheelDelta;
		fov = std::clamp(fov, s_MinFOV, s_MaxFOV);

		m_TestScene->m_MainCamera.Set_FoV(fov);
	}
}

