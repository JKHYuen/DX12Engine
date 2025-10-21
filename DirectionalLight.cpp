#include "DirectionalLight.h"
#include "ShaderResourceView.h"
#include "RootSignature.h"
#include "CommandList.h"
#include "Device.h"
#include "PBRObjectPSO.h"
#include "Mesh.h"
#include "Helpers.h"

#include <wrl/client.h>
#include <d3dcompiler.h>
#include <cmath>

using namespace DirectX;
using namespace Microsoft::WRL;

DirectionalLight::DirectionalLight(
    Device& device, std::shared_ptr<RootSignature> depthRenderRootSignature, CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT depthRenderInputLayout,
    XMFLOAT3 color, XMFLOAT3 eulerDir, int shadowMapResolution, float shadowDistance,
    float shadowMapNearZ, float shadowMapFarZ, float shadowBias)
    : m_Device(device)
    , m_DepthRenderRootSignature(depthRenderRootSignature)
    , m_ShadowBias(shadowBias)
    , m_Color(XMFLOAT4(color.x, color.y, color.z, 1.0f))
    , m_ViewPort(D3D12_VIEWPORT(0.0f, 0.0f, (float)shadowMapResolution, (float)shadowMapResolution, 0.0f, 1.0f)) {

    XMStoreFloat4x4(&m_OrthoMatrix, XMMatrixOrthographicLH(shadowDistance, shadowDistance, shadowMapNearZ, shadowMapFarZ));
    SetDirection(eulerDir.x, eulerDir.y, eulerDir.z);

    // Create directional light shadow map
    auto shadowMapDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT, shadowMapResolution, shadowMapResolution, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );

    D3D12_CLEAR_VALUE depthClearValue;
    depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthClearValue.DepthStencil = { 1.0f, 0 };

    auto shadowMapDepthTexture = std::make_shared<Texture>(m_Device, shadowMapDesc, &depthClearValue);
    shadowMapDepthTexture->SetName(L"Directional Light Shadow Map");
    m_DirectionalShadowMap.AttachTexture(AttachmentPoint::DepthStencil, shadowMapDepthTexture);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    //m_ShadowMapSRV = std::make_shared<ShaderResourceView>(m_Device, shadowMapDepthTexture, &srvDesc);
    shadowMapDepthTexture->CreateShaderResourceView(srvDesc);

    // Initialize ImGui SRV for debug
    // Note: SRVs for ImGui render have its own allocator and descriptor heap (instead of the two stage DynamicDescriptorHeap system) to keep things simple
    //s_GuiShadowMapDebugSRV = EditorGui::AllocateImageSRV(*m_Device, m_DirectionalShadowMap.GetTexture(AttachmentPoint::DepthStencil), &srvDesc);

    /// TODO: we are loading basic model VS again, shader blobs should probably be cached
    Microsoft::WRL::ComPtr<ID3DBlob> vs;
    ThrowIfFailed(D3DReadFileToBlob(L"compiled_shaders/PBR_VS.cso", &vs));

    struct ShadowDepthPipelineStateStream {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS                    VS;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER            Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  DSVFormat;
    } shadowDepthPipelineStateStream;

    CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
    rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;

    shadowDepthPipelineStateStream.pRootSignature = m_DepthRenderRootSignature->GetD3D12RootSignature().Get();
    shadowDepthPipelineStateStream.InputLayout = depthRenderInputLayout;
    shadowDepthPipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    shadowDepthPipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
    shadowDepthPipelineStateStream.Rasterizer = rasterizerDesc;
    shadowDepthPipelineStateStream.DSVFormat = m_DirectionalShadowMap.GetDepthStencilFormat();

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = { sizeof(ShadowDepthPipelineStateStream), &shadowDepthPipelineStateStream };
    ThrowIfFailed(m_Device.GetD3D12Device()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_DepthRenderPSO)));
}

void DirectionalLight::SetColor(float red, float green, float blue) {
    m_Color = XMFLOAT4(red, green, blue, 1.0f);
}

void DirectionalLight::SetDirection(float rotX, float rotY, float rotZ) {
    rotX = XMConvertToRadians(std::fmod(rotX, 360.0f));
    rotY = XMConvertToRadians(std::fmod(rotY, 360.0f));
    rotZ = XMConvertToRadians(std::fmod(rotZ, 360.0f));

    SetQuaternionDirection(XMQuaternionRotationRollPitchYaw(rotX, rotY, rotZ));
}

void DirectionalLight::SetQuaternionDirection(XMVECTOR rotationQuaternion) {
    XMVECTOR dirVec = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    dirVec = XMVector3Rotate(dirVec, rotationQuaternion);

    XMStoreFloat4(&m_Direction, dirVec);

    m_Position.x = XMVectorGetX(dirVec) * m_LightDistance;
    m_Position.y = XMVectorGetY(dirVec) * m_LightDistance;
    m_Position.z = XMVectorGetZ(dirVec) * m_LightDistance;

    // Regenerate view matrix
    GenerateViewMatrix();
}

void DirectionalLight::GenerateViewMatrix() {
    static XMFLOAT3 up{ 0.0f, 1.0f, 0.0f };
    // Always look at origin, direction determined by m_Position
    static XMFLOAT3 lookAt{ 0.0f, 0.0f, 0.0f };
    XMStoreFloat4x4(&m_ViewMatrix, XMMatrixLookAtLH(XMLoadFloat3(&m_Position), XMLoadFloat3(&lookAt), XMLoadFloat3(&up)));
}

// Note: a bit hacky
void DirectionalLight::GetEulerAngles(float& out_X, float& out_Y) const {
    XMVECTOR normDirVec = XMLoadFloat3(&m_Position);
    float x = -XMVectorGetX(normDirVec);
    float z = -XMVectorGetY(normDirVec);
    float y = XMVectorGetZ(normDirVec);

    float r = XMVectorGetX(XMVector3Length(normDirVec));
    float t = std::atan2(y, x);
    float p = std::acos(z / r);

    out_X = XMConvertToDegrees(p) - 90.f;
    out_Y = XMConvertToDegrees(t) + 90.f;
}

void DirectionalLight::SetShadowDepthPipelineState(CommandList& directCommandList) const {
    directCommandList.ClearDepthStencilTexture(m_DirectionalShadowMap.GetTexture(AttachmentPoint::DepthStencil), D3D12_CLEAR_FLAG_DEPTH);
    directCommandList.SetRenderTarget(m_DirectionalShadowMap);
    directCommandList.SetPipelineState(m_DepthRenderPSO);
    directCommandList.SetGraphicsRootSignature(m_DepthRenderRootSignature);
    directCommandList.SetViewport(m_ViewPort);
}

void DirectionalLight::RenderObjectToDepth(CommandList& directCommandList, Mesh& mesh, XMMATRIX modelMatrix) const {
    PBRObjectPSO::VertexProps shadowDepthVertexCB;

    XMStoreFloat4x4(&shadowDepthVertexCB.SRT, modelMatrix);
    XMStoreFloat4x4(&shadowDepthVertexCB.MVP, modelMatrix * XMLoadFloat4x4(&m_ViewMatrix) * XMLoadFloat4x4(&m_OrthoMatrix));
    directCommandList.SetGraphicsDynamicConstantBuffer(PBRObjectPSO::PBRRootParameters::VertexCB, shadowDepthVertexCB);
    mesh.Draw(directCommandList);
}
