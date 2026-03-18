#include "GameObject.h"
#include "CommandList.h"
#include "PBRObjectPSO.h"
#include "OutlinePSO.h"
#include "Skybox.h"
#include "DirectionalLight.h"
#include "ShaderResourceView.h"
#include "Camera.h"
#include "Scene.h"
#include "Events.h"
#include "DirectXMath.h"
#include "Mesh.h"

#include "Logger.h"
#include <format>

using namespace DirectX;

GameObject::GameObject(CommandList& copyCommandList, GameObjectParams params, std::shared_ptr<Mesh> mesh) 
	: m_PBR_PSO(params.pbrPSO)
	, m_Outline_PSO(params.outlinePSO) 
	, m_Mesh(mesh)
	, m_Name(params.name)
{
	SetTranslation(params.translation.x, params.translation.y, params.translation.z);
	SetEulerRotation(params.eulerRotation.x, params.eulerRotation.y, params.eulerRotation.z);
	SetScale(params.scale.x, params.scale.y, params.scale.z);

	UpdateShaderResources(copyCommandList, params.pbrMatName);

	// Set rest of textures not updated in UpdateShaderResources()
	m_TextureResources[PBRObjectPSO::IrradianceCubemap]    = params.scene.GetSkybox().GetIrradianceTexture();
	m_TextureResources[PBRObjectPSO::PrefilterCubemap]     = params.scene.GetSkybox().GetPrefilterTexture();
	m_TextureResources[PBRObjectPSO::BRDFLut]              = params.scene.GetSkybox().Get_BRDF_LUT_Texture();
	m_TextureResources[PBRObjectPSO::DirectionalShadowMap] = params.scene.GetDirectionalLight().GetShadowMapTexture();
}

