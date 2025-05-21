#include "DemoGame.h"

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif
#include <algorithm> // For std::min and std::max.
#include <iostream> // For std::min and std::max.

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
#include "Material.h"

using namespace DirectX;
using namespace Microsoft::WRL;

/// TODO: TEMP
namespace {
	struct VertexCB {
		XMFLOAT4X4A SRT;
		XMFLOAT4X4A MVP;
		XMFLOAT4A   CameraPosition;
		XMFLOAT4A   Pad1;
		XMFLOAT4A   Pad2;
		XMFLOAT4A   Pad3;
		XMFLOAT4X4A Pad4;
	};

	struct MaterialCB {
		XMFLOAT4A   Time;
		XMFLOAT4A   DirLight;
		XMFLOAT4A   Pad1;
		XMFLOAT4A   Pad2;
		XMFLOAT4X4A Pad3;
		XMFLOAT4X4A Pad4;
		XMFLOAT4X4A Pad5;
	};

	Mesh s_TestCubeMesh;

	void CreateTestCube(CommandList& commandList, float size = 1.0f) {
		// Cube is centered at 0,0,0
		float s = size * 0.5f;

		// 8 edges of cube.
		XMFLOAT3 p[8] = {{ s, s, -s }, { s, s, s }, { s, -s, s }, { s, -s, -s },{ -s, s, s }, { -s, s, -s }, { -s, -s, -s }, { -s, -s, s }};
		// 6 face normals
		XMFLOAT3 n[6] = {{ 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }};
		// 4 unique texture coordinates
		XMFLOAT3 t[4] = {{ 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }};

		// Indices for the vertex positions.
		uint16_t i[24] = {
			0, 1, 2, 3,  // +X
			4, 5, 6, 7,  // -X
			4, 1, 0, 5,  // +Y
			2, 7, 6, 3,  // -Y
			1, 4, 7, 2,  // +Z
			5, 0, 3, 6   // -Z
		};

		std::vector<VertexPositionNormalTangentBitangentTexture> vertices;
		std::vector<uint16_t>  indices;

		for(uint16_t f = 0; f < 6; ++f)  // For each face of the cube.
		{
			// Four vertices per face.
			vertices.emplace_back(p[i[f * 4 + 0]], n[f], t[0]);
			vertices.emplace_back(p[i[f * 4 + 1]], n[f], t[1]);
			vertices.emplace_back(p[i[f * 4 + 2]], n[f], t[2]);
			vertices.emplace_back(p[i[f * 4 + 3]], n[f], t[3]);

			// First triangle.
			indices.emplace_back(f * 4 + 0);
			indices.emplace_back(f * 4 + 1);
			indices.emplace_back(f * 4 + 2);

			// Second triangle
			indices.emplace_back(f * 4 + 2);
			indices.emplace_back(f * 4 + 3);
			indices.emplace_back(f * 4 + 0);
		}

		auto vertexBuffer = commandList.CopyVertexBuffer(vertices);
		auto indexBuffer = commandList.CopyIndexBuffer(indices);

		s_TestCubeMesh = Mesh();
		s_TestCubeMesh.SetVertexBuffer(0, vertexBuffer);
		s_TestCubeMesh.SetIndexBuffer(indexBuffer);
	}
}

/// 

DemoGame::DemoGame(const std::wstring& name, uint32_t width, uint32_t height, bool vSync)
	: m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
	, m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
	, m_Forward(0)
	, m_Backward(0)
	, m_Left(0)
	, m_Right(0)
	, m_Up(0)
	, m_Down(0)
	, m_Pitch(0)
	, m_Yaw(0)
	, m_ShiftPressed(false)
	, m_Width(width)
	, m_Height(height)
	, m_Vsync(vSync) {

	m_Window = Application::Get().CreateRenderWindow(name, width, height, *this);

	XMVECTOR cameraPos    = XMVectorSet(0, 5, -20, 1);
	XMVECTOR cameraTarget = XMVectorSet(0, 5, 0, 1);
	XMVECTOR cameraUp     = XMVectorSet(0, 1, 0, 0);

	m_Camera.set_LookAt(cameraPos, cameraTarget, cameraUp);

	m_pAlignedCameraData = (CameraData*)_aligned_malloc(sizeof(CameraData), 16);

	m_pAlignedCameraData->m_InitialCamPos = m_Camera.get_Translation();
	m_pAlignedCameraData->m_InitialCamRot = m_Camera.get_Rotation();
}

DemoGame::~DemoGame() {
	_aligned_free(m_pAlignedCameraData);
}

