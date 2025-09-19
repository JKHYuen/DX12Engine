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

/// TODO: TEMP, need scene/gameobject system to replace all this
namespace {
	// TODO: maybe move root sig / PSO creation in new class
	enum PBRRootParameters {
		VertexCB,         // ConstantBuffer<Mat> VertexCB : register(b0);
		MaterialCB,       // ConstantBuffer<Material> MaterialCB : register( b0, space1 );
		Textures,         // Texture2D AlbedoTex            : register(t0);
						  // Texture2D NormalTex            : register(t1);
						  // Texture2D MaterialTex          : register(t2);
						  // Texture2D IrradianceCubemap    : register(t3);
						  // Texture2D PrefilterCubemap     : register(t4);
						  // Texture2D BRDFLut              : register(t5);
		VolatileTextures, // Texture2D DirectionalShadowMap : register(t6);		
		
		NumRootParameters
	};

	struct VertexProps {
		XMFLOAT4X4 SRT;
		XMFLOAT4X4 MVP;
		XMFLOAT4X4 directionalLightMVP;
		XMFLOAT4   CameraPosition;
		XMFLOAT4   Pad1;
		XMFLOAT4   Pad2;
		XMFLOAT4   Pad3;
	};

	struct MaterialProps {
		XMFLOAT4   Time;
		XMFLOAT4   DirLight;
		XMFLOAT4   DirLightColor;
		XMFLOAT4   Pad2;
		XMFLOAT4X4 Pad3;
		XMFLOAT4X4 Pad4;
		XMFLOAT4X4 Pad5;
	};

	/// TODO: TEMP
	// Textures are shared pointers for texture cache use in CommandList class
	std::shared_ptr<Texture> s_Test_Albedo;
	std::shared_ptr<Texture> s_Test_Normal;
	std::shared_ptr<Texture> s_Test_Material; // r: AO, g: metallic, b: roughness, a: height 

	std::unique_ptr<Mesh> s_TestFloorMesh;
	std::unique_ptr<Mesh> s_TestMesh_0;

	EditorGui::GuiDescriptorAllocation s_GuiShadowMapDebugSRV;

	DirectionalLight s_DirectionalLight {XMFLOAT3(9.0f, 8.0f, 7.0f), XMFLOAT3(50.0f, 230.0f, 0.0f), s_ShadowMapResolution, s_ShadowDistance, s_ShadowMapNear, s_ShadowMapFar, s_ShadowBias};

	std::shared_ptr<ShaderResourceView> s_ShadowMapSRV;

