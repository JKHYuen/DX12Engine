#include "Camera.h"
#include "GameObject.h"
#include "PBRObjectPSO.h"
#include "PointLight.h"

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

	auto shadowCubemap = std::make_shared<Texture>(device, shadowCubemapDesc);

	std::wstring wName {};
	StringConvert::String_To_WideString(name, wName);
	shadowCubemap->SetName(wName + L" Shadow Cubemap");

	m_ShadowCubemap_RT = std::make_unique<RenderTarget>();
	m_ShadowCubemap_RT->AttachTexture(AttachmentPoint::DepthStencil, shadowCubemap);
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

void PointLight::SetShadowDepthPipelineStateAndRenderTarget(CommandList& directCommandList) const {
	//directCommandList.ClearDepthStencilTexture(m_DirectionalShadowMapRT->GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
	//directCommandList.SetViewport(m_ViewPort);
	//directCommandList.SetRenderTarget(*m_ShadowCubemap_RT);

	//m_DepthPSO->SetPipelineState(directCommandList);
}

void PointLight::RenderObjectToDepth(CommandList& directCommandList, Mesh& mesh, PBRVertexProps vertexProps, const PBRTessellationProps& tessProps) const {
	//// Use directional light view/proj matrix and all other copied values from vertexProps
	//XMStoreFloat4x4(&vertexProps.MVP, XMLoadFloat4x4(&vertexProps.SRT) * XMLoadFloat4x4(&m_LightViewMatrix) * XMLoadFloat4x4(&m_LightOrthoMatrix));

	//directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::PBRRootParameters::VertexCB, vertexProps);
	//directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::PBRRootParameters::TessellationCB, tessProps);
	//mesh.Draw(directCommandList);
}
