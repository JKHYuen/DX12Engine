#include "GameObject.h"
#include "CommandList.h"
#include "PBRObjectPSO.h"
#include "Skybox.h"
#include "DirectionalLight.h"
#include "ShaderResourceView.h"
#include "DirectXMath.h"
#include "Mesh.h"

using namespace DirectX;

void GameObject::LoadResources(CommandList& copyCommandList, const Skybox& skybox, const DirectionalLight& directionalLight, const std::wstring& pbrMatName, const std::wstring& meshName) {
	textureResources.reserve(PBRObjectPSO::PBRTextures::NumPBRTextures);
	textureResources.push_back(copyCommandList.LoadTextureFromFile(L"assets/materials/" + pbrMatName + L"_albedo.tga", true));
	textureResources.push_back(copyCommandList.LoadTextureFromFile(L"assets/materials/" + pbrMatName + L"_normal.tga", false));
	textureResources.push_back(copyCommandList.LoadTextureFromFile(L"assets/materials/" + pbrMatName + L"_mat.tga",    false));
	
	//textureResources.emplace_back(skybox.GetIrradianceSRV()->GetResource());
	//textureResources.emplace_back(skybox.GetPrefilterSRV()->GetResource());
	//textureResources.emplace_back(skybox.Get_BRDF_LUT_SRV()->GetResource());
	//textureResources.emplace_back(directionalLight.GetShadowMapSRV()->GetResource());
}

void GameObject::Render() {
	//XMMATRIX SRTMat = XMLoadFloat4x4(&scaleMat) * XMLoadFloat4x4(&rotationMat) * XMLoadFloat4x4(&translationMat);
	//VertexProps vertexCB;
	//XMStoreFloat4x4(&vertexCB.SRT, SRTMat);
	//XMStoreFloat4x4(&vertexCB.MVP, SRTMat * m_Camera.get_ViewMatrix() * m_Camera.get_ProjectionMatrix());
	//XMStoreFloat4x4(&vertexCB.directionalLightMVP, SRTMat * directionalLightViewMat * directionalLightOrthoMat);
	//XMStoreFloat4(&vertexCB.CameraPosition, m_Camera.get_Translation());

	//directCommandList->SetGraphicsDynamicConstantBuffer(PBRRootParameters::VertexCB, vertexCB);

	//// Pixel Shader Buffers
	//// TODO: lighting vars
	//MaterialProps materialCB;
	//XMVECTORF32 timeVec = { (float)e.Time, (float)e.DeltaTime, 0.0f, 0.0f };
	//XMStoreFloat4(&materialCB.Time, timeVec);

	//materialCB.DirLight = s_DirectionalLight->GetDirection();
	//materialCB.DirLightColor = s_DirectionalLight->GetColor();

	//directCommandList->SetGraphicsDynamicConstantBuffer(PBRRootParameters::MaterialCB, materialCB);
	//directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 0, s_Test_Albedo, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 1, s_Test_Normal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 2, s_Test_Material, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 3, m_Skybox->GetIrradianceSRV(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 4, m_Skybox->GetPrefilterSRV(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	//directCommandList->SetShaderResourceView(PBRRootParameters::Textures, 5, m_Skybox->Get_BRDF_LUT_SRV(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//directCommandList->SetShaderResourceView(PBRRootParameters::VolatileTextures, 0, s_DirectionalLight->GetShadowMapSRV(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	//m_Mesh->Draw(*directCommandList);
}
