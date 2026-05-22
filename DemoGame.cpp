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

#include "Application.h"
#include "Device.h"
#include "SwapChain.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "RootSignature.h"
#include "Helpers.h"
#include "Window.h"
#include "Colors.h"
#include "Texture.h"
#include "Skybox.h"
#include "EditorGui.h"
#include "DirectionalLight.h"
#include "GameObject.h"
#include "PBRObjectPSO.h"
#include "UnlitPSO.h"
#include "UnlitPrimitivePSO.h"
#include "ImageBasedLightingPSO.h"
#include "BloomPSO.h"
#include "BloomEffect.h"
#include "OutlineEffect.h"
#include "AssetImporter.h"
#include "Logger.h"

#include "imgui.h"
#include "implot.h"


using namespace DirectX;
using namespace Microsoft::WRL;

// static parameters
// non const values only represent starting values, they will change during runtime
namespace {
	constexpr float sk_MouseSpeed = 0.05f;

	constexpr DXGI_FORMAT sk_HDRFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	constexpr DXGI_FORMAT sk_DepthStencilBufferFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

	static const std::wstring s_defaultSkyboxName = L"industrial_sunset_puresky_4k.hdr";
	
	// Default Directional Light Shadow params
	int   s_ShadowMapResolution = 4096;
	float s_ShadowMapNear       = 0.1f;
	float s_ShadowMapFar        = 150.0f;
	float s_ShadowDistance      = 35.0f;
	float s_ShadowBias          = 0.001f;

	float s_DefaultFOV = 45.0f;
	float s_MinFOV     = 12.0f;
	float s_MaxFOV     = 90.0f;

	// Projection Matrix
	float s_ZNear = 0.1f;
	float s_ZFar  = 1000.0f;
}

