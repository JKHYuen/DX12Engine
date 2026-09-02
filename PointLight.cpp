#include "PointLight.h"
#include "UnlitPSO.h"
#include "PBRObjectPSO.h"
#include "Camera.h"

#include "DX12EngineCore/Mesh.h"
#include "DX12EngineCore/CommandList.h"

#include <memory>
#include <DirectXMath.h>
#include <DirectXMathConvert.inl>

using namespace DirectX;

PointLight::PointLight(XMFLOAT4 color, XMFLOAT3 translation, std::shared_ptr<Mesh> visualizationMesh, UnlitPSO* unlitPSO)
	: m_Color(color)
	, m_Translation(translation)
	, m_VisualizationMesh(visualizationMesh)
	, m_UnlitPSO(unlitPSO)
{}

void PointLight::RenderMesh(CommandList& directCommandList, const UpdateEventArgs& e, const Camera& viewCamera) {
	m_UnlitPSO->SetPipelineState(directCommandList);

	PBRVertexProps vertexProps {};
	XMStoreFloat4x4(&vertexProps.SRT, XMMatrixTranslation(m_Translation.x, m_Translation.y, m_Translation.z));

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
