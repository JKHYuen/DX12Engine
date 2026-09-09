#include "Camera.h"
#include "PBRObjectPSO.h"
#include "PointLight.h"
#include "RenderEnums.h"
#include "UnlitPSO.h"

#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/Mesh.h"

#include <DirectXMath.h>
#include <DirectXMathConvert.inl>
#include <DirectXMathMatrix.inl>
#include <memory>

using namespace DirectX;
using namespace RenderEnums;

PointLight::PointLight(XMFLOAT4 color, XMFLOAT3 translation, std::shared_ptr<Mesh> visualizationMesh, UnlitPSO* unlitPSO)
	: m_Color(color)
	, m_Translation(translation)
	, m_VisualizationMesh(visualizationMesh)
	, m_UnlitPSO(unlitPSO)
{}

void PointLight::RenderMesh(CommandList& directCommandList, const UpdateEventArgs& e, const Camera& viewCamera) {
	m_UnlitPSO->SetPipelineState(directCommandList, RenderFlags_None);

	PBRVertexProps vertexProps {};
	XMStoreFloat4x4(&vertexProps.SRT, XMMatrixMultiply(XMMatrixScaling(0.3f, 0.3f, 0.3f), XMMatrixTranslation(m_Translation.x, m_Translation.y, m_Translation.z)));
	XMStoreFloat4x4(
		&vertexProps.MVP,
		XMMatrixMultiply(
			XMMatrixMultiply(XMLoadFloat4x4(&vertexProps.SRT), viewCamera.Get_ViewMatrix()),
			viewCamera.Get_ProjectionMatrix()
		)
	);
	vertexProps.color = m_Color;

	// Simple mesh render will not use tessellation, pass empty struct
	PBRTessellationProps tessProps {};

	m_UnlitPSO->UpdateResources(directCommandList, vertexProps, tessProps);
	m_VisualizationMesh->Draw(directCommandList);
}