DemoGame::DemoGame(const std::wstring& name, uint32_t windowWidth, uint32_t windowHeight, bool vSync, bool isFullScreen)
	: m_DefaultScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, m_WindowWidth(windowWidth)
	, m_WindowHeight(windowHeight)
	, m_IsVsync(vSync)
{
	m_Window = Application::Get().CreateRenderWindow(name, windowWidth, windowHeight, *this);

	/// TODO: dont hardcode asset path
	// Get skybox names from asset folder
	// Note: wide strings from file system is supported, but it can not be properly displayed with ImGui
	for(const auto& entry : std::filesystem::directory_iterator(AssetImporter::g_AssetPath / L"cubemaps")) {
		m_SkyboxNames.emplace_back(entry.path().filename().c_str());
	}

	m_Device = std::make_shared<Device>();

	// Create EditorGui singleton
	EditorGui::Create(*m_Device, sk_HDRFormat, SwapChain::sk_BufferCount, m_Window->GetWindowHandle());

	// Create post process RT buffers
	m_PostProcessRTs = {*m_Device, sk_HDRFormat, windowWidth, windowHeight};

	/// TODO: Tweakable MSAA
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

		// depth stencil buffer
		auto depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			sk_DepthStencilBufferFormat, m_WindowWidth, m_WindowHeight, 1, 1, multiSampleDesc.Count, multiSampleDesc.Quality,
			D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

		D3D12_CLEAR_VALUE depthClearValue;
		depthClearValue.Format = depthStencilDesc.Format;
		depthClearValue.DepthStencil = { 1.0f, 0 };

		auto depthStencilTexture = std::make_shared<Texture>(*m_Device, depthStencilDesc, &depthClearValue);
		depthStencilTexture->SetName(L"Depth Stencil Render Target");

		m_HDR_MSAA_RT.AttachTexture(AttachmentPoint::Color0, colorTexture);
		m_HDR_MSAA_RT.AttachTexture(AttachmentPoint::DepthStencil, depthStencilTexture);
	}

	/// Create PSOs 
	/// TODO:(this should be managed somewhere else)
	m_PBR_PSO = std::make_unique<PBRObjectPSO>(*m_Device, multiSampleDesc, m_HDR_MSAA_RT.GetRenderTargetFormats(), sk_DepthStencilBufferFormat);
	m_IBL_PSO = std::make_unique<ImageBasedLightingPSO>(*m_Device, m_HDR_MSAA_RT);

	m_Bloom_PSO = std::make_unique<BloomPSO>(*m_Device, m_HDR_MSAA_RT);
	m_Unlit_PSO = std::make_unique<UnlitPSO>(*m_Device, m_HDR_MSAA_RT.GetRenderTargetFormats(), m_PBR_PSO.get()->GetRootSignature());
	m_UnlitPrimitive_PSO = std::make_unique<UnlitPrimitivePSO>(*m_Device, m_HDR_MSAA_RT.GetRenderTargetFormats(), m_PBR_PSO.get()->GetRootSignature());
	///

	m_BloomEffect = std::make_unique<BloomEffect>(*m_Device, m_HDR_MSAA_RT, m_Bloom_PSO.get());
	m_OutlineEffect = std::make_unique<OutlineEffect>(*m_Device, m_HDR_MSAA_RT, m_Unlit_PSO.get(), m_Bloom_PSO.get());

	auto& copyCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	// Load Assets (COPY operations)
	{
		auto copyCommandList = copyCommandQueue.GetCommandList();

		DirectionalLight::DirectionalLightParams dirLightParams {
			m_PBR_PSO->GetRootSignature(), // reuse PBR root signature for depth render
			VertexInput::Get_POS_NORM_TAN_BIT_UV_InputLayout(),
			XMFLOAT3(9.0f, 8.0f, 7.0f),
			XMFLOAT3(140.0f, 230.0f, 0.0f),
			s_ShadowMapResolution,
			s_ShadowDistance,
			{s_ShadowMapNear, s_ShadowMapFar},
			s_ShadowBias
		};

		auto& computeCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
		auto computeCommandList = computeCommandQueue.GetCommandList();

		Skybox::SkyboxParams skyboxParams {
			s_defaultSkyboxName,
			m_IBL_PSO.get()
		};

		m_DemoScene = std::make_unique<Scene>(*m_Device, *copyCommandList, *computeCommandList, dirLightParams, skyboxParams, *this);

		// Wait for IBL resource creation to finish before using them (e.g. panotocubemap in Skybox class)
		copyCommandQueue.WaitForFenceValue(copyCommandQueue.ExecuteCommandList(copyCommandList));
		computeCommandQueue.WaitForFenceValue(computeCommandQueue.ExecuteCommandList(computeCommandList));

		// Precompute skybox IBL textures
		auto& directCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
		auto directCommandList = directCommandQueue.GetCommandList();

		directCommandList->SetScissorRect(m_DefaultScissorRect);

		m_DemoScene->ComputeSkyboxIBLs(*directCommandList);
		directCommandQueue.ExecuteCommandList(directCommandList);

		/// TEST SCENE
		{
			GameObject::EntityParams goParams {
				"Sphere",
				*m_DemoScene,
				XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f)
			};

			GameObject::RenderProps goRenderProps {};
			goRenderProps.pbrPSO = m_PBR_PSO.get();

			goRenderProps.pbrMatName = L"stonewall";
			goParams.scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
			goParams.translation = XMFLOAT3(0.0f, 3.0f, 0.0f);
			goRenderProps.heightMapMagnitude = 0.05f;
			m_DemoScene->AddGameObject(*copyCommandList, goParams, goRenderProps, copyCommandList->GetSpherePrimitive());

			/// STRESS TEST
			//const int s = 5;
			//for(int i = 0; i < 10; i++) {
			//	for(int j = 0; j < 10; j++) {
			//		for(int k = 0; k < 10; k++) {
			//			goParams.translation = XMFLOAT3(s * i, s * j, s * k);
			//			m_DemoScene->AddGameObject(*copyCommandList, goParams, goRenderProps, copyCommandList->GetCubePrimitive());
			//			
			//			static int count = 0;
			//			Logger::Log(++count);
			//		}
			//	}
			//}
			/// END STRESS TEST

			goParams.translation = XMFLOAT3(-4.0f, 3.0f, 0.0f);
			goRenderProps.pbrMatName = L"marble";
			goRenderProps.heightMapMagnitude = 0.0f;
			m_DemoScene->AddGameObject(*copyCommandList, goParams, goRenderProps, copyCommandList->GetSpherePrimitive());

			goParams.name = "Cube";
			goParams.scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
			goParams.translation = XMFLOAT3(4.0f, 3.0f, 0.0f);
			goRenderProps.pbrMatName = L"metal_grid";
			goRenderProps.heightMapMagnitude = 0.0f;
			m_DemoScene->AddGameObject(*copyCommandList, goParams, goRenderProps, copyCommandList->GetCubePrimitive());

			// Test model import
			{
				goParams.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
				goParams.translation = XMFLOAT3(0.0f, 3.0f, 4.0f);
				goRenderProps.pbrMatName = L"viking_sword";
				goRenderProps.heightMapMagnitude = 0.0f;
				std::wstring modelFilePath = (AssetImporter::g_AssetPath / L"models" / goRenderProps.pbrMatName / goRenderProps.pbrMatName).native() + L".obj";
				auto importedMesh = AssetImporter::ImportModel(*copyCommandList, modelFilePath);

				m_DemoScene->AddGameObject(*copyCommandList, goParams, goRenderProps, importedMesh);
			}

			goParams.name = "Floor";
			goParams.scale = XMFLOAT3(40.0f, 40.0f, 40.0f);
			goParams.translation = XMFLOAT3(0.0f, 0.0f, 0.0f);
			goRenderProps.pbrMatName = L"bog";
			goRenderProps.uvScale = XMFLOAT2(5.0f, 5.0f);
			goRenderProps.heightMapMagnitude = 0.0f;
			goRenderProps.parallaxMagnitude = 0.005f;
			goRenderProps.useParallaxShadow = true;
			goRenderProps.isShadowCaster = false;
			m_DemoScene->AddGameObject(*copyCommandList, goParams, goRenderProps, copyCommandList->GetQuadPrimitive());

			copyCommandQueue.ExecuteCommandList(copyCommandList);
		}
	}

	/// TODO: extract PSOs as classes to match rest of project
	// Create Post Process/Tonemap Pipeline States 
	// Note: post process pipeline currently unused, will be used for bloom eventually, it should also be in a separate class
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

		// Note: cull front, triangle created in ScreenRender_VS faces away from camera
		CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
		rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;

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
		postProcessPipelineStateStream.VS = AssetImporter::GetCompiledShaderFromFile(L"ScreenRender_VS.cso");
		postProcessPipelineStateStream.PS = AssetImporter::GetCompiledShaderFromFile(L"Postprocess_PS.cso");
		postProcessPipelineStateStream.Rasterizer = rasterizerDesc;
		postProcessPipelineStateStream.RTVFormats = m_SwapChain->GetRenderTarget().GetRenderTargetFormats();

		m_Device->CreatePipelineState(postProcessPipelineStateStream, m_PostprocessPSO);

		// Tonemap PSO
		postProcessPipelineStateStream.PS = AssetImporter::GetCompiledShaderFromFile(L"Tonemap_PS.cso");
		m_Device->CreatePipelineState(postProcessPipelineStateStream, m_TonemapPSO);
	}

	// Wait for loading operations to complete before rendering the first frame
	copyCommandQueue.FlushWait();

	// Wait for all resizable elements to be created before setting to fullscreen (OnResize will subsuquently be called)
	if(isFullScreen) m_Window->SetFullscreen(true);
}

