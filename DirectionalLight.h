#pragma once

// Note: currently hardcoded to only render objects using PBRObjectsPSO

#include <directxmath.h>
#include <d3d12.h>
#include <wrl/client.h>

#include "RenderTarget.h"

using namespace DirectX;
using namespace Microsoft::WRL;

class ShaderResourceView;
class CommandList;
class RootSignature;
class Mesh;

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

    void GenerateViewMatrix();

    XMFLOAT4 GetColor() const { return m_Color; }
    void SetColor(float r, float g, float b);

    XMFLOAT3 GetPosition() const { return m_Position; }

    // rotX, rotY, rotZ is in degrees
    XMFLOAT4 GetDirection() const { return m_Direction; }
    void SetDirection(float rotX, float rotY, float rotZ);

    // Sets direction of light, by rotating default direction (0.0, 0.0, 1.0) by rotation parameters (radians)
    // Updates m_Position and calls GenerateViewMatrix
    void SetQuaternionDirection(XMVECTOR rotationQuaternion);

    float GetShadowBias() const { return m_ShadowBias; }
    void SetShadowBias(float newValue) { m_ShadowBias = newValue; }

    void GetEulerAngles(float& out_X, float& out_Y) const;

    XMFLOAT4X4 GetOrthoMatrix() const { return m_OrthoMatrix; }
    XMFLOAT4X4 GetViewMatrix() const { return m_ViewMatrix; }
    D3D12_VIEWPORT GetViewPort() const { return m_ViewPort; }

    std::shared_ptr<Texture> GetShadowMapTexture() const { return m_DirectionalShadowMap.GetTexture(AttachmentPoint::DepthStencil); }
    
    void SetShadowDepthPipelineStateAndRenderTarget(CommandList& directCommandList) const;
    void RenderObjectToDepth(CommandList& directCommandList, Mesh& mesh, XMMATRIX modelMatrix) const;

private:
    Device& m_Device;

    float m_LightDistance = -100.0f;
    XMFLOAT4 m_Color{};

    // normalized 3d free vector representing direction of light
    XMFLOAT4 m_Direction{};

    float m_ShadowBias{};

    // For shadow mapping
    XMFLOAT3 m_Position;
    XMFLOAT3 m_LookAt;
    XMFLOAT4X4 m_OrthoMatrix;
    XMFLOAT4X4 m_ViewMatrix;
    D3D12_VIEWPORT m_ViewPort;

    RenderTarget m_DirectionalShadowMap;

    /// TODO: these members can be static
    ComPtr<ID3D12PipelineState> m_DepthRenderPSO;
    std::shared_ptr<RootSignature> m_DepthRenderRootSignature;
};

