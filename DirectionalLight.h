#pragma once

#include <directxmath.h>
#include <d3d12.h>

using namespace DirectX;

class DirectionalLight {
public:
    // Note: eulerDir is in radians
    DirectionalLight(XMFLOAT3 color, XMFLOAT3 eulerDir, int shadowMapResolution, float shadowDistance, float shadowMapNearZ, float shadowMapFarZ, float shadowBias);

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

private:
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
};