uint32_t DemoGame::Run() {
	m_Window->Show();

	// Starts Windows msg loop
	// OnUpdate() called on WM_PAINT message
	uint32_t retCode = Application::Get().Run();

	return retCode;
}

void DemoGame::OnResize(const ResizeEventArgs& e) {
	m_WindowWidth  = std::max(1u, e.Width);
	m_WindowHeight = std::max(1u, e.Height);

	m_SwapChain->Resize(m_WindowWidth, m_WindowHeight);

	float aspectRatio = m_WindowWidth / (float)m_WindowHeight;
	m_DemoScene->m_MainCamera.Set_Projection(m_DemoScene->m_MainCamera.Get_FoV(), aspectRatio, s_ZNear, s_ZFar);

	m_HDR_MSAA_RT.Resize(m_WindowWidth, m_WindowHeight);

	m_PostProcessRTs.Resize(m_WindowWidth, m_WindowHeight);

	m_BloomEffect->ResizeRenderTargets(m_WindowWidth, m_WindowHeight);
	m_OutlineEffect->Resize(m_WindowWidth, m_WindowHeight);
}

void DemoGame::OnUpdate(const UpdateEventArgs& e) {
	// Calculate Moving average frame rate (over 128 [sk_frameTimeSamples] frames)
	// moving average used for realtime updates to FPS graph (rather than once a second)
	static uint64_t frameHistoryIndex = 0;
	static double frameTimeSum = 0;
	frameTimeSum -= m_frameTimeHistory[frameHistoryIndex]; // subtract oldest value
	frameTimeSum += e.DeltaTime;
	m_frameTimeHistory[frameHistoryIndex] = e.DeltaTime;

	frameHistoryIndex = (frameHistoryIndex + 1) % sk_frameTimeSamples;
	m_CurrentAvgFPS = (int)(sk_frameTimeSamples / frameTimeSum);

	// Can reduce input latency
	// m_SwapChain->WaitForSwapChain();

	// Update the camera transform if ImGui is not showing, unless right click is held
	// Note: Also check if camera transforms are actually needed, camera class is optimized with dirty flags (includes frustum building)
	if(!EditorGui::Get().GetUIVisibilityState() || m_RightClickPressed) {
		// Camera TRANSLATION
		if(!(m_Left == 0 && m_Right == 0 && m_Up == 0 && m_Down == 0 && m_Forward == 0 && m_Backward == 0)) {
			float speedMultipler = m_LeftShiftPressed ? 32.0f : 8.0f;

			// extra slow movement if using left or right click
			if(m_LeftControlPressed) {
				speedMultipler *= 0.05f;
			}

			XMVECTOR cameraTranslation =
				XMVector3Normalize(XMVECTORF32 { m_Right - m_Left, m_Up - m_Down, m_Forward - m_Backward, 1.0f })
				* speedMultipler * (float)e.DeltaTime;
			m_DemoScene->m_MainCamera.Translate(cameraTranslation);
		}
		
		// Camera ROTATION
		// Note: float compare should be ok because we are only looking if value changed from last frame, not comparing 2 calculated floats
		static float s_LastPitch {};
		static float s_LastYaw {};
		if(!(s_LastPitch == m_CameraPitch && s_LastYaw == m_CameraYaw)) {
			XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(-m_CameraPitch), XMConvertToRadians(-m_CameraYaw), 0.0f);
			m_DemoScene->m_MainCamera.Set_Rotation(cameraRotation);
		}
		s_LastPitch = m_CameraPitch;
		s_LastYaw = m_CameraYaw;
	}

	OnRender(e);
}

