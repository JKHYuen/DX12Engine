#include "PBRGameObject.h"
#include "GameObject.h"

#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/Mesh.h"

#include "AssetImporter.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "Events.h"
#include "PBRObjectPSO.h"
#include "RenderConstants.h"
#include "Scene.h"
#include "Skybox.h"
#include "UnlitPSO.h"
#include "UnlitPrimitivePSO.h"

#include "d3d12.h"
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <cstdint>
#include <Logger.h>

using namespace RenderEnums;
using namespace DirectX;

PBRGameObject::PBRGameObject(Scene& scene, CommandList& copyCommandList, const GameObject::EntityParams& params, const RenderProps& renderProps, std::shared_ptr<Mesh> mesh)
	: GameObject(params, mesh)
	, m_RenderProps(renderProps)
{
	UpdatePBRShaderResourcesFromFile(copyCommandList, m_RenderProps.pbrMatName);

	// Set rest of textures not updated in UpdateShaderResources()
	m_TextureResources[PBRObjectPSO::IrradianceCubemap] = scene.GetSkybox()->GetIrradianceTexture();
	m_TextureResources[PBRObjectPSO::PrefilterCubemap] = scene.GetSkybox()->GetPrefilterTexture();
	m_TextureResources[PBRObjectPSO::BRDFLut] = scene.GetSkybox()->Get_BRDF_LUT_Texture();
	m_TextureResources[PBRObjectPSO::DirectionalShadowMap] = scene.GetDirLight()->GetShadowMapTexture();
}

