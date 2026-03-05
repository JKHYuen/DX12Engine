#include "GameObject.h"
#include "CommandList.h"
#include "PBRObjectPSO.h"
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
	: m_PSO(params.PSO) {

	XMStoreFloat4x4(&m_TranslationMat, XMMatrixTranslation(params.translation.x, params.translation.y, params.translation.z));
	XMStoreFloat4x4(&m_RotationMat, XMMatrixRotationRollPitchYaw(params.eulerRotation.x, params.eulerRotation.y, params.eulerRotation.z));
	XMStoreFloat4x4(&m_ScaleMat, XMMatrixScaling(params.scale.x, params.scale.y, params.scale.z));

	UpdateShaderResources(copyCommandList, params.pbrMatName);

	/// TODO: check if skybox is initialized
	m_TextureResources[PBRObjectPSO::IrradianceCubemap]    = params.scene.GetSkybox().GetIrradianceTexture();
	m_TextureResources[PBRObjectPSO::PrefilterCubemap]     = params.scene.GetSkybox().GetPrefilterTexture();
	m_TextureResources[PBRObjectPSO::BRDFLut]              = params.scene.GetSkybox().Get_BRDF_LUT_Texture();
	m_TextureResources[PBRObjectPSO::DirectionalShadowMap] = params.scene.GetDirectionalLight().GetShadowMapTexture();

	m_Mesh = mesh;

	UpdateAABBScale();
	UpdateAABBTranslation();
}

void GameObject::UpdateShaderResources(CommandList& copyCommandList, const std::wstring& pbrMatName) {
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
	// Note: we are setting PSO/Root sig for every game object render rather than batching and setting the state once,
	//       this could be slow, not an issue for current basic implementation
	m_PSO->SetPipelineState(directCommandList);

	XMFLOAT4X4 v = scene.GetDirectionalLight().GetViewMatrix();
	XMFLOAT4X4 o = scene.GetDirectionalLight().GetOrthoMatrix();
	XMMATRIX directionalLightViewMat = XMLoadFloat4x4(&v);
	XMMATRIX directionalLightOrthoMat = XMLoadFloat4x4(&o);

	XMMATRIX SRTMat = XMLoadFloat4x4(&m_ScaleMat) * XMLoadFloat4x4(&m_RotationMat) * XMLoadFloat4x4(&m_TranslationMat);
	PBRObjectPSO::VertexProps vertexCB;
	XMStoreFloat4x4(&vertexCB.SRT, SRTMat);
	XMStoreFloat4x4(&vertexCB.MVP, SRTMat * scene.m_MainCamera.Get_ViewMatrix() * scene.m_MainCamera.Get_ProjectionMatrix());
	XMStoreFloat4x4(&vertexCB.directionalLightMVP, SRTMat * directionalLightViewMat * directionalLightOrthoMat);
	XMStoreFloat4(&vertexCB.CameraPosition, scene.m_MainCamera.Get_Translation());

	PBRObjectPSO::MaterialProps materialCB;
	XMVECTORF32 timeVec = { (float)e.Time, (float)e.DeltaTime, 0.0f, 0.0f };
	XMStoreFloat4(&materialCB.Time, timeVec);

	materialCB.DirLight = scene.GetDirectionalLight().GetDirection();
	materialCB.DirLightColor = scene.GetDirectionalLight().GetColor();

	m_PSO->UpdateResources(directCommandList, m_TextureResources, vertexCB, materialCB);

	m_Mesh->Draw(directCommandList);
}

void GameObject::RenderToDirectionalShadowMap(CommandList& directCommandList, const Scene& scene) {
	// we can use a dirty flag to only update SRT when neccesary
	// assume we are in right rendering pipeline (see DirectionalLight::SetShadowDepthPipelineStateAndRenderTarget)
	XMMATRIX SRTMat = XMLoadFloat4x4(&m_ScaleMat) * XMLoadFloat4x4(&m_RotationMat) * XMLoadFloat4x4(&m_TranslationMat);
	scene.GetDirectionalLight().RenderObjectToDepth(directCommandList, *m_Mesh, SRTMat);
}

void GameObject::Translate(float x, float y, float z) {
	XMStoreFloat4x4(&m_TranslationMat, XMMatrixMultiply(XMLoadFloat4x4(&m_TranslationMat), XMMatrixTranslation(x, y, z)));
	UpdateAABBTranslation();
}

void GameObject::Rotate(float x, float y, float z) {
	XMStoreFloat4x4(&m_RotationMat, XMMatrixMultiply(XMLoadFloat4x4(&m_RotationMat), XMMatrixRotationRollPitchYaw(x, y, z)));
}

void GameObject::Scale(float x, float y, float z) {
	XMStoreFloat4x4(&m_ScaleMat, XMMatrixMultiply(XMLoadFloat4x4(&m_ScaleMat), XMMatrixScaling(x, y, z)));
	UpdateAABBScale();
}

void GameObject::SetTranslation(float x, float y, float z) {
	XMStoreFloat4x4(&m_TranslationMat, XMMatrixTranslation(x, y, z));
	UpdateAABBTranslation();
}

void GameObject::SetRotation(float x, float y, float z) {
	XMStoreFloat4x4(&m_RotationMat, XMMatrixRotationRollPitchYaw(x, y, z));
}

void GameObject::SetScale(float x, float y, float z) {
	XMStoreFloat4x4(&m_ScaleMat, XMMatrixScaling(x, y, z));
	UpdateAABBScale();
}

void GameObject::UpdateAABBScale() {
	XMFLOAT3 meshExtents = m_Mesh->GetExtents();
	XMFLOAT3 scaledExtents {};
	XMStoreFloat3(&scaledExtents, XMVector3Transform(XMLoadFloat3(&meshExtents), XMLoadFloat4x4(&m_ScaleMat)));
	m_AABB.Extents = scaledExtents;
}

void GameObject::UpdateAABBTranslation() {
	XMFLOAT3 translatedAABBPosition {};
	XMStoreFloat3(&translatedAABBPosition, XMVector3Transform(XMVectorZero(), XMLoadFloat4x4(&m_TranslationMat)));
	m_AABB.Center = translatedAABBPosition;
}
