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

GameObject::GameObject(CommandList& copyCommandList, const EntityParams& params, const RenderProps& renderProps, std::shared_ptr<Mesh> mesh)
	: m_Mesh(mesh)
	, m_Name(params.name)
	, m_RenderProps(renderProps)
{
	SetTranslation(params.translation.x, params.translation.y, params.translation.z);
	SetEulerRotation(params.eulerRotation.x, params.eulerRotation.y, params.eulerRotation.z);
	SetScale(params.scale.x, params.scale.y, params.scale.z);

	UpdatePBRShaderResources(copyCommandList, m_RenderProps.pbrMatName);

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
GameObject::GameObject(CommandList& copyCommandList, const EntityParams& params, const RenderProps& renderProps, const std::wstring& meshFilePath) {

}

void GameObject::Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene) {
	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
	m_RenderProps.pbrPSO->SetPipelineState(directCommandList);

	XMFLOAT4X4 v = scene.GetDirLight().GetViewMatrix();
	XMFLOAT4X4 p = scene.GetDirLight().GetOrthoMatrix();
	XMMATRIX directionalLightViewMat = XMLoadFloat4x4(&v);
	XMMATRIX directionalLightOrthoMat = XMLoadFloat4x4(&p);

	// Vertex Props
	{
		XMStoreFloat4x4(&m_PBRVertexCB.SRT, XMLoadFloat4x4(&m_ScaleMat) * XMLoadFloat4x4(&m_RotationMat) * XMLoadFloat4x4(&m_TranslationMat));
		XMStoreFloat4x4(&m_PBRVertexCB.MVP, XMLoadFloat4x4(&m_PBRVertexCB.SRT) * scene.m_MainCamera.Get_ViewMatrix() * scene.m_MainCamera.Get_ProjectionMatrix());

		XMStoreFloat4x4(&m_PBRVertexCB.directionalLightMVP, XMLoadFloat4x4(&m_PBRVertexCB.SRT) * directionalLightViewMat * directionalLightOrthoMat);
		XMStoreFloat4(&m_PBRVertexCB.cameraPosition, scene.m_MainCamera.Get_Translation());

		m_PBRVertexCB.uvScale            = m_RenderProps.uvScale;
		m_PBRVertexCB.heightMapMagnitude = m_RenderProps.heightMapMagnitude;
	}

	// Tessellation Props
	{
		XMStoreFloat4(&m_TessellationProps.cameraPosition, scene.m_MainCamera.Get_Translation());
		m_TessellationProps.SRT = m_PBRVertexCB.SRT;
		//m_TessellationProps.cullingPlanes[4] = ;
		//m_TessellationProps.cullBias = ;
		m_TessellationProps.screenDimensions = { (float)scene.GetGameWindowWidth() , (float)scene.GetGameWindowHeight() };
		m_TessellationProps.tessellationMagnitude = m_RenderProps.tessellationMagnitude;
	}

	// Light Props
	{
		m_PBRLightCB.Time = { (float)e.Time, (float)e.DeltaTime, 0.0f, 0.0f };
		m_PBRLightCB.dirLight = scene.GetDirLight().GetNormDirectionVector();
		m_PBRLightCB.dirLightColor = scene.GetDirLight().GetColor();
	}

	PBRObjectPSO::MaterialProps materialProps {};
	{
		materialProps.useParallaxShadow     = m_RenderProps.useParallaxShadow ? 1.0f : 0.0f;
		materialProps.minParallaxLayers     = (float)m_RenderProps.minParallaxLayers;
		materialProps.maxParallaxLayers     = (float)m_RenderProps.maxParallaxLayers;
		materialProps.directionalShadowBias = scene.GetDirLight().GetShadowBias();
		materialProps.parallaxMagnitude     = m_RenderProps.parallaxMagnitude;
	}

	m_RenderProps.pbrPSO->UpdateResources(directCommandList, m_TextureResources, m_PBRVertexCB, m_TessellationProps, materialProps, m_PBRLightCB);

	m_Mesh->Draw(directCommandList);
}

void GameObject::RenderSilhouette(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene, OutlinePSO* outlinePSO) const {
	// Set all but MaterialTex to null SRVs (we need MaterialTex for height map)
	for(int i = 0; i < PBRObjectPSO::TextureIndex::NumTextures; i++) {
		if(i != PBRObjectPSO::TextureIndex::MaterialTex) {
			directCommandList.SetNullShaderResourceView(PBRObjectPSO::PBRRootParameters::Textures, i);
		}
		else {
			directCommandList.SetShaderResourceView(PBRObjectPSO::PBRRootParameters::Textures, i, m_TextureResources[i], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		}
	}

	outlinePSO->UpdateResources(directCommandList, m_PBRVertexCB, m_TessellationProps);
	m_Mesh->Draw(directCommandList);
}

// we can use a dirty flag to only update SRT when neccesary
// assume we are in right rendering pipeline (see DirectionalLight::SetShadowDepthPipelineStateAndRenderTarget)
void GameObject::RenderToDirectionalShadowMap(CommandList& directCommandList, const DirectionalLight& directionalLight) {
	if(!m_RenderProps.isShadowCaster) return;

	// Set all but MaterialTex to null SRVs (we need MaterialTex for height map)
	for(int i = 0; i < PBRObjectPSO::TextureIndex::NumTextures; i++) {
		if(i != PBRObjectPSO::TextureIndex::MaterialTex) {
			directCommandList.SetNullShaderResourceView(PBRObjectPSO::PBRRootParameters::Textures, i);
		}
		else {
			directCommandList.SetShaderResourceView(PBRObjectPSO::PBRRootParameters::Textures, i, m_TextureResources[i], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		}
	}

	directionalLight.RenderObjectToDepth(directCommandList, *m_Mesh, m_PBRVertexCB, m_TessellationProps);
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

