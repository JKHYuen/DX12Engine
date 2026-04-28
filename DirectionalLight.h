#pragma once

// Note: currently hardcoded to only render objects using PBRObjectsPSO

#include <directxmath.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <wrl/client.h>

#include "RenderTarget.h"

using namespace DirectX;
using namespace Microsoft::WRL;

class ShaderResourceView;
class CommandList;
class RootSignature;
class Mesh;
class Device;

class DirectionalLight {
public:
    // Root signature and Input layout is for shadow caster depth rendering on shadow map
    // 
    // Note: eulerDir is in radians
    struct DirectionalLightParams {
        std::shared_ptr<RootSignature> depthRenderRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT depthRenderInputLayout;
        XMFLOAT3 color;
        XMFLOAT3 eulerDir;
        int shadowMapResolution;
        float shadowDistance, shadowMapNearZ, shadowMapFarZ, shadowBias;
    };

    DirectionalLight(Device& device, DirectionalLightParams params);

    DirectionalLight(const DirectionalLight&)            = delete;
    DirectionalLight& operator=(const DirectionalLight&) = delete;
    DirectionalLight(DirectionalLight&&)                 = delete;
    DirectionalLight& operator=(DirectionalLight&&)      = delete;

    void GenerateViewMatrix();

    XMFLOAT4 GetColor() const { return m_Color; }
    void SetColor(float r, float g, float b);

    XMFLOAT3 GetPosition() const { return m_Position; }

    // rotX, rotY, rotZ is in degrees
    void SetEulerAngle(float rotX, float rotY, float rotZ);

    XMFLOAT4 GetNormDirectionVector() const { return m_NormDirectionVector; }

    // Sets direction of light, by rotating default direction (0.0, 0.0, 1.0) by rotation parameters (radians)
    // Updates m_Position and calls GenerateViewMatrix
    void SetQuaternionAngle(XMVECTOR rotationQuaternion);

    float GetShadowBias() const { return m_ShadowBias; }
    void SetShadowBias(float newValue) { m_ShadowBias = newValue; }

    XMFLOAT3 GetEulerAngles() const { return m_EulerAngles; }

    XMFLOAT4X4 GetOrthoMatrix() const { return m_OrthoMatrix; }
    XMFLOAT4X4 GetViewMatrix() const { return m_ViewMatrix; }
    D3D12_VIEWPORT GetViewPort() const { return m_ViewPort; }

    std::shared_ptr<Texture> GetShadowMapTexture() const { return m_DirectionalShadowMap.GetTexture(AttachmentPoint::DepthStencil); }
    
    void SetShadowDepthPipelineStateAndRenderTarget(CommandList& directCommandList) const;
    void RenderObjectToDepth(CommandList& directCommandList, Mesh& mesh, XMMATRIX modelMatrix) const;

private:
    Device& m_Device;

    // normalized 3d free vector representing direction of light
    XMFLOAT4 m_NormDirectionVector;
    // Keep euler angle representation for user facing values
    XMFLOAT3 m_EulerAngles;


    /// For shadow mapping
    float m_LightDistance;
    float m_ShadowBias;
    XMFLOAT4 m_Color{};
    XMFLOAT3 m_Position;
    XMFLOAT3 m_LookAt;
    XMFLOAT4X4 m_OrthoMatrix;
    XMFLOAT4X4 m_ViewMatrix;
    D3D12_VIEWPORT m_ViewPort;

    RenderTarget m_DirectionalShadowMap;
    ///

    /// TODO: these members can be static
    ComPtr<ID3D12PipelineState> m_DepthRenderPSO;
    std::shared_ptr<RootSignature> m_DepthRenderRootSignature;
};