uint32_t DemoGame::Run() {
	LoadContent();
	m_Window->Show();

	// Start Window msg loop
	uint32_t retCode = Application::Get().Run();

	UnloadContent();
	return retCode;
}

bool DemoGame::LoadContent() {
	m_Device = std::make_shared<Device>();
	m_SwapChain = std::make_shared<SwapChain>(*m_Device, m_Window->GetWindowHandle(), m_Vsync, DXGI_FORMAT_R8G8B8A8_UNORM);

	auto& commandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto commandList = commandQueue.GetCommandList();

	// TODO: TEMP - Create test cube index and vertex buffers
	CreateTestCube(*commandList);

	commandQueue.ExecuteCommandList(commandList);

	// Load the vertex shader
	ComPtr<ID3DBlob> vertexShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"VertexShader.cso", &vertexShaderBlob));

	// Load the pixel shader
	ComPtr<ID3DBlob> pixelShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"PixelShader.cso", &pixelShaderBlob));

	/// Create root signature.
	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	// TODO: TEMP Test Cube Render
	CD3DX12_ROOT_PARAMETER1 rootParameters[2];
	rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[1].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

	m_RootSignature = std::make_shared<RootSignature>(*m_Device, rootSignatureDescription.Desc_1_1);
	///

	/// Create Pipeline State
	struct PipelineStateStream {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
	} pipelineStateStream;

	DXGI_FORMAT backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT;

	// TODO: Tweakable MSAA
	DXGI_SAMPLE_DESC sampleDesc = m_Device->GetMultisampleQualityLevels(backBufferFormat);

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = backBufferFormat;
	
	pipelineStateStream.pRootSignature        = m_RootSignature->GetD3D12RootSignature().Get();
	pipelineStateStream.InputLayout			  = VertexPositionNormalTangentBitangentTexture::GetInputLayout();
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.VS					  = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
	pipelineStateStream.PS					  = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
	pipelineStateStream.DSVFormat			  = depthBufferFormat;
	pipelineStateStream.RTVFormats            = rtvFormats;
	pipelineStateStream.SampleDesc            = sampleDesc;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {sizeof(PipelineStateStream), &pipelineStateStream};
	ThrowIfFailed(m_Device->GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_D3d12PipelineState)));
	///

	/// Create an off-screen render target with a single color buffer and a depth stencil buffer
	// color buffer
	auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		backBufferFormat, m_Width, m_Height, 1, 1,
		sampleDesc.Count, sampleDesc.Quality, 
		D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
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
		depthBufferFormat, m_Width, m_Height, 1, 1, 
		sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	);

	D3D12_CLEAR_VALUE depthClearValue;
	depthClearValue.Format = depthDesc.Format;
	depthClearValue.DepthStencil = {1.0f, 0};

	auto depthTexture = std::make_shared<Texture>(*m_Device, depthDesc, &depthClearValue);
	depthTexture->SetName(L"Depth Render Target");

	// Attach the textures to the render target.
	m_RenderTarget.AttachTexture(AttachmentPoint::Color0, colorTexture);
	m_RenderTarget.AttachTexture(AttachmentPoint::DepthStencil, depthTexture);
	///

	commandQueue.Flush();  // Wait for loading operations to complete before rendering the first frame.

	return true;
}


void DemoGame::OnResize(ResizeEventArgs& e) {
	m_Width = std::max(1, e.Width);
	m_Height = std::max(1, e.Height);

	m_SwapChain->Resize(m_Width, m_Height);

	float aspectRatio = m_Width / (float)m_Height;
	m_Camera.set_Projection(45.0f, aspectRatio, 0.1f, 100.0f);

	m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height));
	m_RenderTarget.Resize(m_Width, m_Height);
}

// NOTE: might not be needed?
void DemoGame::UnloadContent() {
	m_RenderTarget.Reset();
	m_D3d12PipelineState.Reset();

	m_RootSignature.reset();
	m_SwapChain.reset();
	m_Device.reset();
}

