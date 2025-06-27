#include "Skybox.h"
#include <DirectXMath.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <array>
#include <memory>

#include "Device.h"
#include "RootSignature.h"
#include "RenderTarget.h"
#include "CommandList.h"
#include "ShaderResourceView.h"
#include "Mesh.h"
#include "Camera.h"
#include "Helpers.h"

using namespace DirectX;

namespace {
	Microsoft::WRL::ComPtr<ID3D12PipelineState> ms_SkyboxPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> ms_ConvolutionPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> ms_PrefilterPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> ms_IntegratePSO;

	std::unique_ptr<Mesh> s_SkyboxCube;
	bool s_IsInitialized = false;

}

Skybox::Skybox(Device& device, CommandList& copyCommandList, std::wstring hdrTextureName, std::unique_ptr<Mesh> reversedCube, RenderTarget& renderTarget)
	: m_Device(device) {

	// TODO: skip one time initialization operations
	s_IsInitialized = true;

	s_SkyboxCube = std::move(reversedCube);
	m_HDRPanoTexture = copyCommandList.LoadTextureFromFile(L"assets/" + hdrTextureName + L".hdr", true);

	auto cubemapDesc = m_HDRPanoTexture->GetD3D12ResourceDesc();
	// TODO: make face resolution tweakable
	cubemapDesc.Width = cubemapDesc.Height = 1024;
	cubemapDesc.DepthOrArraySize = 6;
	cubemapDesc.MipLevels = 0;
	
	m_SkyCubemapTexture = std::make_shared<Texture>(m_Device, cubemapDesc);
	m_SkyCubemapTexture->SetName(hdrTextureName + L" Skybox Cubemap");

	copyCommandList.PanoToCubemap(m_SkyCubemapTexture, m_HDRPanoTexture);

	D3D12_SHADER_RESOURCE_VIEW_DESC cubeMapSRVDesc = {};
	cubeMapSRVDesc.Format = cubemapDesc.Format;
	cubeMapSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	cubeMapSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	cubeMapSRVDesc.TextureCube.MipLevels = (UINT)-1;  // Use all mips.

	m_SkyCubemapSRV = std::make_shared<ShaderResourceView>( m_Device, m_SkyCubemapTexture, &cubeMapSRVDesc);

	// Load shaders
	Microsoft::WRL::ComPtr<ID3DBlob> vs;
	Microsoft::WRL::ComPtr<ID3DBlob> ps;
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Skybox_VS.cso", &vs));
	ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/Skybox_PS.cso", &ps));

	D3D12_INPUT_ELEMENT_DESC inputLayout[1] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = 
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameters[2];
	rootParameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[1].InitAsDescriptorTable(1, &descriptorRange, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(
		0, D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, 
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription {};
	rootSignatureDescription.Init_1_1(2, rootParameters, 1, &linearClampSampler, rootSignatureFlags);

	m_SkyboxRootSignature = std::make_shared<RootSignature>(m_Device, rootSignatureDescription.Desc_1_1);

	// Setup the Skybox pipeline state.
	struct SkyboxPipelineState {
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS                    PS;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC           SampleDesc;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            RasterizerDesc;
	} skyboxPipelineStateStream;

	CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
	rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;

	skyboxPipelineStateStream.pRootSignature = m_SkyboxRootSignature->GetD3D12RootSignature().Get();
	skyboxPipelineStateStream.InputLayout = {inputLayout, 1};
	skyboxPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	skyboxPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	skyboxPipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(ps.Get());
	skyboxPipelineStateStream.RTVFormats = renderTarget.GetRenderTargetFormats();
	skyboxPipelineStateStream.SampleDesc = renderTarget.GetSampleDesc();
	skyboxPipelineStateStream.RasterizerDesc = rasterizerDesc;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {sizeof(SkyboxPipelineState), &skyboxPipelineStateStream};
	ThrowIfFailed(m_Device.GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&ms_SkyboxPSO)));
}

void Skybox::Render(CommandList& directCommandList, const Camera& camera) {
	auto viewMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(camera.get_Rotation()));
	auto projMatrix = camera.get_ProjectionMatrix();
	auto viewProjMatrix = viewMatrix * projMatrix;

	directCommandList.SetPipelineState(ms_SkyboxPSO);
	directCommandList.SetGraphicsRootSignature(m_SkyboxRootSignature);
					 
	directCommandList.SetGraphics32BitConstants(0, viewProjMatrix);
	directCommandList.SetShaderResourceView(1, 0, m_SkyCubemapSRV, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	s_SkyboxCube->Draw(directCommandList);
}