void GameObject::UpdateShaderResources(CommandList& copyCommandList, const std::wstring& pbrMatName) {
	m_MaterialName = pbrMatName;

	// Load Resources, note that commandlist is not executed here
	std::wstring matPathPrefix { L"assets/materials/" + pbrMatName + L"/" + pbrMatName };
	m_TextureResources[PBRObjectPSO::AlbedoTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_albedo.tga", true);
	m_TextureResources[PBRObjectPSO::NormalTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_normal.tga", false);
	m_TextureResources[PBRObjectPSO::MaterialTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_mat.tga", false);
}

GameObject::GameObject(CommandList& copyCommandList, GameObjectParams params, const std::wstring& meshFileName) {
	/// TODO: load from file with meshFileName
}

void GameObject::Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene) {
	// Render object with PBR shading
	// Note: we are setting PSO/Root sig for every game object render rather than batching and setting the state once,
	//       this could be slow, not an issue for current basic implementation
	{
		if(b_Outline) {
			m_PBR_PSO->SetStencilWritePipelineState(directCommandList);
			directCommandList.GetD3D12CommandList()->OMSetStencilRef(1);
		}
		else {
			m_PBR_PSO->SetPipelineState(directCommandList);
		}

		XMFLOAT4X4 v = scene.GetDirectionalLight().GetViewMatrix();
		XMFLOAT4X4 o = scene.GetDirectionalLight().GetOrthoMatrix();
		XMMATRIX directionalLightViewMat = XMLoadFloat4x4(&v);
		XMMATRIX directionalLightOrthoMat = XMLoadFloat4x4(&o);

		PBRObjectPSO::VertexProps pbrVertexCB {};
		XMStoreFloat4x4(&pbrVertexCB.SRT, XMLoadFloat4x4(&m_ScaleMat) * XMLoadFloat4x4(&m_RotationMat) * XMLoadFloat4x4(&m_TranslationMat));
		XMStoreFloat4x4(&pbrVertexCB.MVP, XMLoadFloat4x4(&pbrVertexCB.SRT) * scene.m_MainCamera.Get_ViewMatrix() * scene.m_MainCamera.Get_ProjectionMatrix());

		XMStoreFloat4x4(&pbrVertexCB.directionalLightMVP, XMLoadFloat4x4(&pbrVertexCB.SRT) * directionalLightViewMat * directionalLightOrthoMat);
		XMStoreFloat4(&pbrVertexCB.CameraPosition, scene.m_MainCamera.Get_Translation());

		PBRObjectPSO::MaterialProps pbrMaterialCB {};
		pbrMaterialCB.Time = { (float)e.Time, (float)e.DeltaTime, 0.0f, 0.0f };

		pbrMaterialCB.DirLight = scene.GetDirectionalLight().GetDirection();
		pbrMaterialCB.DirLightColor = scene.GetDirectionalLight().GetColor();

		m_PBR_PSO->UpdateResources(directCommandList, m_TextureResources, pbrVertexCB, pbrMaterialCB);
		m_Mesh->Draw(directCommandList);
	}

	// Render object outline by rendering slightly bigger mesh of object and front culling (this method doesn't work on quads)
	// Note: outline effect should be it's own class, preferably as some sort of component system
	//if(b_Outline){
	//	m_Outline_PSO->SetPipelineState(directCommandList);
	//	directCommandList.GetD3D12CommandList()->OMSetStencilRef(1);

	//	OutlinePSO::VertexProps outlineVertexCB {};
	//	
	//	// Magic number
	//	float outlineMeshScale = 1.05f; 
	//	XMStoreFloat4x4(&SRT, XMMatrixScaling(outlineMeshScale, outlineMeshScale, outlineMeshScale) * XMLoadFloat4x4(&SRT));
	//	outlineVertexCB.SRT = SRT;

	//	XMStoreFloat4x4(&MVP, XMLoadFloat4x4(&SRT) * scene.m_MainCamera.Get_ViewMatrix() * scene.m_MainCamera.Get_ProjectionMatrix());
	//	outlineVertexCB.MVP = MVP;

	//	OutlinePSO::MaterialProps outlineMaterialCB {};
	//	outlineMaterialCB.outlineColor = { 10.0f, 10.0f, 0.0f, 1.0f };
	//	m_Outline_PSO->UpdateResources(directCommandList, outlineVertexCB, outlineMaterialCB);
	//	m_Mesh->Draw(directCommandList);
	//}
}

/// TODO: fix DX error when rendering outline
void GameObject::RenderOutline(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene) {
	if(b_Outline) {
		m_Outline_PSO->SetPipelineState(directCommandList);
		directCommandList.GetD3D12CommandList()->OMSetStencilRef(1);

		OutlinePSO::VertexProps outlineVertexCB {};

		// Magic number
		float outlineMeshScale = 1.05f;
		XMStoreFloat4x4(
			&outlineVertexCB.SRT, XMLoadFloat4x4(&m_ScaleMat) * XMMatrixScaling(outlineMeshScale, outlineMeshScale, outlineMeshScale)
			* XMLoadFloat4x4(&m_RotationMat) 
			* XMLoadFloat4x4(&m_TranslationMat)
		);
		XMStoreFloat4x4(&outlineVertexCB.MVP, XMLoadFloat4x4(&outlineVertexCB.SRT) * scene.m_MainCamera.Get_ViewMatrix() * scene.m_MainCamera.Get_ProjectionMatrix());

		OutlinePSO::MaterialProps outlineMaterialCB {};
		outlineMaterialCB.outlineColor = { 10.0f, 10.0f, 0.0f, 1.0f };
		m_Outline_PSO->UpdateResources(directCommandList, outlineVertexCB, outlineMaterialCB);
		m_Mesh->Draw(directCommandList);
	}

}

// we can use a dirty flag to only update SRT when neccesary
// assume we are in right rendering pipeline (see DirectionalLight::SetShadowDepthPipelineStateAndRenderTarget)
void GameObject::RenderToDirectionalShadowMap(CommandList& directCommandList, const DirectionalLight& directionalLight) {
	XMMATRIX SRTMat = XMLoadFloat4x4(&m_ScaleMat) * XMLoadFloat4x4(&m_RotationMat) * XMLoadFloat4x4(&m_TranslationMat);
	directionalLight.RenderObjectToDepth(directCommandList, *m_Mesh, SRTMat);
}

void GameObject::Translate(float x, float y, float z) {
	SetTranslation(m_Translation.x + x, m_Translation.y + y, m_Translation.z + z);
}

void GameObject::EulerRotate(float x, float y, float z) {
	SetEulerRotation(m_EulerRotation.x + x,m_EulerRotation.y + y, m_EulerRotation.z + z);
}

void GameObject::Scale(float x, float y, float z) {
	SetScale(m_Scale.x * x, m_Scale.y * y, m_Scale.z * z);
}

void GameObject::SetTranslation(float x, float y, float z) {
	m_Translation = { x, y, z };
	XMStoreFloat4x4(&m_TranslationMat, XMMatrixTranslation(x, y, z));
	m_AABB.Center = m_Translation;
}

void GameObject::SetEulerRotation(float x, float y, float z) {
	m_EulerRotation = { x, y, z };
	XMStoreFloat4x4(&m_RotationMat, XMMatrixRotationRollPitchYaw(x, y, z));
}

void GameObject::SetScale(float x, float y, float z) {
	m_Scale = {x, y, z};
	XMStoreFloat4x4(&m_ScaleMat, XMMatrixScaling(x, y, z));
	
	XMFLOAT3 meshExtents = m_Mesh->GetExtents();
	m_AABB.Extents.x = meshExtents.x * m_Scale.x;
	m_AABB.Extents.y = meshExtents.y * m_Scale.y;
	m_AABB.Extents.z = meshExtents.z * m_Scale.z;
}