void DemoGame::OnUpdate(UpdateEventArgs& e) {
	static uint64_t frameCount = 0;
	static double fpsTimer = 0.0;

	fpsTimer += e.DeltaTime;
	frameCount++;

	if(fpsTimer > 1.0) {
		double fps = frameCount / fpsTimer;

		wchar_t buffer[256];
		swprintf_s(buffer, L" FPS: %f\n", fps);
		m_Window->SetWindowTitle(buffer);

		frameCount = 0;
		fpsTimer = 0.0;
	}

	m_SwapChain->WaitForSwapChain();

	/// Update the camera.
	float speedMultipler = m_ShiftPressed ? 32.0f : 8.0f;

	XMVECTOR cameraTranslate = XMVectorSet(m_Right - m_Left, 0.0f, m_Forward - m_Backward, 1.0f) * speedMultipler * (float)e.DeltaTime;
	XMVECTOR cameraPan = XMVectorSet(0.0f, m_Up - m_Down, 0.0f, 1.0f) * speedMultipler * (float)e.DeltaTime;
	m_Camera.Translate(cameraTranslate, Space::Local);
	m_Camera.Translate(cameraPan, Space::Local);

	XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(-m_Pitch), XMConvertToRadians(-m_Yaw), 0.0f);
	m_Camera.set_Rotation(cameraRotation);

	XMMATRIX viewMatrix = m_Camera.get_ViewMatrix();
	///

	OnRender(e);
}

void DemoGame::OnRender(UpdateEventArgs& e) {
	auto& commandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = commandQueue.GetCommandList();

	// Clear the render targets.
	FLOAT clearColor[] = {0.6f, 0.6f, 0.7f, 1.0f};
	commandList->ClearTexture(m_RenderTarget.GetTexture(AttachmentPoint::Color0), clearColor);
	commandList->ClearDepthStencilTexture(m_RenderTarget.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH);

	// Setup command list
	commandList->SetPipelineState(m_D3d12PipelineState);
	commandList->SetGraphicsRootSignature(m_RootSignature);
	commandList->SetViewport(m_Viewport);
	commandList->SetScissorRect(m_ScissorRect);
	commandList->SetRenderTarget(m_RenderTarget);

	/// TEMP: Render Test Cube
	// Vertex Shader Buffers
	XMMATRIX translationMat = XMMatrixTranslation(1.0f, 1.0f, 1.0f);
	XMMATRIX rotationMat    = XMMatrixRotationY(XMConvertToRadians(45.0f));
	XMMATRIX scaleMat       = XMMatrixScaling(5.0f, 5.0f, 5.0f);
	XMMATRIX SRTMat         = scaleMat * rotationMat * translationMat;

	VertexCB matrixCB;
	XMStoreFloat4x4A(&matrixCB.SRT, SRTMat);
	XMStoreFloat4x4A(&matrixCB.MVP, SRTMat * m_Camera.get_ViewMatrix() * m_Camera.get_ProjectionMatrix());
	XMStoreFloat4A(&matrixCB.CameraPosition, m_Camera.get_Translation());

	commandList->SetGraphicsDynamicConstantBuffer(0, matrixCB);

	// Pixel Shader Buffers
	// TODO: lighting vars
	MaterialCB materialCB;
	XMVECTORF32 timeVec = {(float)e.Time, (float)e.DeltaTime, 0.0f, 0.0f};
	XMStoreFloat4A(&materialCB.Time, timeVec);

	XMVECTORF32 dirLight = {0.4f, -1.0f, 0.6f, 0.0f};
	XMStoreFloat4A(&materialCB.DirLight, dirLight);

	commandList->SetGraphicsDynamicConstantBuffer(1, materialCB);

	s_TestCubeMesh.Draw(*commandList);
	///

	// Resolve the MSAA render target to the swapchain's backbuffer.
	auto& swapChainRT = m_SwapChain->GetRenderTarget();
	auto  swapChainBackBuffer = swapChainRT.GetTexture(AttachmentPoint::Color0);
	auto  msaaRenderTarget = m_RenderTarget.GetTexture(AttachmentPoint::Color0);
	commandList->ResolveSubresource(swapChainBackBuffer, msaaRenderTarget);

	// Present
	commandQueue.ExecuteCommandList(commandList);
	m_SwapChain->Present();
}

void DemoGame::OnMouseMoved(MouseMotionEventArgs& e) {
	constexpr float mouseSpeed = 0.1f;

	m_Pitch -= e.DeltaY * mouseSpeed;
	m_Pitch = std::clamp(m_Pitch, -90.0f, 90.0f);
	m_Yaw -= e.DeltaX * mouseSpeed;
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
		m_ShiftPressed = true;
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
	case KeyCode::ShiftKey:
		m_ShiftPressed = false;
		break;
	}
}

void DemoGame::OnMouseWheel(MouseWheelEventArgs& e) {
	auto fov = m_Camera.get_FoV();

	fov -= e.WheelDelta;
	fov = std::clamp(fov, 12.0f, 90.0f);

	m_Camera.set_FoV(fov);
}
