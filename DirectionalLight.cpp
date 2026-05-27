#include "DirectionalLight.h"

#include "DX12EngineCore/ShaderResourceView.h"
#include "DX12EngineCore/RootSignature.h"
#include "DX12EngineCore/Texture.h"
#include "DX12EngineCore/RenderTarget.h"
#include "DX12EngineCore/CommandList.h"
#include "DX12EngineCore/Device.h"
#include "DX12EngineCore/Mesh.h"

#include "PBRObjectPSO.h"
#include "EditorGui.h"
#include "AssetImporter.h"
#include "Helpers.h"

#include <wrl/client.h>
#include <cmath>

using namespace DirectX;
using namespace Microsoft::WRL;

DirectionalLight::DirectionalLight(Device& device, DirectionalLightParams params)
    : m_Device(device)
    , m_ObjectRootSignature(params.objectRootSignature)
    , m_ShadowBias(params.shadowBias)
    , m_Color(XMFLOAT4(params.color.x, params.color.y, params.color.z, 1.0f))
    , m_ViewPort(D3D12_VIEWPORT(0.0f, 0.0f, (float)params.shadowMapResolution, (float)params.shadowMapResolution, 0.0f, 1.0f))
    , m_LightDistance(params.shadowNearFarZ.y * 0.7f) // random heuristic
    , m_ShadowRenderDistance(params.shadowRenderDistance)
    , m_ShadowNearFarZ(params.shadowNearFarZ)
{
    m_DirectionalShadowMapRT = std::make_unique<RenderTarget>();

    XMStoreFloat4x4(&m_LightOrthoMatrix, XMMatrixOrthographicLH(m_ShadowRenderDistance, m_ShadowRenderDistance, m_ShadowNearFarZ.x, m_ShadowNearFarZ.y));
    SetEulerAngles(params.eulerDegreeDir.x, params.eulerDegreeDir.y, params.eulerDegreeDir.z);

    // Create directional light shadow map
    auto shadowMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT, params.shadowMapResolution, params.shadowMapResolution, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );

    D3D12_CLEAR_VALUE depthClearValue;
    depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthClearValue.DepthStencil = { 1.0f, 0 };

    auto shadowMapDepthTexture = std::make_shared<Texture>(m_Device, shadowMapDesc, &depthClearValue);
    shadowMapDepthTexture->SetName(L"Directional Light Shadow Map");
    m_DirectionalShadowMapRT->AttachTexture(AttachmentPoint::DepthStencil, shadowMapDepthTexture);

    // Initialize ImGui SRV for debug
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        shadowMapDepthTexture->CreateShaderResourceView(srvDesc);

        EditorGui::Get().RegisterImageSRV(device, shadowMapDepthTexture, &srvDesc, EditorGui::ImGuiDebugSRVIndex::DirectionalShadowMap);
    }

    struct ShadowDepthPipelineStateStream {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
        CD3DX12_PIPELINE_STATE_STREAM_HS                    HS;
        CD3DX12_PIPELINE_STATE_STREAM_DS                    DS;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DSVFormat;
    } shadowDepthPipelineStateStream;

    CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
    rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;

    shadowDepthPipelineStateStream.pRootSignature = m_ObjectRootSignature->GetD3D12RootSignature().Get();
    shadowDepthPipelineStateStream.InputLayout = params.depthRenderInputLayout;
    shadowDepthPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    shadowDepthPipelineStateStream.VS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_VS.cso");
    shadowDepthPipelineStateStream.HS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_HS.cso");
    shadowDepthPipelineStateStream.DS = AssetImporter::Get().GetCompiledShaderFromFile(L"PBR_DS.cso");
    shadowDepthPipelineStateStream.Rasterizer = rasterizerDesc;
    shadowDepthPipelineStateStream.DSVFormat = m_DirectionalShadowMapRT->GetDepthStencilFormat();

    m_Device.CreatePipelineState(shadowDepthPipelineStateStream, m_DepthRenderPSO);
}

