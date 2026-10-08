#include "PointLight.h"

#include "Camera.h"
#include "EditorGui.h"
#include "GameObject.h"
#include "PBRObjectPSO.h"
#include "RenderConstants.h"
#include "DepthPSO.h"
#include "UnlitPSO.h"
#include "StringHelpers.h"

#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/Mesh.h"
#include <DX12EngineCore/RenderTarget.h>
#include "DX12EngineCore/Texture.h"

#include <DirectXMath.h>
#include <DirectXMathConvert.inl>
#include <DirectXMathMatrix.inl>
#include <memory>
#include <string>
#include <d3dx12_core.h>

using namespace DirectX;
using namespace RenderEnums;
using namespace RenderGlobals;

namespace {
	constexpr UINT sk_ShadowCubemapResolution = 1024;
}

PointLight::PointLight(Device& device, const std::string& name, PointLightParams params)
	: GameObject(params.translation, 0.3f, params.visualizationMesh, name)
	, m_Color(params.color)
	, m_Radius(params.radius)
	, m_VisualIntensity(0.3f)
	, m_VisualMeshScale(0.3f)
	, m_VisualizationMesh(params.visualizationMesh)
	, m_UnlitPSO(params.unlitPSO)
	, m_DepthPSO(params.depthPSO)
{
	auto shadowCubemapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_D32_FLOAT, sk_ShadowCubemapResolution, sk_ShadowCubemapResolution,
		6, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	);

	D3D12_CLEAR_VALUE depthClearValue {};
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthClearValue.DepthStencil = { 0.0f, 0 };

	auto shadowCubemap = std::make_shared<Texture>(device, shadowCubemapDesc, &depthClearValue);

	std::wstring wName {};
	StringConvert::String_To_WideString(name, wName);
	shadowCubemap->SetName(wName + L" Shadow Cubemap");

	m_ShadowCubemap_RT = std::make_unique<RenderTarget>();
	m_ShadowCubemap_RT->AttachTexture(AttachmentPoint::DepthStencil, shadowCubemap);

	// Create cubemap SRV for shadow map reading
	D3D12_SHADER_RESOURCE_VIEW_DESC cubeMapSRVDesc {};
	cubeMapSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	cubeMapSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	cubeMapSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	cubeMapSRVDesc.TextureCube.MipLevels = -1;
	shadowCubemap->CreateShaderResourceView(cubeMapSRVDesc);

	for(int i = 0; i < 6; i++) {
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = i;
		dsvDesc.Texture2DArray.ArraySize = 1;
		m_CubemapDSVs[i] = dsvDesc;
	}
}

PointLight::PointLight(Device& device, PointLightParams params)
	: PointLight(device, "Point Light", params)
{}

void PointLight::RenderMesh(CommandList& directCommandList, const UpdateEventArgs& e, const Camera& viewCamera) {
	m_UnlitPSO->SetPipelineState(directCommandList, RenderFlags_None);

	PBRVertexProps vertexProps {};
	XMStoreFloat4x4(&vertexProps.SRT, 
		XMMatrixMultiply(XMMatrixScaling(m_VisualMeshScale, m_VisualMeshScale, m_VisualMeshScale), XMMatrixTranslation(m_Translation.x, m_Translation.y, m_Translation.z))
	);
	XMStoreFloat4x4(
		&vertexProps.MVP,
		XMMatrixMultiply(
			XMMatrixMultiply(XMLoadFloat4x4(&vertexProps.SRT), viewCamera.Get_ViewMatrix()),
			viewCamera.Get_ProjectionMatrix()
		)
	);
	vertexProps.color = XMFLOAT4(m_Color.x * m_VisualIntensity, m_Color.y * m_VisualIntensity, m_Color.z * m_VisualIntensity, 1.0f);

	// Simple mesh render will not use tessellation, pass empty struct
	PBRTessellationProps tessProps {};

	m_UnlitPSO->UpdateResources(directCommandList, vertexProps, tessProps);
	m_VisualizationMesh->Draw(directCommandList);
}

// Could be a static function
void PointLight::SetShadowDepthPipelineState(CommandList& directCommandList) const {
	m_DepthPSO->SetPipelineState(directCommandList);

	constexpr D3D12_VIEWPORT sk_Viewport { 0.0f, 0.0f, sk_ShadowCubemapResolution, sk_ShadowCubemapResolution, 1.0, 0.0f };
	directCommandList.SetViewport(sk_Viewport);
}

void PointLight::RenderObjectToDepth(CommandList& directCommandList, Mesh& mesh, PBRVertexProps vertexProps, const PBRTessellationProps& tessProps) const {
	/// TODO: near, far values probably need to be tweaked
	static XMMATRIX cubemapProjectionMat = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.0f), 1.0f, m_Radius, 0.01f);

	XMMATRIX cubeMapCaptureViewMats[] = {
		XMMatrixLookAtLH(XMLoadFloat3(&m_Translation), XMLoadFloat3(&m_Translation) + XMLoadFloat3(&float3_100),  XMLoadFloat3(&float3_010)),
		XMMatrixLookAtLH(XMLoadFloat3(&m_Translation), XMLoadFloat3(&m_Translation) + XMLoadFloat3(&float3_n100), XMLoadFloat3(&float3_010)),
		XMMatrixLookAtLH(XMLoadFloat3(&m_Translation), XMLoadFloat3(&m_Translation) + XMLoadFloat3(&float3_010),  XMLoadFloat3(&float3_00n1)),
		XMMatrixLookAtLH(XMLoadFloat3(&m_Translation), XMLoadFloat3(&m_Translation) + XMLoadFloat3(&float3_0n10), XMLoadFloat3(&float3_001)),
		XMMatrixLookAtLH(XMLoadFloat3(&m_Translation), XMLoadFloat3(&m_Translation) + XMLoadFloat3(&float3_001),  XMLoadFloat3(&float3_010)),
		XMMatrixLookAtLH(XMLoadFloat3(&m_Translation), XMLoadFloat3(&m_Translation) + XMLoadFloat3(&float3_00n1), XMLoadFloat3(&float3_010)),
	};

	/// TODO: make this work 
	for(int i = 0; i < 6; i++) {
		m_ShadowCubemap_RT->GetTexture(AttachmentPoint::DepthStencil)->CreateDepthStencilResourceView(m_CubemapDSVs[i]);
		directCommandList.SetRenderTarget(*m_ShadowCubemap_RT);

		// Use Point light view/proj matrix with other copied values from vertexProps
		XMStoreFloat4x4(&vertexProps.MVP, XMLoadFloat4x4(&vertexProps.SRT) * cubeMapCaptureViewMats[i] * cubemapProjectionMat);

		directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::PBRRootParameters::VertexCB, vertexProps);
		directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::PBRRootParameters::TessellationCB, tessProps);
		mesh.Draw(directCommandList);
	}
}

void PointLight::ClearShadowCubemap(CommandList& directCommandList) {
	for(int i = 0; i < 6; i++) {
		m_ShadowCubemap_RT->GetTexture(AttachmentPoint::DepthStencil)->CreateDepthStencilResourceView(m_CubemapDSVs[i]);
		directCommandList.ClearDepthStencilTexture(m_ShadowCubemap_RT->GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH, 0.0f);
	}
}

std::shared_ptr<Texture> PointLight::GetShadowDepthTexture() const {
	return m_ShadowCubemap_RT->GetTexture(AttachmentPoint::DepthStencil);
}
