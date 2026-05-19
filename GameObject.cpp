#include "GameObject.h"

#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif
#include "CommandList.h"
#include "PBRObjectPSO.h"
#include "UnlitPSO.h"
#include "UnlitPrimitivePSO.h"
#include "Skybox.h"
#include "DirectionalLight.h"
#include "ShaderResourceView.h"
#include "Camera.h"
#include "Scene.h"
#include "Events.h"
#include "Mesh.h"

#include "Logger.h"

#include <DirectXMath.h>
#include <format>
#include <algorithm>
#include <limits>

using namespace DirectX;

GameObject::GameObject(CommandList& copyCommandList, const EntityParams& params, const RenderProps& renderProps, std::shared_ptr<Mesh> mesh)
	: m_Mesh(mesh)
	, m_Name(params.name)
	, m_RenderProps(renderProps)
{
	// Don't use setters (e.g. SetTranslation()), this ensures AABB is initialized properly
	m_Scale = params.scale;
	XMStoreFloat4x4(&m_ScaleMat, XMMatrixScaling(params.scale.x, params.scale.y, params.scale.z));
	m_EulerRotation = params.eulerRotation;
	XMStoreFloat4x4(&m_RotationMat, XMMatrixRotationRollPitchYaw(params.eulerRotation.x, params.eulerRotation.y, params.eulerRotation.z));
	m_Translation = params.translation;
	XMStoreFloat4x4(&m_TranslationMat, XMMatrixTranslation(params.translation.x, params.translation.y, params.translation.z));

	// Initialize AABB
	{
		XMStoreFloat4x4(&m_AABBOffsettedSRMatrix,
			XMMatrixTranslation(-m_Mesh->GetCenter().x, -m_Mesh->GetCenter().y, -m_Mesh->GetCenter().z) *
			XMLoadFloat4x4(&m_ScaleMat) * XMLoadFloat4x4(&m_RotationMat)
		);
		XMStoreFloat3(&m_AABBOffset, XMVector3Transform(XMVECTORF32 { m_Mesh->GetCenter().x, m_Mesh->GetCenter().y, m_Mesh->GetCenter().z }, XMLoadFloat4x4(&m_RotationMat)));

		m_AABB.Center = m_Translation;
		RecalcAABBExtents();
	}

	UpdatePBRShaderResourcesFromFile(copyCommandList, m_RenderProps.pbrMatName);

	// Set rest of textures not updated in UpdateShaderResources()
	m_TextureResources[PBRObjectPSO::IrradianceCubemap]    = params.scene.GetSkybox().GetIrradianceTexture();
	m_TextureResources[PBRObjectPSO::PrefilterCubemap]     = params.scene.GetSkybox().GetPrefilterTexture();
	m_TextureResources[PBRObjectPSO::BRDFLut]              = params.scene.GetSkybox().Get_BRDF_LUT_Texture();
	m_TextureResources[PBRObjectPSO::DirectionalShadowMap] = params.scene.GetDirLight().GetShadowMapTexture();
}

