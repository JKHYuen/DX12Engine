#include "DemoGame.h"

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif
#include <algorithm> // For std::min and std::max.

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
#include "Logger.h"

#include "imgui.h"
#include "implot.h"


using namespace DirectX;
using namespace Microsoft::WRL;

// static parameters
namespace {
	constexpr DXGI_FORMAT sk_HDRFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr DXGI_FORMAT sk_DepthBufferFormat = DXGI_FORMAT_D32_FLOAT;

	// Directional Light Shadow params
	int   s_ShadowMapResolution = 2048;
	float s_ShadowMapNear       = 0.1f;
	float s_ShadowMapFar        = 150.0f;
	float s_ShadowDistance      = 100.0f;
	float s_ShadowBias          = 0.001f;
}

/// TODO: TEMP
namespace {
	//EditorGui::GuiDescriptorAllocation s_GuiShadowMapDebugSRV;
}

DemoGame::DemoGame(const std::wstring& name, uint32_t width, uint32_t height, bool vSync)
	: m_DefaultScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, m_ScreenViewport(CD3DX12_VIEWPORT(0.0f, 0.0f, (float)width, (float)height))
	, m_Forward(0)
	, m_Backward(0)
	, m_Left(0)
	, m_Right(0)
	, m_Up(0)
	, m_Down(0)
	, m_Pitch(0)
	, m_Yaw(0)
	, m_IsShiftPressed(false)
	, m_ShowImGuiWindow(false)
	, m_CurrentAvgFPS(0)
	, m_WindowWidth(width)
	, m_WindowHeight(height)
	, m_IsVsync(vSync)
	, m_HDR_MSAA_RenderTarget() 
	, m_FloatRenderTarget() {

	m_Window = Application::Get().CreateRenderWindow(name, width, height, *this);
//
//	XMVECTOR cameraPos    = XMVectorSet(0, 5, -20, 1);
//	XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
//	XMVECTOR cameraUp     = XMVectorSet(0, 1, 0, 0);
//
//	m_Camera.Set_LookAt(cameraPos, cameraTarget, cameraUp);
}

uint32_t DemoGame::Run() {
	// m_Device created here
	Initialize();
	m_Window->Show();

	// Starts Windows msg loop
	// OnUpdate() called on WM_PAINT message
	uint32_t retCode = Application::Get().Run();

	UnloadContent();

	return retCode;
}