	/// TODO: move functions below to CommandList.cpp?
	// Algorithm from https://rastertek.com/dx11win10tut20.html
	// Refactored and simplified by KHY
	void CalculateModelVectors(std::vector<VertexInput>& vertices, const std::vector<uint16_t>& indices) {
		XMVECTOR tangent {}, bitangent {};
		float vector1[3] {}, vector2[3] {};
		float tuVector[2] {}, tvVector[2] {};

		size_t triangleCount = indices.size() / 3;

		// Index of index buffer
		size_t i = 0;

		// Go through all triangles and calculate the the tangent and bitangent vectors
		for(int f = 0; f < triangleCount; f++) {
			VertexInput vertex1 = vertices[indices[i++]];
			VertexInput vertex2 = vertices[indices[i++]];
			VertexInput vertex3 = vertices[indices[i++]];

			// Calculate tangent and bitangent
			{
				// Calculate the two vectors for this face
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

				// Calculate the cross products, multiply by the coefficient and normalize to get the tangent and bitangent
				tangent = XMVector3Normalize(XMVectorSet(
					(tvVector[1] * vector1[0] - tvVector[0] * vector2[0]),
					(tvVector[1] * vector1[1] - tvVector[0] * vector2[1]),
					(tvVector[1] * vector1[2] - tvVector[0] * vector2[2]), 1.0f)
				);
				bitangent = XMVector3Normalize(XMVectorSet(
					(tuVector[0] * vector2[0] - tuVector[1] * vector1[0]),
					(tuVector[0] * vector2[1] - tuVector[1] * vector1[1]),
					(tuVector[0] * vector2[2] - tuVector[1] * vector1[2]), 1.0f)
				);
			}

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

	std::unique_ptr<Mesh> CreateCube(CommandList& commandList, float size = 1.0f) {
		// Cube is centered at 0,0,0
		float s = size * 0.5f;

		// 8 edges of cube.
		XMFLOAT3 p[8] = {{ s, s, -s }, { s, s, s }, { s, -s, s }, { s, -s, -s },{ -s, s, s }, { -s, s, -s }, { -s, -s, -s }, { -s, -s, s }};
		// 6 face normals
		XMFLOAT3 n[6] = {{ 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }};
		// 4 unique texture coordinates
		XMFLOAT3 uv[4] = {{ 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }};

		// Indices for the vertex positions.
		size_t i[24] = {
			0, 1, 2, 3,  // +X
			4, 5, 6, 7,  // -X
			4, 1, 0, 5,  // +Y
			2, 7, 6, 3,  // -Y
			1, 4, 7, 2,  // +Z
			5, 0, 3, 6   // -Z
		};

		std::vector<VertexInput> vertices;
		std::vector<uint16_t>  indices;

		for(int f = 0; f < 6; ++f)  // For each face of the cube.
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

		auto cubeMeshPtr = std::make_unique<Mesh>();
		cubeMeshPtr->SetVertexBuffer(0, vertexBuffer);
		cubeMeshPtr->SetIndexBuffer(indexBuffer);

		return cubeMeshPtr;
	}

	std::unique_ptr<Mesh> CreateSphere(CommandList& commandList, float radius, uint32_t tessellation) {

		if(tessellation < 3)
			throw std::out_of_range("tessellation parameter out of range");

		std::vector<VertexInput> vertices;
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
		uint16_t stride = (uint16_t)horizontalSegments + 1;

		for(uint16_t i = 0; i < verticalSegments; i++) {
			for(uint16_t j = 0; j < horizontalSegments; j++) {
				uint16_t nextI = i + 1;
				uint16_t nextJ = (j + 1) % stride;

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

		auto sphereMeshPtr = std::make_unique<Mesh>();
		sphereMeshPtr->SetVertexBuffer(0, vertexBuffer);
		sphereMeshPtr->SetIndexBuffer(indexBuffer);

		return sphereMeshPtr;
	}

	std::unique_ptr<Mesh> CreateQuad(CommandList& commandList, float width, float height) {
		// Define a plane that is aligned with the X-Z plane and the normal is facing up in the Y-axis.
		std::vector<VertexInput> vertices = {
			VertexInput(XMFLOAT3(-0.5f * width, 0.0f, 0.5f * height),  XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 0.0f)), // 0
			VertexInput(XMFLOAT3(0.5f * width, 0.0f, 0.5f * height),   XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)), // 1
			VertexInput(XMFLOAT3(0.5f * width, 0.0f, -0.5f * height),  XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(1.0f, 1.0f, 0.0f)), // 2
			VertexInput(XMFLOAT3(-0.5f * width, 0.0f, -0.5f * height), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f))  // 3
		};

		std::vector<uint16_t> indices = {1, 3, 0, 2, 3, 1};

		CalculateModelVectors(vertices, indices);

		auto vertexBuffer = commandList.CopyVertexBuffer(vertices);
		auto indexBuffer = commandList.CopyIndexBuffer(indices);

		auto planeMeshPtr = std::make_unique<Mesh>();
		planeMeshPtr ->SetVertexBuffer(0, vertexBuffer);
		planeMeshPtr ->SetIndexBuffer(indexBuffer);

		return planeMeshPtr;
	}
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
	, m_Width(width)
	, m_Height(height)
	, m_IsVsync(vSync)
	, m_Camera()
	, m_HDR_MSAA_RenderTarget() 
	, m_FloatRenderTarget()
	, m_DirectionalShadowMap() {

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
			sk_HDRFormat, m_Width, m_Height, 1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
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
			sk_DepthBufferFormat, m_Width, m_Height, 1, 1, sampleDesc.Count, sampleDesc.Quality, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
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
			sk_HDRFormat, m_Width, m_Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
		);

		auto floatRenderTexture = std::make_shared<Texture>(*m_Device, floatTextureDesc, &colorClearValue);
		floatRenderTexture->SetName(L"Screen Floating Point Render Target");
		m_FloatRenderTarget.AttachTexture(AttachmentPoint::Color0, floatRenderTexture);

		/// TODO: store this in directional light?
		// Create directional light shadow map
		auto shadowMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			sk_DepthBufferFormat, s_ShadowMapResolution, s_ShadowMapResolution, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
		);