/// TODO: somehow make this compatible with assimp loading
void GameObject::UpdatePBRShaderResourcesFromFile(CommandList& copyCommandList, const std::wstring& pbrMatName) {
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

void GameObject::Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene, bool b_WireframeRender) {
	/// TODO: add bias value (derived from heightMapMagnitude)
	if(!scene.m_MainCamera.CheckAABBInFrustum(m_AABB, 0.0f)) {
		return;
	}

	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
	
	if(b_WireframeRender)
		m_RenderProps.pbrPSO->SetWireframePipelineState(directCommandList);
	else
		m_RenderProps.pbrPSO->SetPipelineState(directCommandList);

	// Vertex Props
	// using XMMatrixMultiply instead of operator* as recommended: 
	// https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-optimizing#avoid-operator-overloads-when-possible
	{
		// Cache vars for AABB extents calc
		{
			XMStoreFloat3(&m_AABBOffset, XMVector3Transform(XMVECTORF32 { m_Mesh->GetCenter().x, m_Mesh->GetCenter().y, m_Mesh->GetCenter().z }, XMLoadFloat4x4(&m_RotationMat)));

			XMStoreFloat4x4(&m_AABBOffsettedSRMatrix,
				XMMatrixMultiply(
					XMMatrixTranslation(-m_Mesh->GetCenter().x, -m_Mesh->GetCenter().y, -m_Mesh->GetCenter().z),
					XMMatrixMultiply(XMLoadFloat4x4(&m_ScaleMat), XMLoadFloat4x4(&m_RotationMat))
				)
			);
		}

		XMStoreFloat4x4(&m_PBRVertexCB.SRT, XMMatrixMultiply(XMLoadFloat4x4(&m_AABBOffsettedSRMatrix), XMLoadFloat4x4(&m_TranslationMat)));
		XMStoreFloat4x4(
			&m_PBRVertexCB.MVP, 
			XMMatrixMultiply(
				XMMatrixMultiply(XMLoadFloat4x4(&m_PBRVertexCB.SRT), scene.m_MainCamera.Get_ViewMatrix()),
				scene.m_MainCamera.Get_ProjectionMatrix()
			)
		);

		XMFLOAT4X4 v = scene.GetDirLight().GetViewMatrix();
		XMFLOAT4X4 p = scene.GetDirLight().GetOrthoMatrix();
		XMMATRIX directionalLightViewMat = XMLoadFloat4x4(&v);
		XMMATRIX directionalLightOrthoMat = XMLoadFloat4x4(&p);
		XMStoreFloat4x4(&m_PBRVertexCB.directionalLightMVP,
			XMMatrixMultiply(
				XMMatrixMultiply(XMLoadFloat4x4(&m_PBRVertexCB.SRT), directionalLightViewMat),
				directionalLightOrthoMat
			)
		);

		XMStoreFloat4(&m_PBRVertexCB.cameraPosition, scene.m_MainCamera.Get_Translation());

		m_PBRVertexCB.uvScale            = m_RenderProps.uvScale;
		m_PBRVertexCB.heightMapMagnitude = m_RenderProps.heightMapMagnitude;
		m_PBRVertexCB.color = XMFLOAT4(1.0, 1.0, 1.0, 1.0); // unused
	}

	// Tessellation Props
	{
		XMStoreFloat4(&m_TessellationCB.cameraPosition, scene.m_MainCamera.Get_Translation());
		m_TessellationCB.SRT = m_PBRVertexCB.SRT;
		/// TODO: only if we want per triangle culling
		//m_TessellationProps.cullingPlanes[4] = ;
		//m_TessellationProps.cullBias = ;
		m_TessellationCB.screenDimensions = { (float)scene.GetGameWindowWidth() , (float)scene.GetGameWindowHeight() };
		m_TessellationCB.tessellationMagnitude = m_RenderProps.tessellationMagnitude;
	}

	// Light Props
	{
		m_PBRLightCB.Time = { (float)e.Time, (float)e.DeltaTime, 0.0f, 0.0f };
		m_PBRLightCB.dirLight = scene.GetDirLight().GetNormDirectionVector();
		m_PBRLightCB.dirLightColor = scene.GetDirLight().GetColor();
	}

	PBRMaterialProps materialProps {};
	{
		materialProps.useParallaxShadow     = m_RenderProps.useParallaxShadow ? 1.0f : 0.0f;
		materialProps.minParallaxLayers     = (float)m_RenderProps.minParallaxLayers;
		materialProps.maxParallaxLayers     = (float)m_RenderProps.maxParallaxLayers;
		materialProps.directionalShadowBias = scene.GetDirLight().GetShadowBias();
		materialProps.parallaxMagnitude     = m_RenderProps.parallaxMagnitude;
	}

	m_RenderProps.pbrPSO->UpdateResources(directCommandList, m_TextureResources, m_PBRVertexCB, m_TessellationCB, materialProps, m_PBRLightCB);

	m_Mesh->Draw(directCommandList);
}

void GameObject::RenderSilhouette(CommandList& directCommandList, const UpdateEventArgs& e, UnlitPSO* unlitPSO, XMFLOAT4 color) {
	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);

	// Set all but MaterialTex to null SRVs (we need MaterialTex for height map)
	for(int i = 0; i < PBRObjectPSO::TextureIndex::NumTextures; i++) {
		if(i != PBRObjectPSO::TextureIndex::MaterialTex) {
			directCommandList.SetNullShaderResourceView(PBRObjectPSO::PBRRootParameters::Textures, i);
		}
		else {
			directCommandList.SetShaderResourceView(PBRObjectPSO::PBRRootParameters::Textures, PBRObjectPSO::TextureIndex::MaterialTex, m_TextureResources[PBRObjectPSO::TextureIndex::MaterialTex], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		}
	}

	m_PBRVertexCB.color = color;
	unlitPSO->UpdateResources(directCommandList, m_PBRVertexCB, m_TessellationCB);

	m_Mesh->Draw(directCommandList);
}