bool DemoGame::Initialize() {
	m_Device = std::make_shared<Device>();
	m_EditorGui = std::make_unique<EditorGui>(*m_Device, sk_HDRFormat, SwapChain::sk_BufferCount, m_Window->GetWindowHandle());
	// TODO: Tweakable MSAA
	DXGI_SAMPLE_DESC sampleDesc = m_Device->GetMultisampleQualityLevels(sk_HDRFormat);

	m_SwapChain = std::make_shared<SwapChain>(*m_Device, m_Window->GetWindowHandle(), m_IsVsync, sk_HDRFormat);

	/// Create render targets
	{
		// color buffer
		auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			sk_HDRFormat, m_WindowWidth, m_WindowHeight, 1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
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
			sk_DepthBufferFormat, m_WindowWidth, m_WindowHeight, 1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
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

	/// Create PBR Pipeline State (For rendering PBR objects)
	m_PBR_PSO = std::make_unique<PBRObjectPSO>(*m_Device, sampleDesc, m_HDR_MSAA_RenderTarget.GetRenderTargetFormats(), sk_DepthBufferFormat);

	auto& copyCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	/// Load Assets (COPY operations)
	{
		auto copyCommandList = copyCommandQueue.GetCommandList();

		std::wstring matName = L"stonewall";
		std::wstring skyboxName = L"industrial_sunset_puresky_4k";

		DirectionalLight::DirectionalLightParams dirLightParams {
			m_PBR_PSO->GetRootSignature(),
			VertexInput::GetInputLayout(),
			XMFLOAT3(9.0f, 8.0f, 7.0f),
			XMFLOAT3(50.0f, 230.0f, 0.0f),
			s_ShadowMapResolution,
			s_ShadowDistance,
			s_ShadowMapNear,
			s_ShadowMapFar,
			s_ShadowBias
		};

		Skybox::SkyboxParams skyboxParams {
			skyboxName,
			copyCommandList->CreateCubePrimitive(),
			m_HDR_MSAA_RenderTarget
		};

		m_Scene = std::make_unique<Scene>(*m_Device, *copyCommandList, dirLightParams, skyboxParams);

		// Test objects to render
		{
			GameObject::GameObjectParams goParams{
				*m_Scene,
				XMMatrixTranslation(1.0f, 4.0f, 1.0f), XMMatrixIdentity(), XMMatrixScaling(1.0f, 1.0f, 1.0f),
				m_PBR_PSO,
				matName,
			};

			// Test Sphere Object
			m_Scene->CreateGameObject(*copyCommandList, goParams, copyCommandList->CreateSpherePrimitive());

			goParams.translationMat = XMMatrixTranslation(1.0f, 1.0f, 1.0f);
			goParams.scaleMat = XMMatrixScaling(40.0f, 1.0f, 40.0f);

			// Test Floor Object
			m_Scene->CreateGameObject(*copyCommandList, goParams, copyCommandList->CreateQuadPrimitive());
		}

		/// TODO:
		//// Initialize ImGui SRV for debug
		//// Note: SRVs for ImGui render have its own allocator and descriptor heap (instead of the two stage DynamicDescriptorHeap system) to keep things simple
		//s_GuiShadowMapDebugSRV = EditorGui::AllocateImageSRV(*m_Device, m_DirectionalShadowMap.GetTexture(AttachmentPoint::DepthStencil), &srvDesc);

		copyCommandQueue.ExecuteCommandList(copyCommandList);
	}

	/// Create Post Process/Tonemap Pipeline States 
	/// Note: post process pipeline currently unused, will be used for bloom eventually
	{
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags_VSPS =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
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

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {sizeof(PostProcessPipelineStateStream), &postProcessPipelineStateStream};
		ThrowIfFailed(m_Device->GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PostprocessPSO)));

		// Tonemap PSO
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Tonemap_PS.cso", &ps));
		postProcessPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		pipelineStateStreamDesc = {sizeof(PostProcessPipelineStateStream), &postProcessPipelineStateStream};
		ThrowIfFailed(m_Device->GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_TonemapPSO)));
	}

	// Wait for loading operations to complete before rendering the first frame
	copyCommandQueue.FlushWait();  

	// Precompute skybox IBL textures
	auto& directCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto directCommandList = directCommandQueue.GetCommandList();

	m_Scene->ComputeSkyboxIBLMaps(*directCommandList);
	directCommandQueue.ExecuteCommandList(directCommandList);

	return true;
}

void DemoGame::OnResize(ResizeEventArgs& e) {
	m_WindowWidth = std::max(1, e.Width);
	m_WindowHeight = std::max(1, e.Height);

	m_SwapChain->Resize(m_WindowWidth, m_WindowHeight);

	float aspectRatio = m_WindowWidth / (float)m_WindowHeight;
	/// TODO: Define default z values somewhere (maybe scene class that's not written yet)
	m_Scene->m_MainCamera.Set_Projection(45.0f, aspectRatio, 0.1f, 1000.0f);

	m_ScreenViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_WindowWidth), static_cast<float>(m_WindowHeight));
	m_HDR_MSAA_RenderTarget.Resize(m_WindowWidth, m_WindowHeight);
	m_FloatRenderTarget.Resize(m_WindowWidth, m_WindowHeight);
}

// NOTE: only needed if multiple IGame objects are loaded in one session
void DemoGame::UnloadContent() {
	m_HDR_MSAA_RenderTarget.Reset();
	m_FloatRenderTarget.Reset();
	//m_DirectionalShadowMap.Reset();

	//m_PBR_PSO.Reset();
	m_TonemapPSO.Reset();
	m_PostprocessPSO.Reset();
	//m_ShadowDepthPSO.Reset();

	//m_PBRRootSignature.reset();
	m_PostProcessRootSignature.reset();

	m_SwapChain.reset();
	m_Device.reset();
}