void DemoGame::OnRender(const UpdateEventArgs& e) {
	auto& directCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto directCommandList = directCommandQueue.GetCommandList();

	directCommandList->SetScissorRect(m_DefaultScissorRect);

	/// Render Test Scene
	// Perform HDR rendering to multisampled render target
	m_DemoScene->Render(m_HDR_MSAA_RT, *directCommandList, e);

	/// MSAA resolve
	auto& swapChainRT = m_SwapChain->GetRenderTarget();
	// Resolve the MSAA render target to the swapchain's backbuffer
	directCommandList->ResolveSubresource(
		m_PostProcessRTs.RTs[0].GetTexture(AttachmentPoint::Color0),
		m_HDR_MSAA_RT.GetTexture(AttachmentPoint::Color0)
	);

	/// Post Processing
	{
		directCommandList->ClearTexture(m_PostProcessRTs.RTs[1].GetTexture(AttachmentPoint::Color0), Colors::DebugMagenta);
		m_BloomEffect->Render(*directCommandList, m_PostProcessRTs.RTs[0], m_PostProcessRTs.RTs[1]);
		
		/// TODO: come up with something less silly, we need some more logic in PostProcessRenderTargets
		RenderTarget* nextInputPostProcessRT = &m_PostProcessRTs.RTs[1];
		if(m_OutlineEffect->Render(*directCommandList, e, *m_DemoScene, m_PostProcessRTs.RTs[1], m_PostProcessRTs.RTs[0])) {
			nextInputPostProcessRT = &m_PostProcessRTs.RTs[0];
		};

		m_DemoScene->RenderBoundingBoxes(*nextInputPostProcessRT, *directCommandList, e, m_UnlitPrimitive_PSO.get());

		/// TODO: move this to a tonemapping PSO class
		// Tonemapping
		directCommandList->SetPipelineState(m_TonemapPSO);
		directCommandList->SetGraphicsRootSignature(m_PostProcessRootSignature);
		directCommandList->SetViewport(swapChainRT.GetViewport());
		directCommandList->SetRenderTarget(swapChainRT);
		directCommandList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		directCommandList->SetShaderResourceView(0, 0, nextInputPostProcessRT->GetTexture(AttachmentPoint::Color0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		// non indexed full screen render (see ScreenRender vertex shader)
		directCommandList->Draw(3);
	}

	/// Draw ImGui
	{
		/// NOTE: Cursor visibility is currently solely controlled by ImGui, not ideal but works for now.
		///       Cursor is visible anytime a ImGui window is shown or if picker is enabled.
		ImGui::SetMouseCursor(
			EditorGui::Get().GetUIVisibilityState() || !Application::Get().GetCursorClientAreaLockState() ? 
			ImGuiMouseCursor_Arrow : ImGuiMouseCursor_None
		);

		// Keeping these functions separate for flexibility (as opposed to moving them into one function in EditorGui)
		EditorGui::Get().NewFrame();
		EditorGui::Get().DrawGameDebugUI(*m_Device, *m_DemoScene, *this);
		EditorGui::Get().DrawObjectInspector(*m_Device, *m_DemoScene);
		EditorGui::Get().Render(*directCommandList);
	}

	/// Present
	directCommandQueue.ExecuteCommandList(directCommandList);
	m_SwapChain->Present();
}

void DemoGame::OnMouseMove(const MouseMotionEventArgs& e) {
	m_MouseMoved = e.DeltaX != 0 || e.DeltaY != 0;

	// Record mouse rotations only if ImGui is closed or right click is held down
	if(!EditorGui::Get().GetUIVisibilityState() || m_RightClickPressed) {
		m_CameraPitch -= e.DeltaY * sk_MouseSpeed;
		m_CameraPitch = std::clamp(m_CameraPitch, -90.0f, 90.0f);
		m_CameraYaw -= e.DeltaX * sk_MouseSpeed;
	}
}

void DemoGame::OnMouseButtonPressed(const MouseButtonEventArgs& e) {
	if(e.Button == MouseButtonEventArgs::Left) {
		m_LeftClickPressed = true;

		// Lock cursor back to client when left click is pressed,
		// this will be ignored if user presses outside of window or cursor is already locked
		Application::Get().LockCursorToClientArea(m_Window->GetWindowHandle(), true);
	}
	else if(e.Button == MouseButtonEventArgs::Right) {
		m_RightClickPressed = true;
	}
}

void DemoGame::OnMouseButtonReleased(const MouseButtonEventArgs& e) {
	m_DemoScene->OnMouseButtonReleased(e);

	if(e.Button == MouseButtonEventArgs::Left) {
		m_LeftClickPressed = false;
		m_Forward = 0.0f;
	}
	else if(e.Button == MouseButtonEventArgs::Right) {
		m_RightClickPressed = false;
		m_Forward = 0.0f;
	}
}

void DemoGame::OnKeyPressed(const KeyEventArgs& e) {
	m_DemoScene->OnKeyPressed(e);

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
			m_Up = 1.0f;
			break;

		case KeyCode::E:
			m_Down = 1.0f;
			break;

		case KeyCode::F:
			EditorGui::Get().SetPickerState(true);
			break;

		case KeyCode::Escape:
			if(!Application::Get().GetCursorClientAreaLockState()) {
				Application::Get().Quit();
			}

			Application::Get().LockCursorToClientArea(m_Window->GetWindowHandle(), false);
			break;

		case KeyCode::F11: 
		case KeyCode::End: // only because I have End key bound to my mouse, can remove if this gets in the way 
			m_Window->ToggleFullscreen();
			break;
		
		case KeyCode::V:
			m_SwapChain->ToggleVSync();
			break;

		case KeyCode::ShiftKey:
			m_LeftShiftPressed = true;
			break;

		case KeyCode::ControlKey:
			m_LeftControlPressed = true;
			break;
	}
}

void DemoGame::OnKeyReleased(const KeyEventArgs& e) {
	m_DemoScene->OnKeyReleased(e);

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
			m_Up = 0.0f;
			break;

		case KeyCode::E:
			m_Down = 0.0f;
			break;

		case KeyCode::F1: 
			EditorGui::Get().ToggleDebugWindowState();
			break;

		case KeyCode::F:
			EditorGui::Get().SetPickerState(false);
			break;

		case KeyCode::ShiftKey:
			m_LeftShiftPressed = false;
			break;

		case KeyCode::ControlKey:
			m_LeftControlPressed = false;
			break;
	}
}

void DemoGame::OnMouseWheel(const MouseWheelEventArgs& e) {
	if(!EditorGui::Get().GetUIVisibilityState()) {
		auto fov = m_DemoScene->m_MainCamera.Get_FoV();
		fov = std::clamp(fov - e.WheelDelta, s_MinFOV, s_MaxFOV);
		m_DemoScene->m_MainCamera.Set_FoV(fov);
	}
}