		auto shadowMapDepthTexture = std::make_shared<Texture>(*m_Device, shadowMapDesc, &depthClearValue);
		shadowMapDepthTexture->SetName(L"Directional Light Shadow Map");
		m_DirectionalShadowMap.AttachTexture(AttachmentPoint::DepthStencil, shadowMapDepthTexture);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		s_ShadowMapSRV = std::make_shared<ShaderResourceView>(*m_Device, shadowMapDepthTexture, &srvDesc);

		// Initialize ImGui SRV for debug
		// Note: SRVs for ImGui render have its own allocator and descriptor heap (instead of the two stage DynamicDescriptorHeap system) to keep things simple
		s_GuiShadowMapDebugSRV = EditorGui::AllocateImageSRV(*m_Device, m_DirectionalShadowMap.GetTexture(AttachmentPoint::DepthStencil), &srvDesc);
	}

	auto& copyCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	/// Load Assets (COPY operations)
	{
		auto copyCommandList = copyCommandQueue.GetCommandList();

		/// TODO / TEMP: more dynamic scene object loading
		s_TestFloorMesh = CreateQuad(*copyCommandList, 1.0f, 1.0f);
		s_TestMesh_0 = CreateSphere(*copyCommandList, 1.0f, 64);

		std::wstring matName = L"stonewall";
		s_Test_Albedo = copyCommandList->LoadTextureFromFile(L"assets/materials/" + matName + L"_albedo.tga", true);
		s_Test_Normal = copyCommandList->LoadTextureFromFile(L"assets/materials/" + matName + L"_normal.tga", false);
		s_Test_Material = copyCommandList->LoadTextureFromFile(L"assets/materials/" + matName + L"_mat.tga", false);

		// Load Skybox Assets
		std::wstring skyboxName = L"industrial_sunset_puresky_4k";
		m_Skybox = std::make_unique<Skybox>(*m_Device, *copyCommandList, skyboxName, CreateCube(*copyCommandList, 1.0f), m_HDR_MSAA_RenderTarget);

		copyCommandQueue.ExecuteCommandList(copyCommandList);
	}

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags_VSPS =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	/// Create PBR Pipeline State (For rendering PBR objects)
	{
		// Load PBR shaders
		ComPtr<ID3DBlob> vs;
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PBR_VS.cso", &vs));
		ComPtr<ID3DBlob> ps;
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PBR_PS.cso", &ps));

		// PBR root signature
		CD3DX12_ROOT_PARAMETER1 rootParameters[PBRRootParameters::NumRootParameters];
		rootParameters[PBRRootParameters::VertexCB].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[PBRRootParameters::MaterialCB].InitAsConstantBufferView(0, 1, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);

		// Static descriptors
		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0);
		rootParameters[PBRRootParameters::Textures].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		// Volatile descriptors
		CD3DX12_DESCRIPTOR_RANGE1 volatileDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 0U, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
		rootParameters[PBRRootParameters::VolatileTextures].InitAsDescriptorTable(1, &volatileDescriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC anisotropicSampler(0, D3D12_FILTER_ANISOTROPIC);
		CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(1, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		CD3DX12_STATIC_SAMPLER_DESC samplers[] = {anisotropicSampler, linearClampSampler};

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(PBRRootParameters::NumRootParameters, rootParameters, 2, samplers, rootSignatureFlags_VSPS);

		m_PBRRootSignature = std::make_shared<RootSignature>(*m_Device, rootSignatureDescription.Desc_1_1);

		struct HDRPipelineStateStream {
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS VS;
			CD3DX12_PIPELINE_STATE_STREAM_PS PS;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
			CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
			CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
		} hdrPipelineStateStream;

		hdrPipelineStateStream.pRootSignature = m_PBRRootSignature->GetD3D12RootSignature().Get();
		hdrPipelineStateStream.InputLayout = VertexInput::GetInputLayout();
		hdrPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		hdrPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		hdrPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
		hdrPipelineStateStream.DSVFormat = sk_DepthBufferFormat;
		hdrPipelineStateStream.RTVFormats = m_HDR_MSAA_RenderTarget.GetRenderTargetFormats();
		hdrPipelineStateStream.SampleDesc = sampleDesc;

		// TODO: move this to device class
		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {sizeof(HDRPipelineStateStream), &hdrPipelineStateStream};
		ThrowIfFailed(m_Device->GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_PBR_PSO)));
	}

	/// Create Post Process/Tonemap Pipeline States 
	/// Note: post process pipeline currently unused, will be used for bloom eventually
	{
		CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParameters[1];
		rootParameters[0].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(
			0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, 
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
		rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 1, &linearClampSampler, rootSignatureFlags_VSPS);
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

	/// TODO: store this in directional light?
	/// Create Depth Only Render Pipeline State (for shadow mapping)
	{
		/// TODO: we are loading basic model VS again, shader blobs should probably be cached
		ComPtr<ID3DBlob> vs;
		ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PBR_VS.cso", &vs));

		struct ShadowDepthPipelineStateStream {
			CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
			CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
			CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
			CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
			CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            Rasterizer;
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DSVFormat;
		} shadowDepthPipelineStateStream;

		CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
		rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;

		shadowDepthPipelineStateStream.pRootSignature = m_PBRRootSignature->GetD3D12RootSignature().Get();
		shadowDepthPipelineStateStream.InputLayout = VertexInput::GetInputLayout();
		shadowDepthPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		shadowDepthPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
		shadowDepthPipelineStateStream.Rasterizer = rasterizerDesc;
		shadowDepthPipelineStateStream.DSVFormat = m_DirectionalShadowMap.GetDepthStencilFormat();

		D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = { sizeof(ShadowDepthPipelineStateStream), &shadowDepthPipelineStateStream };
		ThrowIfFailed(m_Device->GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_ShadowDepthPSO)));
	}

	// Wait for loading operations to complete before rendering the first frame
	copyCommandQueue.FlushWait();  

	// Precompute skybox IBL textures
	auto& directCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto directCommandList = directCommandQueue.GetCommandList();
	m_Skybox->ComputeIBLMaps(*directCommandList, m_Camera);
	directCommandQueue.ExecuteCommandList(directCommandList);

	return true;
}