void DemoGame::OnUpdate(UpdateEventArgs& e) {
	// Moving average frame rate (over 128 [sk_frameTimeSamples] frames)
	// moving average used for realtime updates tp FPS graph (rather than once a second)
	static uint64_t frameIndex = 0;
	static double frameTimeSum = 0;
	frameTimeSum -= m_frameTimeHistory[frameIndex];
	frameTimeSum += e.DeltaTime;
	m_frameTimeHistory[frameIndex] = e.DeltaTime;

	frameIndex = (frameIndex + 1) % DemoGame::sk_frameTimeSamples;
	m_CurrentAvgFPS = (int)(DemoGame::sk_frameTimeSamples / frameTimeSum);

	//m_SwapChain->WaitForSwapChain();

	/// Update the camera.
	if(!m_ShowImGuiWindow) {
		float speedMultipler = m_IsShiftPressed ? 32.0f : 16.0f;
		
		XMVECTOR cameraTranslation = 
			XMVector3Normalize(XMVectorSet(m_Right - m_Left, 0.0f, m_Forward - m_Backward, 1.0f))
			* speedMultipler * (float)e.DeltaTime;
		XMVECTOR cameraPan = XMVectorSet(0.0f, m_Up - m_Down, 0.0f, 1.0f) 
			* speedMultipler * (float)e.DeltaTime;

		m_Scene->m_MainCamera.Translate(cameraTranslation, Space::Local);
		m_Scene->m_MainCamera.Translate(cameraPan, Space::Local);

		XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(-m_Pitch), XMConvertToRadians(-m_Yaw), 0.0f);
		m_Scene->m_MainCamera.Set_Rotation(cameraRotation);
	}
	///

	OnRender(e);
}

void DemoGame::ShowImGuiWindow(CommandList& directCommandList) {
	m_EditorGui->NewFrame();

	static ImGuiSliderFlags kSliderFlags = ImGuiSliderFlags_AlwaysClamp;

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
		ImGui::Begin("DX12 Engine", &m_ShowImGuiWindow, ImGuiWindowFlags_NoCollapse);

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

		/// TODO: move this to it's own window?
		// Performance Graph 
		// Graph data update rate based on s_GraphUpdateRate, default: 60hz
		// This is to throttle the rate ScrollingBuffer records data so we don't need a huge buffer for high frame rates over a big time scale
		{
			static ScrollingBuffer s_FPSGraphBuffer;

			if(s_FPSGraphBuffer.Data.size() == 0) {
				s_FPSGraphBuffer.AddPoint((float)ImGui::GetTime(), (float)m_CurrentAvgFPS);
			}

			// Save axis extents for update ticks faster than s_GraphUpdateRate
			static std::pair xCurrentAxisExtents = {0.0, 1.0};
			static std::pair yCurrentAxisExtents = {0.0, 1.0};

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
		{
			static float fov = m_Scene->m_MainCamera.Get_FoV();
			ImGui::SliderFloat("FOV", &fov, 12.0f, 90.0f);
			m_Scene->m_MainCamera.Set_FoV(fov);
		}

		static float sceneDirLightAngle[2] { 50.0f, 230.0f };
		if(ImGui::DragFloat2("[x, y]", sceneDirLightAngle, 0.1f, 0.0f, 1000.0f, "%.2f", kSliderFlags)) {
			sceneDirLightAngle[0] = std::fmod(sceneDirLightAngle[0], 360.0f);
			sceneDirLightAngle[1] = std::fmod(sceneDirLightAngle[1], 360.0f);
			m_Scene->SetDirectionalLightDirection(sceneDirLightAngle[0], sceneDirLightAngle[1], 0.0f);
			//s_DirectionalLight->SetDirection(sceneDirLightAngle[0], sceneDirLightAngle[1], 0.0f);
		}

		//{
		//	static float imageScale = 0.25f;
		//	ImGui::SliderFloat("Scale", &imageScale, 0.0, 1.0, "%.2fx");
		//	ImVec2 imageSize = ImVec2(1920.0f * imageScale, 1080.0f * imageScale);

		//	ImGui::Image(
		//		(ImTextureID)s_GuiShadowMapDebugSRV.gpuHandle.ptr, imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f)
		//	);
		//}

		ImGui::End();
	}


	m_EditorGui->Render(directCommandList);
}

