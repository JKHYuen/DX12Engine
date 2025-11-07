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

using namespace DirectX;

GameObject::GameObject(CommandList& copyCommandList, GameObjectParams params, std::shared_ptr<Mesh> mesh) 
	: m_PSO(params.PSO) {

	XMStoreFloat4x4(&m_TranslationMat, params.translationMat);
	XMStoreFloat4x4(&m_RotationMat, params.rotationMat);
	XMStoreFloat4x4(&m_ScaleMat, params.scaleMat);

	// Load Resources, note that commandlist is not executed here
	m_TextureResources.resize(PBRObjectPSO::sk_NumTextures);

	std::wstring matPathPrefix { L"assets/materials/" + params.pbrMatName + L"/" + params.pbrMatName };
	m_TextureResources[PBRObjectPSO::AlbedoTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_albedo.tga", true);
	m_TextureResources[PBRObjectPSO::NormalTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_normal.tga", false);
	m_TextureResources[PBRObjectPSO::MaterialTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_mat.tga", false);

	/// TODO: check if skybox is initialized
	m_TextureResources[PBRObjectPSO::IrradianceCubemap] = params.scene.GetSkybox().GetIrradianceTexture();
	m_TextureResources[PBRObjectPSO::PrefilterCubemap] = params.scene.GetSkybox().GetPrefilterTexture();
	m_TextureResources[PBRObjectPSO::BRDFLut] = params.scene.GetSkybox().Get_BRDF_LUT_Texture();
	m_TextureResources[PBRObjectPSO::DirectionalShadowMap] = params.scene.GetDirectionalLight().GetShadowMapTexture();

	m_Mesh = mesh;
}

GameObject::GameObject(CommandList& copyCommandList, GameObjectParams params, const std::wstring& meshFileName) {
	/// TODO: load from file with params.meshFileName
}

void GameObject::Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene) {
	// Note: we are setting PSO/Root sig for every game object render, this could be slow, not an issue for current basic implementation
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
	XMMATRIX SRTMat = XMLoadFloat4x4(&m_ScaleMat) * XMLoadFloat4x4(&m_RotationMat) * XMLoadFloat4x4(&m_TranslationMat);
	scene.GetDirectionalLight().RenderObjectToDepth(directCommandList, *m_Mesh, SRTMat);
}
