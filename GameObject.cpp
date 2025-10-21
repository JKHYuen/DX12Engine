#include "GameObject.h"
#include "CommandList.h"
#include "PBRObjectPSO.h"
#include "Skybox.h"
#include "DirectionalLight.h"
#include "ShaderResourceView.h"
#include "Camera.h"
#include "Events.h"
#include "DirectXMath.h"
#include "Mesh.h"

using namespace DirectX;

/// TODO: move mainCamera and directionalLight to a Scene class
GameObject::GameObject(XMMATRIX translationMat, XMMATRIX rotationMat, XMMATRIX scaleMat, DirectionalLight& directionalLight, Camera& mainCamera, std::shared_ptr<PBRObjectPSO> pbrPSO)
	: m_PBR_PSO(pbrPSO)
	, m_DirectionalLight(directionalLight)
	, m_MainCamera(mainCamera) {

	XMStoreFloat4x4(&m_TranslationMat, translationMat);
	XMStoreFloat4x4(&m_RotationMat, rotationMat);
	XMStoreFloat4x4(&m_ScaleMat, scaleMat);
}

void GameObject::LoadResources(CommandList& copyCommandList, const Skybox& skybox, const std::wstring& pbrMatName, const std::wstring& meshName) {
	/// TODO: load mesh from filename
}

/// TODO: move skybox to a Scene class
void GameObject::LoadResources(CommandList& copyCommandList, const Skybox& skybox, const std::wstring& pbrMatName, std::shared_ptr<Mesh> mesh) {
	m_TextureResources.reserve(PBRObjectPSO::sk_NumTextures);
	m_TextureResources.resize(PBRObjectPSO::sk_NumTextures);

	m_TextureResources[PBRObjectPSO::AlbedoTex] = 
		copyCommandList.LoadTextureFromFile(L"assets/materials/" + pbrMatName + L"_albedo.tga", true);
	m_TextureResources[PBRObjectPSO::NormalTex] = 
		copyCommandList.LoadTextureFromFile(L"assets/materials/" + pbrMatName + L"_normal.tga", false);
	m_TextureResources[PBRObjectPSO::MaterialTex] = 
		copyCommandList.LoadTextureFromFile(L"assets/materials/" + pbrMatName + L"_mat.tga", false);
	
	m_TextureResources[PBRObjectPSO::IrradianceCubemap]    = skybox.GetIrradianceSRV();
	m_TextureResources[PBRObjectPSO::PrefilterCubemap]     = skybox.GetPrefilterSRV();
	m_TextureResources[PBRObjectPSO::BRDFLut]              = skybox.Get_BRDF_LUT_SRV();
	m_TextureResources[PBRObjectPSO::DirectionalShadowMap] = m_DirectionalLight.GetShadowMapTexture();

	m_Mesh = mesh;
}

void GameObject::Render(CommandList& directCommandList, UpdateEventArgs& e) {
	XMFLOAT4X4 v = m_DirectionalLight.GetViewMatrix();
	XMFLOAT4X4 o = m_DirectionalLight.GetOrthoMatrix();
	XMMATRIX directionalLightViewMat = XMLoadFloat4x4(&v);
	XMMATRIX directionalLightOrthoMat = XMLoadFloat4x4(&o);

	XMMATRIX SRTMat = XMLoadFloat4x4(&m_ScaleMat) * XMLoadFloat4x4(&m_RotationMat) * XMLoadFloat4x4(&m_TranslationMat);
	PBRObjectPSO::VertexProps vertexCB;
	XMStoreFloat4x4(&vertexCB.SRT, SRTMat);
	XMStoreFloat4x4(&vertexCB.MVP, SRTMat * m_MainCamera.get_ViewMatrix() * m_MainCamera.get_ProjectionMatrix());
	XMStoreFloat4x4(&vertexCB.directionalLightMVP, SRTMat * directionalLightViewMat * directionalLightOrthoMat);
	XMStoreFloat4(&vertexCB.CameraPosition, m_MainCamera.get_Translation());

	PBRObjectPSO::MaterialProps materialCB;
	XMVECTORF32 timeVec = { (float)e.Time, (float)e.DeltaTime, 0.0f, 0.0f };
	XMStoreFloat4(&materialCB.Time, timeVec);

	materialCB.DirLight = m_DirectionalLight.GetDirection();
	materialCB.DirLightColor = m_DirectionalLight.GetColor();

	m_PBR_PSO->UpdateResources(directCommandList, m_TextureResources, vertexCB, materialCB);

	m_Mesh->Draw(directCommandList);
}

void GameObject::RenderToDirectionalShadowMap(CommandList& directCommandList) {
	/// TODO: use a dirty flag to only update SRT when neccesary
	XMMATRIX SRTMat = XMLoadFloat4x4(&m_ScaleMat) * XMLoadFloat4x4(&m_RotationMat) * XMLoadFloat4x4(&m_TranslationMat);
	m_DirectionalLight.RenderObjectToDepth(directCommandList, *m_Mesh, SRTMat);
}