void DemoGame::OnResize(ResizeEventArgs& e) {
	m_Width = std::max(1, e.Width);
	m_Height = std::max(1, e.Height);

	m_SwapChain->Resize(m_Width, m_Height);

	float aspectRatio = m_Width / (float)m_Height;
	/// TODO: Define default z values somewhere
	m_Camera.set_Projection(45.0f, aspectRatio, 0.1f, 1000.0f);

	m_ScreenViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_Width), static_cast<float>(m_Height));
	m_HDR_MSAA_RenderTarget.Resize(m_Width, m_Height);
	m_FloatRenderTarget.Resize(m_Width, m_Height);
}

// NOTE: might not be needed?
void DemoGame::UnloadContent() {
	m_HDR_MSAA_RenderTarget.Reset();
	m_FloatRenderTarget.Reset();

	m_PBR_PSO.Reset();
	m_TonemapPSO.Reset();
	m_PostprocessPSO.Reset();

	m_PBRRootSignature.reset();
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

		m_Camera.Translate(cameraTranslation, Space::Local);
		m_Camera.Translate(cameraPan, Space::Local);

		XMVECTOR cameraRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(-m_Pitch), XMConvertToRadians(-m_Yaw), 0.0f);
		m_Camera.set_Rotation(cameraRotation);
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
			static float fov = m_Camera.get_FoV();
			ImGui::SliderFloat("FOV", &fov, 12.0f, 90.0f);
			m_Camera.set_FoV(fov);
		}

		static float sceneDirLightAngle[2] { 50.0f, 230.0f };
		if(ImGui::DragFloat2("[x, y]", sceneDirLightAngle, 0.1f, 0.0f, 1000.0f, "%.2f", kSliderFlags)) {
			sceneDirLightAngle[0] = std::fmod(sceneDirLightAngle[0], 360.0f);
			sceneDirLightAngle[1] = std::fmod(sceneDirLightAngle[1], 360.0f);
			s_DirectionalLight.SetDirection(sceneDirLightAngle[0], sceneDirLightAngle[1], 0.0f);
		}

		{
			static float imageScale = 0.3f;
			ImGui::SliderFloat("Scale", &imageScale, 0.0, 1.0, "%.2fx");
			ImVec2 imageSize = ImVec2(1920.0f * imageScale, 1080.0f * imageScale);

			ImGui::Image(
				(ImTextureID)s_GuiShadowMapDebugSRV.gpuHandle.ptr, imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f)
			);
		}

		ImGui::End();
	}


	m_EditorGui->Render(directCommandList);
}

