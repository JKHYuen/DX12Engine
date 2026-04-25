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

GameObject::GameObject(CommandList& copyCommandList, const EntityParams& params, RenderProps renderProps, std::shared_ptr<Mesh> mesh)
	: m_Mesh(mesh)
	, m_Name(params.name)
	, m_RenderProps(renderProps)
{
	SetTranslation(params.translation.x, params.translation.y, params.translation.z);
	SetEulerRotation(params.eulerRotation.x, params.eulerRotation.y, params.eulerRotation.z);
	SetScale(params.scale.x, params.scale.y, params.scale.z);

	UpdatePBRShaderResources(copyCommandList, renderProps.pbrMatName);

	// Set rest of textures not updated in UpdateShaderResources()
	m_TextureResources[PBRObjectPSO::IrradianceCubemap]    = params.scene.GetSkybox().GetIrradianceTexture();
	m_TextureResources[PBRObjectPSO::PrefilterCubemap]     = params.scene.GetSkybox().GetPrefilterTexture();
	m_TextureResources[PBRObjectPSO::BRDFLut]              = params.scene.GetSkybox().Get_BRDF_LUT_Texture();
	m_TextureResources[PBRObjectPSO::DirectionalShadowMap] = params.scene.GetDirLight().GetShadowMapTexture();
}

/// TODO: somehow make this compatible with assimp loading
///        Rename this function to indicate it's loading textures from file
void GameObject::UpdatePBRShaderResources(CommandList& copyCommandList, const std::wstring& pbrMatName) {
	m_RenderProps.pbrMatName = pbrMatName;

	/// TODO: set root asset folder somewhere, maybe in IGame
	std::wstring matPathPrefix { L"assets/materials/" + pbrMatName + L"/" + pbrMatName };

	// Load Resources, note that commandlist is not executed here
	m_TextureResources[PBRObjectPSO::AlbedoTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_albedo.tga", true);
	m_TextureResources[PBRObjectPSO::NormalTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_normal.tga", false);
	m_TextureResources[PBRObjectPSO::MaterialTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_mat.tga", false);
}

void GameObject::UpdateIBLShaderResources(const Scene& scene) {
	m_TextureResources[PBRObjectPSO::IrradianceCubemap] = scene.GetSkybox().GetIrradianceTexture();
	m_TextureResources[PBRObjectPSO::PrefilterCubemap]  = scene.GetSkybox().GetPrefilterTexture();

	/// TODO: this should be cached, it never changes
	m_TextureResources[PBRObjectPSO::BRDFLut]           = scene.GetSkybox().Get_BRDF_LUT_Texture();
}

/// TODO: load from file with meshFileName
GameObject::GameObject(CommandList& copyCommandList, const EntityParams& params, RenderProps renderProps, const std::wstring& meshFilePath) {

}

void GameObject::Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene) {
	// Render object with PBR shading, use stencil write version of PSO for outline effect to work
	if(b_Outline) {
		m_RenderProps.pbrPSO->SetStencilWritePipelineState(directCommandList);
		directCommandList.GetD3D12CommandList()->OMSetStencilRef(1);
	}
	else {
		m_RenderProps.pbrPSO->SetPipelineState(directCommandList);
	}

	XMFLOAT4X4 v = scene.GetDirLight().GetViewMatrix();
	XMFLOAT4X4 o = scene.GetDirLight().GetOrthoMatrix();
	XMMATRIX directionalLightViewMat = XMLoadFloat4x4(&v);
	XMMATRIX directionalLightOrthoMat = XMLoadFloat4x4(&o);

	PBRObjectPSO::VertexProps pbrVertexCB {};
	{
		XMStoreFloat4x4(&pbrVertexCB.SRT, XMLoadFloat4x4(&m_ScaleMat) * XMLoadFloat4x4(&m_RotationMat) * XMLoadFloat4x4(&m_TranslationMat));
		XMStoreFloat4x4(&pbrVertexCB.MVP, XMLoadFloat4x4(&pbrVertexCB.SRT) * scene.m_MainCamera.Get_ViewMatrix() * scene.m_MainCamera.Get_ProjectionMatrix());

		XMStoreFloat4x4(&pbrVertexCB.directionalLightMVP, XMLoadFloat4x4(&pbrVertexCB.SRT) * directionalLightViewMat * directionalLightOrthoMat);
		XMStoreFloat4(&pbrVertexCB.cameraPosition, scene.m_MainCamera.Get_Translation());

		pbrVertexCB.uvScale            = m_RenderProps.uvScale;
		pbrVertexCB.heightMapMagnitude = m_RenderProps.heightMapMagnitude;
	}

	PBRObjectPSO::LightProps lightProps {};
	{
		lightProps.Time = { (float)e.Time, (float)e.DeltaTime, 0.0f, 0.0f };
		lightProps.dirLight = scene.GetDirLight().GetDirection();
		lightProps.dirLightColor = scene.GetDirLight().GetColor();
	}

	PBRObjectPSO::MaterialProps materialProps {};
	{
		materialProps.useParallaxShadow = m_RenderProps.useParallaxShadow ? 1.0f : 0.0f;
		materialProps.minParallaxLayers = (float)m_RenderProps.minParallaxLayers;
		materialProps.maxParallaxLayers = (float)m_RenderProps.maxParallaxLayers;
		materialProps.directionalShadowBias = scene.GetDirLight().GetShadowBias(); 
		materialProps.parallaxMagnitude = m_RenderProps.parallaxMagnitude;
	}

	m_RenderProps.pbrPSO->UpdateResources(directCommandList, m_TextureResources, pbrVertexCB, materialProps, lightProps);

	m_Mesh->Draw(directCommandList);
}

void GameObject::RenderOutline(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene) {
	if(b_Outline) {
		m_RenderProps.outlinePSO->SetPipelineState(directCommandList);
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
		m_RenderProps.outlinePSO->UpdateResources(directCommandList, outlineVertexCB, outlineMaterialCB);
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