/// TODO: somehow make this compatible with assimp loading
void PBRGameObject::UpdatePBRShaderResourcesFromFile(CommandList& copyCommandList, const std::wstring& pbrMatName) {
	m_RenderProps.pbrMatName = pbrMatName;

	std::wstring matPathPrefix { AssetImporter::Get().GetAssetPath() / L"materials" / pbrMatName / pbrMatName };

	// Load Resources
	// NOTE: commandlist is not executed here
	m_TextureResources[PBRObjectPSO::AlbedoTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_albedo.tga", true);
	m_TextureResources[PBRObjectPSO::NormalTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_normal.tga", false);
	m_TextureResources[PBRObjectPSO::MaterialTex] =
		copyCommandList.LoadTextureFromFile(matPathPrefix + L"_mat.tga", false);
}

void PBRGameObject::UpdateIBLShaderResources(const Scene& scene) {
	m_TextureResources[PBRObjectPSO::IrradianceCubemap] = scene.GetSkybox()->GetIrradianceTexture();
	m_TextureResources[PBRObjectPSO::PrefilterCubemap] = scene.GetSkybox()->GetPrefilterTexture();

	/// TODO: this should be cached, it never changes
	m_TextureResources[PBRObjectPSO::BRDFLut] = scene.GetSkybox()->Get_BRDF_LUT_Texture();
}

/// TODO: load from file with meshFileName
//PBRGameObject::GameObject(CommandList& copyCommandList, const EntityParams& params, const RenderProps& renderProps, const std::wstring& meshFilePath) {
//
//}

void PBRGameObject::Render(CommandList& directCommandList, const UpdateEventArgs& e, const Scene& scene, bool b_WireframeRender) {
	// Frustum check is only done here, other render functions rely on b_RenderThisFrame.
	// This means other render functions may respond a frame late (depending on call order) but we don't need to do frustum check multiple times a frame
	/// TODO: add bias value (derived from heightMapMagnitude)
	if(!scene.GetMainCamera().CheckAABBInFrustum(m_AABB, 0.0f)) {
		mb_RenderThisFrame = false;
		return;
	}

	mb_RenderThisFrame = true;

	RenderFlags flags = RenderFlags_None;
	if(b_WireframeRender) flags |= RenderFlags_Wireframe;
	flags |= m_RenderProps.tessellationModeFlag;

	m_RenderProps.pbrPSO->SetPipelineState(directCommandList, flags);

	// Vertex Props
	// using XMMatrixMultiply instead of operator* as recommended: 
	// https://learn.microsoft.com/en-us/windows/win32/dxmath/pg-xnamath-optimizing#avoid-operator-overloads-when-possible
	{
		XMStoreFloat4x4(&m_PBRVertexCB.SRT, XMMatrixMultiply(XMLoadFloat4x4(&m_SRMat), XMLoadFloat4x4(&m_TranslationMat)));

		XMStoreFloat4x4(
			&m_PBRVertexCB.MVP,
			XMMatrixMultiply(
				XMMatrixMultiply(XMLoadFloat4x4(&m_PBRVertexCB.SRT), scene.GetMainCamera().Get_ViewMatrix()),
				scene.GetMainCamera().Get_ProjectionMatrix()
			)
		);

		XMFLOAT4X4 v = scene.GetDirLight()->GetViewMatrix();
		XMFLOAT4X4 p = scene.GetDirLight()->GetOrthoMatrix();
		XMMATRIX directionalLightViewMat = XMLoadFloat4x4(&v);
		XMMATRIX directionalLightOrthoMat = XMLoadFloat4x4(&p);
		XMStoreFloat4x4(&m_PBRVertexCB.directionalLightMVP,
			XMMatrixMultiply(
				XMMatrixMultiply(XMLoadFloat4x4(&m_PBRVertexCB.SRT), directionalLightViewMat),
				directionalLightOrthoMat
			)
		);

		XMStoreFloat4(&m_PBRVertexCB.cameraPosition, scene.GetMainCamera().Get_Translation());

		m_PBRVertexCB.uvScale = m_RenderProps.uvScale;
		m_PBRVertexCB.heightMapMagnitude = m_RenderProps.heightMapMagnitude;
		m_PBRVertexCB.color = XMFLOAT4(1.0, 1.0, 1.0, 1.0); // unused
	}

	// Tessellation Props
	{
		XMStoreFloat4(&m_TessellationCB.cameraPosition, scene.GetMainCamera().Get_Translation());
		m_TessellationCB.SRT = m_PBRVertexCB.SRT;
		m_TessellationCB.screenDimensions = { (float)scene.GetWindowWidth() , (float)scene.GetWindowHeight() };

		if(((m_RenderProps.tessellationModeFlag) & RenderFlags_UniformTessellation) != 0) {
			m_TessellationCB.tessellationMagnitude = m_RenderProps.tessellationMagnitude;
		}
		else if(((m_RenderProps.tessellationModeFlag) & RenderFlags_EdgeTessellation) != 0) {
			m_TessellationCB.tessellationMagnitude = m_RenderProps.tessellationEdgeLength;
		}
		else {
			m_TessellationCB.tessellationMagnitude = 1.0f;
		}
	}

	// Light Props
	{
		m_PBRLightCB.Time = { (float)e.Time, (float)e.DeltaTime, 0.0f, 0.0f };
		m_PBRLightCB.dirLight = scene.GetDirLight()->GetNormDirectionVector();
		m_PBRLightCB.dirLightColor = scene.GetDirLight()->GetColor();

		PointLightProps pl {};
		for(uint32_t i = 0; i < RenderGlobals::gk_MaxPointLightCount; i++) {
			XMFLOAT3 plWorldPos = scene.GetPointLight(i)->GetTranslation();
			XMFLOAT3 plColor = scene.GetPointLight(i)->GetColor();
			pl.worldPosition = XMFLOAT4(plWorldPos.x, plWorldPos.y, plWorldPos.z, 1.0f);
			pl.colorInvRadius = XMFLOAT4(plColor.x, plColor.y, plColor.z, 1.0f / scene.GetPointLight(i)->GetRadius());
			m_PBRLightCB.pointLights[i] = pl;
		}
	}

	PBRMaterialProps materialProps {};
	{
		materialProps.useParallaxShadow = m_RenderProps.useParallaxShadow ? 1.0f : 0.0f;
		materialProps.minParallaxLayers = (float)m_RenderProps.minParallaxLayers;
		materialProps.maxParallaxLayers = (float)m_RenderProps.maxParallaxLayers;
		materialProps.directionalShadowBias = scene.GetDirLight()->GetShadowBias();
		materialProps.parallaxMagnitude = m_RenderProps.parallaxMagnitude;
	}

	m_RenderProps.pbrPSO->UpdateResources(directCommandList, m_TextureResources, m_PBRVertexCB, m_TessellationCB, materialProps, m_PBRLightCB);

	m_Mesh->Draw(directCommandList);
}

// Only used for outline effect
void PBRGameObject::RenderSilhouette(CommandList& directCommandList, const UpdateEventArgs& e, UnlitPSO* unlitPSO, XMFLOAT4 color) {
	// Set all but MaterialTex to null SRVs (we need MaterialTex for height map)
	for(int i = 0; i < PBRObjectPSO::TextureIndex::NumTextures; i++) {
		if(i != PBRObjectPSO::TextureIndex::MaterialTex) {
			directCommandList.SetNullShaderResourceView(PBRObjectPSO::PBRRootParameters::Textures, i);
		}
		else {
			directCommandList.SetShaderResourceView(
				PBRObjectPSO::PBRRootParameters::Textures, PBRObjectPSO::TextureIndex::MaterialTex,
				m_TextureResources[PBRObjectPSO::TextureIndex::MaterialTex], D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
			);
		}
	}

	m_PBRVertexCB.color = color;
	unlitPSO->UpdateResources(directCommandList, m_PBRVertexCB, m_TessellationCB);

	m_Mesh->Draw(directCommandList);
}

// assume we are in right rendering pipeline (see DirectionalLight::SetShadowDepthPipelineStateAndRenderTarget)
void PBRGameObject::RenderToDirectionalShadowMap(CommandList& directCommandList, const DirectionalLight& directionalLight) {
	// Need a smarter shadow cull than just tying it to object frustum culling, shadows will cull in camera
	// will revisit after CSM is implemented
	//if(!b_RenderThisFrame) return;

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

	directionalLight.RenderObjectToDepth(directCommandList, *m_Mesh, m_PBRVertexCB, m_TessellationCB);
}

void PBRGameObject::RenderBoundingBox(CommandList& directCommandList, const UpdateEventArgs& e, UnlitPrimitivePSO* unlitPrimitivePSO, const Scene& scene, XMFLOAT4 color) {
	if(!mb_RenderThisFrame) return;

	// Set all textures to null SRVs
	for(int i = 0; i < PBRObjectPSO::TextureIndex::NumTextures; i++) {
		directCommandList.SetNullShaderResourceView(PBRObjectPSO::PBRRootParameters::Textures, i);
	}

	XMMATRIX aabbSRT = XMMatrixMultiply(
		XMMatrixScaling(2.0f * m_AABB.Extents.x, 2.0f * m_AABB.Extents.y, 2.0f * m_AABB.Extents.z),
		XMMatrixTranslation(m_AABB.Center.x, m_AABB.Center.y, m_AABB.Center.z)
	);

	// Reusing m_PBRVertexCB for convenience, this means we need to make sure it gets updated every frame for different rendering pipelines
	XMStoreFloat4x4(&m_PBRVertexCB.MVP, XMMatrixMultiply(
		XMMatrixMultiply(aabbSRT, scene.GetMainCamera().Get_ViewMatrix()),
		scene.GetMainCamera().Get_ProjectionMatrix())
	);

	m_PBRVertexCB.color = color;

	unlitPrimitivePSO->UpdateResources(directCommandList, m_PBRVertexCB);

	directCommandList.GetCubePrimitive()->Draw(directCommandList);
}