void DemoGame::OnRender(UpdateEventArgs& e) {
	auto& directCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto directCommandList = directCommandQueue.GetCommandList();

	/// TEMP: Render Test Scene
	// Clear the render targets.
	float clearColor[] = {0.6f, 0.6f, 0.7f, 1.0f};
	directCommandList->ClearTexture(m_HDR_MSAA_RenderTarget.GetTexture(AttachmentPoint::Color0), clearColor);
	directCommandList->ClearDepthStencilTexture(m_HDR_MSAA_RenderTarget.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH);

	// Setup command list for HDR rendering to intermediate render target (before multisample resolve)
	directCommandList->SetScissorRect(m_DefaultScissorRect);
	directCommandList->SetViewport(m_ScreenViewport);
	directCommandList->SetRenderTarget(m_HDR_MSAA_RenderTarget);

	m_Skybox->Render(*directCommandList, m_Camera);

	XMFLOAT4X4 v = s_DirectionalLight.GetViewMatrix();
	XMFLOAT4X4 o = s_DirectionalLight.GetOrthoMatrix();
	XMMATRIX directionalLightViewMat = XMLoadFloat4x4(&v);
	XMMATRIX directionalLightOrthoMat = XMLoadFloat4x4(&o);

	{
		directCommandList->SetPipelineState(m_PBR_PSO);
		directCommandList->SetGraphicsRootSignature(m_PBRRootSignature);

		/// Render Objects
		/// Floor
		XMMATRIX translationMat = XMMatrixTranslation(1.0f, 1.0f, 1.0f);
		XMMATRIX rotationMat = XMMatrixIdentity();
		XMMATRIX scaleMat = XMMatrixScaling(40.0f, 1.0f, 40.0f);
		XMMATRIX SRTMat = scaleMat * rotationMat * translationMat;
		VertexProps vertexCB;
		XMStoreFloat4x4(&vertexCB.SRT, SRTMat);
		XMStoreFloat4x4(&vertexCB.MVP, SRTMat * m_Camera.get_ViewMatrix() * m_Camera.get_ProjectionMatrix());
		XMStoreFloat4x4(&vertexCB.directionalLightMVP, SRTMat * directionalLightViewMat * directionalLightOrthoMat);
		XMStoreFloat4(&vertexCB.CameraPosition, m_Camera.get_Translation());

		directCommandList->SetGraphicsDynamicConstantBuffer(PBRRootParameters::VertexCB, vertexCB);

		// Pixel Shader Buffers
		// TODO: lighting vars
		MaterialProps materialCB;
		XMVECTORF32 timeVec = { (float)e.Time, (float)e.DeltaTime, 0.0f, 0.0f };
		XMStoreFloat4(&materialCB.Time, timeVec);

		materialCB.DirLight = s_DirectionalLight.GetDirection();
		materialCB.DirLightColor = s_DirectionalLight.GetColor();

		directCommandList->SetGraphicsDynamicConstantBuffer(PBRRootParameters::MaterialCB, materialCB);
		directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 0, s_Test_Albedo, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 1, s_Test_Normal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 2, s_Test_Material, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 3, m_Skybox->GetIrradianceSRV(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 4, m_Skybox->GetPrefilterSRV(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 5, m_Skybox->Get_BRDF_LUT_SRV(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		directCommandList->SetShaderResourceView(PBRRootParameters::VolatileTextures, 0, s_ShadowMapSRV, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		s_TestFloorMesh->Draw(*directCommandList);

		/// Object above floor
		translationMat = XMMatrixTranslation(1.0f, 4.0f, 1.0f);
		rotationMat = XMMatrixIdentity();
		scaleMat = XMMatrixScaling(1.0f, 1.0f, 1.0f);
		SRTMat = scaleMat * rotationMat * translationMat;
		XMStoreFloat4x4(&vertexCB.SRT, SRTMat);
		XMStoreFloat4x4(&vertexCB.MVP, SRTMat * m_Camera.get_ViewMatrix() * m_Camera.get_ProjectionMatrix());
		XMStoreFloat4x4(&vertexCB.directionalLightMVP, SRTMat * directionalLightViewMat * directionalLightOrthoMat);
		XMStoreFloat4(&vertexCB.CameraPosition, m_Camera.get_Translation());
		directCommandList->SetGraphicsDynamicConstantBuffer(PBRRootParameters::VertexCB, vertexCB);
		s_TestMesh_0->Draw(*directCommandList);
	}

	/// Render depth from directional light for same objects above
	{
		directCommandList->ClearDepthStencilTexture(m_DirectionalShadowMap.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
		directCommandList->SetRenderTarget(m_DirectionalShadowMap);
		directCommandList->SetPipelineState(m_ShadowDepthPSO);
		directCommandList->SetGraphicsRootSignature(m_PBRRootSignature);
		directCommandList->SetViewport(s_DirectionalLight.GetViewPort());

		XMMATRIX translationMat = XMMatrixTranslation(1.0f, 1.0f, 1.0f);
		XMMATRIX rotationMat = XMMatrixIdentity();
		XMMATRIX scaleMat = XMMatrixScaling(20.0f, 1.0f, 20.0f);
		XMMATRIX SRTMat = scaleMat * rotationMat * translationMat;

		VertexProps shadowDepthVertexCB;
		XMStoreFloat4x4(&shadowDepthVertexCB.SRT, SRTMat);
		XMStoreFloat4x4(&shadowDepthVertexCB.MVP, SRTMat * directionalLightViewMat * directionalLightOrthoMat);
		directCommandList->SetGraphicsDynamicConstantBuffer(PBRRootParameters::VertexCB, shadowDepthVertexCB);
		s_TestFloorMesh->Draw(*directCommandList);

		translationMat = XMMatrixTranslation(1.0f, 4.0f, 1.0f);
		rotationMat = XMMatrixIdentity();
		scaleMat = XMMatrixScaling(1.0f, 1.0f, 1.0f);
		SRTMat = scaleMat * rotationMat * translationMat;
		XMStoreFloat4x4(&shadowDepthVertexCB.SRT, SRTMat);
		XMStoreFloat4x4(&shadowDepthVertexCB.MVP, SRTMat * directionalLightViewMat * directionalLightOrthoMat);

		directCommandList->SetGraphicsDynamicConstantBuffer(PBRRootParameters::VertexCB, shadowDepthVertexCB);
		s_TestMesh_0->Draw(*directCommandList);
	}

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
		auto fov = m_Camera.get_FoV();

		fov -= e.WheelDelta;
		fov = std::clamp(fov, 12.0f, 90.0f);

		m_Camera.set_FoV(fov);
	}
}