void DirectionalLight::SetEulerAngles(float rotX, float rotY, float rotZ) {
    if(rotX < 0) rotX += 360.0f;
    if(rotY < 0) rotY += 360.0f;
    if(rotZ < 0) rotZ += 360.0f;

    m_EulerAngles.x = std::fmod(rotX, 360.0f);
    m_EulerAngles.y = std::fmod(rotY, 360.0f);
    m_EulerAngles.z = std::fmod(rotZ, 360.0f);

    SetQuaternionAngle(XMQuaternionRotationRollPitchYaw(
        XMConvertToRadians(m_EulerAngles.x),
        XMConvertToRadians(m_EulerAngles.y),
        XMConvertToRadians(m_EulerAngles.z)
    ));
}

void DirectionalLight::SetQuaternionAngle(XMVECTOR rotationQuaternion) {
    XMVECTOR dirVec = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // starting direction chosen arbitrarily
    dirVec = XMVector3Rotate(dirVec, rotationQuaternion);

    XMStoreFloat4(&m_NormDirectionVector, XMVector3Normalize(dirVec));

    m_Position.x = XMVectorGetX(dirVec) * -m_LightDistance;
    m_Position.y = XMVectorGetY(dirVec) * -m_LightDistance;
    m_Position.z = XMVectorGetZ(dirVec) * -m_LightDistance;

    // Regenerate view matrix
    static XMFLOAT3 up { 0.0f, 1.0f, 0.0f };
    // Always look at origin, direction determined by m_Position
    static XMFLOAT3 lookAt { 0.0f, 0.0f, 0.0f };
    XMStoreFloat4x4(&m_LightViewMatrix, XMMatrixLookAtLH(XMLoadFloat3(&m_Position), XMLoadFloat3(&lookAt), XMLoadFloat3(&up)));
}

void DirectionalLight::SetShadowNearFarZ(XMFLOAT2 nearFarZ) {
    m_ShadowNearFarZ = nearFarZ;
    XMStoreFloat4x4(&m_LightOrthoMatrix, XMMatrixOrthographicLH(m_ShadowRenderDistance, m_ShadowRenderDistance, m_ShadowNearFarZ.x, m_ShadowNearFarZ.y));
}

void DirectionalLight::SetShadowRenderDistance(float distance) {
    m_ShadowRenderDistance = distance;
    XMStoreFloat4x4(&m_LightOrthoMatrix, XMMatrixOrthographicLH(m_ShadowRenderDistance, m_ShadowRenderDistance, m_ShadowNearFarZ.x, m_ShadowNearFarZ.y));
}

void DirectionalLight::SetShadowDepthPipelineStateAndRenderTarget(CommandList& directCommandList) const {
    directCommandList.ClearDepthStencilTexture(m_DirectionalShadowMapRT->GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
    directCommandList.SetViewport(m_ViewPort);
    directCommandList.SetRenderTarget(*m_DirectionalShadowMapRT);

    directCommandList.SetPipelineState(m_DepthRenderPSO);
    directCommandList.SetGraphicsRootSignature(m_ObjectRootSignature);
    directCommandList.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
}

void DirectionalLight::RenderObjectToDepth(CommandList& directCommandList, Mesh& mesh, PBRVertexProps vertexProps, const PBRTessellationProps& tessProps) const {
    // Use directional light view/proj matrix and all other copied values from vertexProps
    XMStoreFloat4x4(&vertexProps.MVP, XMLoadFloat4x4(&vertexProps.SRT) * XMLoadFloat4x4(&m_LightViewMatrix) * XMLoadFloat4x4(&m_LightOrthoMatrix));

    directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::PBRRootParameters::VertexCB, vertexProps);
    directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::PBRRootParameters::TessellationCB, tessProps);
    mesh.Draw(directCommandList);
}

std::shared_ptr<Texture> DirectionalLight::GetShadowMapTexture() const { return m_DirectionalShadowMapRT->GetTexture(AttachmentPoint::DepthStencil); }