void GameObject::RenderBoundingBox(CommandList& directCommandList, const UpdateEventArgs& e, UnlitPrimitivePSO* unlitPrimitivePSO, const Scene& scene, XMFLOAT4 color) {
	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set all textures to null SRVs
	for(int i = 0; i < PBRObjectPSO::TextureIndex::NumTextures; i++) {
		directCommandList.SetNullShaderResourceView(PBRObjectPSO::PBRRootParameters::Textures, i);
	}

	XMMATRIX SRT = XMMatrixScaling(2.0f * m_AABB.Extents.x, 2.0f * m_AABB.Extents.y, 2.0f * m_AABB.Extents.z) * XMMatrixIdentity() * XMMatrixTranslation(m_AABB.Center.x, m_AABB.Center.y, m_AABB.Center.z);
	XMStoreFloat4x4(&m_PBRVertexCB.MVP, SRT * scene.m_MainCamera.Get_ViewMatrix() * scene.m_MainCamera.Get_ProjectionMatrix());

	m_PBRVertexCB.color = color;

	unlitPrimitivePSO->UpdateResources(directCommandList, m_PBRVertexCB);

	directCommandList.GetCubePrimitive()->Draw(directCommandList);
}

// assume we are in right rendering pipeline (see DirectionalLight::SetShadowDepthPipelineStateAndRenderTarget)
void GameObject::RenderToDirectionalShadowMap(CommandList& directCommandList, const DirectionalLight& directionalLight) {
	if(!m_RenderProps.isShadowCaster) return;

	directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);

	// Set all but MaterialTex to null SRVs (we need MaterialTex for height map)
	for(int i = 0; i < PBRObjectPSO::TextureIndex::NumTextures; i++) {
		if(i != PBRObjectPSO::TextureIndex::MaterialTex) {
			directCommandList.SetNullShaderResourceView(PBRObjectPSO::PBRRootParameters::Textures, i);
		}
		else {
			directCommandList.SetShaderResourceView(PBRObjectPSO::PBRRootParameters::Textures, i, m_TextureResources[i], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		}
	}

	directionalLight.RenderObjectToDepth(directCommandList, *m_Mesh, m_PBRVertexCB, m_TessellationCB);
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

	RecalcAABBExtents();
}

void GameObject::SetScale(float x, float y, float z) {
	m_Scale = {x, y, z};
	XMStoreFloat4x4(&m_ScaleMat, XMMatrixScaling(x, y, z));

	RecalcAABBExtents();
}

// Scale and rotate all 8 vertices (for non-uniform scaling support) with object transformation matrices
// then calculate new extents based on this new transformed AABB.
// Kind of slow, called when scaling or rotating object. Can be simplified if there is uniform scaling.
void GameObject::RecalcAABBExtents() {
	// Start with original mesh extents at world origin
	XMFLOAT3 e = m_Mesh->GetExtents();
	std::vector<XMFLOAT3> aabbVerts = {
		{+ e.x, + e.y, + e.z}, {- e.x, - e.y, - e.z}, 
		{+ e.x, + e.y, - e.z}, {- e.x, - e.y, + e.z},
		{+ e.x, - e.y, + e.z}, {- e.x, + e.y, - e.z},
		{+ e.x, - e.y, - e.z}, {- e.x, + e.y, + e.z},
	};

	// Note: this might be simplified if we are more clever about coord spaces
	float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
	for(XMFLOAT3 v : aabbVerts) {
		// Rotate AABB vertex with original mesh center as pivot by
		// translating vertex with original mesh center and then scale and rotate with model matrices
		XMStoreFloat3(&v, XMVector3Transform(XMLoadFloat3(&v), XMLoadFloat4x4(&m_AABBOffsettedSRMatrix)));

		// comparison needs to take original mesh center into account since the calculated box ends up aligned with the object in world space
		maxX = std::fmax(maxX, std::fabs(v.x + m_AABBOffset.x));
		maxY = std::fmax(maxY, std::fabs(v.y + m_AABBOffset.y));
		maxZ = std::fmax(maxZ, std::fabs(v.z + m_AABBOffset.z));
	}

	m_AABB.Extents.x = maxX;
	m_AABB.Extents.y = maxY;
	m_AABB.Extents.z = maxZ;
}

