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
	enum PBRRootParameters {
		VertexCB,       // ConstantBuffer<Mat> VertexCB : register(b0);
		MaterialCB,     // ConstantBuffer<Material> MaterialCB : register( b0, space1 );
		Textures,       // Range Size: 2 
						// Texture2D AlbedoTex : register( t0 );
						// Texture2D NormalTex : register( t1 );
		NumRootParameters
	};

	struct VertexProps {
		XMFLOAT4X4A SRT;
		XMFLOAT4X4A MVP;
		XMFLOAT4A   CameraPosition;
		XMFLOAT4A   Pad1;
		XMFLOAT4A   Pad2;
		XMFLOAT4A   Pad3;
		XMFLOAT4X4A Pad4;
	};

	struct MaterialProps {
		XMFLOAT4A   Time;
		XMFLOAT4A   DirLight;
		XMFLOAT4A   Pad1;
		XMFLOAT4A   Pad2;
		XMFLOAT4X4A Pad3;
		XMFLOAT4X4A Pad4;
		XMFLOAT4X4A Pad5;
	};

	std::shared_ptr<Texture> s_StoneWallAlbedo;
	std::shared_ptr<Texture> s_StoneWallNormal;

	Mesh s_TestCube;
	Mesh s_TestSphere;

	void CalculateTangentBinormal(const VertexInputType& vertex1, const VertexInputType& vertex2, const VertexInputType& vertex3, XMVECTOR& tangent, XMVECTOR& binormal) {
		float vector1[3], vector2[3];
		float tuVector[2], tvVector[2];
		float den;
		float length;

		// Calculate the two vectors for this face.
		vector1[0] = vertex2.Position.x - vertex1.Position.x;
		vector1[1] = vertex2.Position.y - vertex1.Position.y;
		vector1[2] = vertex2.Position.z - vertex1.Position.z;

		vector2[0] = vertex3.Position.x - vertex1.Position.x;
		vector2[1] = vertex3.Position.y - vertex1.Position.y;
		vector2[2] = vertex3.Position.z - vertex1.Position.z;

		// Calculate the tu and tv texture space vectors.
		tuVector[0] = vertex2.TexCoord.x - vertex1.TexCoord.x;
		tvVector[0] = vertex2.TexCoord.y - vertex1.TexCoord.y;

		tuVector[1] = vertex3.TexCoord.x - vertex1.TexCoord.x;
		tvVector[1] = vertex3.TexCoord.y - vertex1.TexCoord.y;

		// Calculate the denominator of the tangent/binormal equation.
		den = 1.0f / (tuVector[0] * tvVector[1] - tuVector[1] * tvVector[0]);

		// Calculate the cross products, multiply by the coefficient and normalize to get the tangent and bitangent
		tangent = XMVector3Normalize(XMVectorSet(
			(tvVector[1] * vector1[0] - tvVector[0] * vector2[0]) * den,
			(tvVector[1] * vector1[1] - tvVector[0] * vector2[1]) * den,
			(tvVector[1] * vector1[2] - tvVector[0] * vector2[2]) * den, 1.0f
		));
		binormal = XMVector3Normalize(XMVectorSet(
			(tuVector[0] * vector2[0] - tuVector[1] * vector1[0]) * den,
			(tuVector[0] * vector2[1] - tuVector[1] * vector1[1]) * den,
			(tuVector[0] * vector2[2] - tuVector[1] * vector1[2]) * den, 1.0f
		));
	}

	void CalculateModelVectors(std::vector<VertexInputType>& vertices, const std::vector<uint16_t>& indices) {
		XMVECTOR tangent {}, bitangent {};

		int triangleCount = indices.size() / 3;

		// Index of index buffer
		size_t i = 0;

		// Go through all triangles and calculate the the tangent and binormal vectors.
		for(int f = 0; f < triangleCount; f++) {
			CalculateTangentBinormal(vertices[indices[i++]], vertices[indices[i++]], vertices[indices[i++]], tangent, bitangent);

			// Store the tangent and binormal 
			// NOTE: some repeated work done
			XMStoreFloat3(&vertices[indices[i - 1]].Tangent, tangent);
			XMStoreFloat3(&vertices[indices[i - 1]].Bitangent, bitangent);
			XMStoreFloat3(&vertices[indices[i - 2]].Tangent, tangent);
			XMStoreFloat3(&vertices[indices[i - 2]].Bitangent, bitangent);
			XMStoreFloat3(&vertices[indices[i - 3]].Tangent, tangent);
			XMStoreFloat3(&vertices[indices[i - 3]].Bitangent, bitangent);
		}
	}

	void CreateTestCube(CommandList& commandList, float size = 1.0f) {
		// Cube is centered at 0,0,0
		float s = size * 0.5f;

		// 8 edges of cube.
		XMFLOAT3 p[8] = {{ s, s, -s }, { s, s, s }, { s, -s, s }, { s, -s, -s },{ -s, s, s }, { -s, s, -s }, { -s, -s, -s }, { -s, -s, s }};
		// 6 face normals
		XMFLOAT3 n[6] = {{ 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }};
		// 4 unique texture coordinates
		XMFLOAT3 uv[4] = {{ 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }};

		// Indices for the vertex positions.
		uint16_t i[24] = {
			0, 1, 2, 3,  // +X
			4, 5, 6, 7,  // -X
			4, 1, 0, 5,  // +Y
			2, 7, 6, 3,  // -Y
			1, 4, 7, 2,  // +Z
			5, 0, 3, 6   // -Z
		};

		std::vector<VertexInputType> vertices;
		std::vector<uint16_t>  indices;

		for(uint16_t f = 0; f < 6; ++f)  // For each face of the cube.
		{
			// Four vertices per face.
			vertices.emplace_back(p[i[f * 4 + 0]], n[f], uv[0]);
			vertices.emplace_back(p[i[f * 4 + 1]], n[f], uv[1]);
			vertices.emplace_back(p[i[f * 4 + 2]], n[f], uv[2]);
			vertices.emplace_back(p[i[f * 4 + 3]], n[f], uv[3]);

			// First triangle.
			indices.emplace_back(f * 4 + 0);
			indices.emplace_back(f * 4 + 1);
			indices.emplace_back(f * 4 + 2);

			// Second triangle
			indices.emplace_back(f * 4 + 2);
			indices.emplace_back(f * 4 + 3);
			indices.emplace_back(f * 4 + 0);
		}

		CalculateModelVectors(vertices, indices);

		auto vertexBuffer = commandList.CopyVertexBuffer(vertices);
		auto indexBuffer = commandList.CopyIndexBuffer(indices);

		s_TestCube = Mesh();
		s_TestCube.SetVertexBuffer(0, vertexBuffer);
		s_TestCube.SetIndexBuffer(indexBuffer);
	}

	void CreateTestSphere(CommandList& commandList, float radius, uint32_t tessellation) {

		if(tessellation < 3)
			throw std::out_of_range("tessellation parameter out of range");

		std::vector<VertexInputType> vertices;
		std::vector<uint16_t>  indices;

		size_t verticalSegments = tessellation;
		size_t horizontalSegments = tessellation * 2;

		// Create rings of vertices at progressively higher latitudes.
		for(size_t i = 0; i <= verticalSegments; i++) {
			float v = 1 - (float)i / verticalSegments;

			float latitude = (i * XM_PI / verticalSegments) - XM_PIDIV2;
			float dy, dxz;

			XMScalarSinCos(&dy, &dxz, latitude);

			// Create a single ring of vertices at this latitude.
			for(size_t j = 0; j <= horizontalSegments; j++) {
				float u = (float)j / horizontalSegments;

				float longitude = j * XM_2PI / horizontalSegments;
				float dx, dz;

				XMScalarSinCos(&dx, &dz, longitude);

				dx *= dxz;
				dz *= dxz;

				auto normal = XMVectorSet(dx, dy, dz, 0);
				auto textureCoordinate = XMVectorSet(u, v, 0, 0);
				auto position = normal * radius;

				vertices.emplace_back(position, normal, textureCoordinate);
			}
		}

		// Fill the index buffer with triangles joining each pair of latitude rings.
		size_t stride = horizontalSegments + 1;

		for(size_t i = 0; i < verticalSegments; i++) {
			for(size_t j = 0; j < horizontalSegments; j++) {
				size_t nextI = i + 1;
				size_t nextJ = (j + 1) % stride;

				indices.push_back(i * stride + nextJ);
				indices.push_back(nextI * stride + j);
				indices.push_back(i * stride + j);

				indices.push_back(nextI * stride + nextJ);
				indices.push_back(nextI * stride + j);
				indices.push_back(i * stride + nextJ);
			}
		}

		CalculateModelVectors(vertices, indices);

		auto vertexBuffer = commandList.CopyVertexBuffer(vertices);
		auto indexBuffer = commandList.CopyIndexBuffer(indices);

		s_TestSphere = Mesh();
		s_TestSphere.SetVertexBuffer(0, vertexBuffer);
		s_TestSphere.SetIndexBuffer(indexBuffer);
	}
}

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
	, m_Vsync(vSync)
	, m_Camera() {

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

	/// Load Assets
	auto& commandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	auto commandList = commandQueue.GetCommandList();

	// TODO / TEMP: more dynamic scene object loading
	//CreateTestCube(*commandList);
	CreateTestSphere(*commandList, 1.0f, 64);

	s_StoneWallAlbedo = commandList->LoadTextureFromFile(L"assets/stonewall_albedo.tga", true);
	s_StoneWallNormal = commandList->LoadTextureFromFile(L"assets/stonewall_normal.tga", false);

	commandQueue.ExecuteCommandList(commandList);
	///

	// Load the vertex shader
	ComPtr<ID3DBlob> vertexShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PBR_vs.cso", &vertexShaderBlob));

	// Load the pixel shader
	ComPtr<ID3DBlob> pixelShaderBlob;
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PBR_ps.cso", &pixelShaderBlob));

	/// Create root signature.
	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	// TODO: TEMP Test Cube Render
	CD3DX12_ROOT_PARAMETER1 rootParameters[PBRRootParameters::NumRootParameters];
	rootParameters[PBRRootParameters::VertexCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[PBRRootParameters::MaterialCB].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
	rootParameters[PBRRootParameters::Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

	//CD3DX12_STATIC_SAMPLER_DESC linearRepeatSampler(0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR);
	CD3DX12_STATIC_SAMPLER_DESC anisotropicSampler(0, D3D12_FILTER_ANISOTROPIC);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(PBRRootParameters::NumRootParameters, rootParameters, 1, &anisotropicSampler, rootSignatureFlags);

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
	pipelineStateStream.InputLayout			  = VertexInputType::GetInputLayout();
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
	float speedMultipler = m_ShiftPressed ? 32.0f : 16.0f;

	XMVECTOR cameraTranslate = XMVectorSet(m_Right - m_Left, 0.0f, m_Forward - m_Backward, 1.0f) * speedMultipler * (float)e.DeltaTime;
	XMVECTOR cameraPan = XMVectorSet(0.0f, m_Up - m_Down, 0.0f, 1.0f) * speedMultipler * (float)e.DeltaTime;
	m_Camera.Translate(cameraTranslate, Space::Local);
	m_Camera.Translate(cameraPan, Space::Local);

	XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(-m_Pitch), XMConvertToRadians(-m_Yaw), 0.0f);
	m_Camera.set_Rotation(cameraRotation);
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
	XMMATRIX rotationMat    = XMMatrixIdentity();
	XMMATRIX scaleMat       = XMMatrixScaling(5.0f, 5.0f, 5.0f);
	XMMATRIX SRTMat         = scaleMat * rotationMat * translationMat;

	VertexProps vertexCB;
	XMStoreFloat4x4A(&vertexCB.SRT, SRTMat);
	XMStoreFloat4x4A(&vertexCB.MVP, SRTMat * m_Camera.get_ViewMatrix() * m_Camera.get_ProjectionMatrix());
	XMStoreFloat4A(&vertexCB.CameraPosition, m_Camera.get_Translation());

	commandList->SetGraphicsDynamicConstantBuffer(PBRRootParameters::VertexCB, vertexCB);

	// Pixel Shader Buffers
	// TODO: lighting vars
	MaterialProps materialCB;
	XMVECTORF32 timeVec = {(float)e.Time, (float)e.DeltaTime, 0.0f, 0.0f};
	XMStoreFloat4A(&materialCB.Time, timeVec);
	XMVECTORF32 dirLight = {0.4f, -1.0f, 0.8f, 0.0f};
	XMStoreFloat4A(&materialCB.DirLight, XMVector3Normalize(dirLight));

	commandList->SetGraphicsDynamicConstantBuffer(PBRRootParameters::MaterialCB, materialCB);
	commandList->SetShaderResourceView(PBRRootParameters::Textures, 0, s_StoneWallAlbedo, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->SetShaderResourceView(PBRRootParameters::Textures, 1, s_StoneWallNormal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//s_TestCube.Draw(*commandList);
	s_TestSphere.Draw(*commandList);
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