void DemoGame::OnRender(UpdateEventArgs& e) {
	auto& directCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto directCommandList = directCommandQueue.GetCommandList();

	/// Render Test Scene
	// Clear the render targets.
	float clearColor[] = {0.6f, 0.6f, 0.7f, 1.0f};
	directCommandList->ClearTexture(m_HDR_MSAA_RenderTarget.GetTexture(AttachmentPoint::Color0), clearColor);
	directCommandList->ClearDepthStencilTexture(m_HDR_MSAA_RenderTarget.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH);

	// Setup command list for HDR rendering to intermediate render target (before multisample resolve)
	directCommandList->SetScissorRect(m_DefaultScissorRect);
	directCommandList->SetViewport(m_ScreenViewport);
	directCommandList->SetRenderTarget(m_HDR_MSAA_RenderTarget);

	m_Scene->RenderSkybox(*directCommandList);

	// Render scene objects
	/// TODO: move pipeline state change into Scene class?
	m_PBR_PSO->SetPipelineState(*directCommandList, m_HDR_MSAA_RenderTarget, m_ScreenViewport, m_DefaultScissorRect);
	m_Scene->RenderObjects(*directCommandList, e);

	// Render depth from directional light for same objects above
	m_Scene->RenderObjectShadowDepths(*directCommandList);

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
	ShowImGuiWindow(*directCommandList);

	/// Present
	directCommandQueue.ExecuteCommandList(directCommandList);
	m_SwapChain->Present();
}

void DemoGame::OnMouseMove(MouseMotionEventArgs& e) {
	if(!m_ShowImGuiWindow) {
		constexpr float mouseSpeed = 0.1f;
		m_Pitch -= e.DeltaY * mouseSpeed;
		m_Pitch = std::clamp(m_Pitch, -90.0f, 90.0f);
		m_Yaw -= e.DeltaX * mouseSpeed;
	}
}

void DemoGame::OnMouseButtonPressed(MouseButtonEventArgs& e) {
	if(e.Button == MouseButtonEventArgs::Left) {
		m_Forward = 1.0f;
	}
}

void DemoGame::OnMouseButtonReleased(MouseButtonEventArgs& e) {
	if(e.Button == MouseButtonEventArgs::Left) {
		m_Forward = 0.0f;
	}
}

void DemoGame::OnKeyPressed(KeyEventArgs& e) {
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
		Application::Get().Quit();
		break;
	case KeyCode::F11:
		m_Window->ToggleFullscreen();
		break;
	case KeyCode::V:
		m_SwapChain->ToggleVSync();
		break;
	case KeyCode::ShiftKey:
		m_IsShiftPressed = true;
		break;
	}
}
void DemoGame::OnKeyReleased(KeyEventArgs& e) {
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
	case KeyCode::Tab:
		m_ShowImGuiWindow = !m_ShowImGuiWindow;
		break;
	case KeyCode::ShiftKey:
		m_IsShiftPressed = false;
		break;
	}
}

void DemoGame::OnMouseWheel(MouseWheelEventArgs& e) {
	if(!m_ShowImGuiWindow) {
		auto fov = m_Scene->m_MainCamera.Get_FoV();

		fov -= e.WheelDelta;
		fov = std::clamp(fov, 12.0f, 90.0f);

		m_Scene->m_MainCamera.Set_FoV(fov);
	}
}
