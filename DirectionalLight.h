#pragma once

#include <directxmath.h>

using namespace DirectX;

class DirectionalLight {
public:
    // Note: eulerDir is in radians
    DirectionalLight(XMFLOAT3 color, XMFLOAT3 eulerDir, float shadowBias = 0.001f);

    void GenerateOrthoMatrix(float width, float nearPlane, float depthPlane);
    void GenerateViewMatrix();

    XMFLOAT4 GetColor() const { return m_Color; }
    void SetColor(float r, float g, float b);

    XMFLOAT3 GetPosition() const { return m_Position; }

    XMFLOAT3 GetDirection() const { return m_Direction; }
    void SetDirection(float rotX, float rotY, float rotZ);

    // Sets direction of light, by rotating default direction (0.0, 0.0, 1.0) by rotation parameters (radians)
// Updates m_Position and calls GenerateViewMatrix
    void SetQuaternionDirection(XMVECTOR rotationQuaternion);

    float GetShadowBias() const { return m_ShadowBias; }
    void SetShadowBias(float newValue) { m_ShadowBias = newValue; }

    void GetEulerAngles(float& out_X, float& out_Y) const;

    void GetOrthoMatrix(XMMATRIX& orthoMatrix) const { orthoMatrix = m_OrthoMatrix; }
    void GetViewMatrix(XMMATRIX& viewMatrix) const { viewMatrix = m_ViewMatrix; }

private:
    float m_LightDistance = -100.0f;
    XMFLOAT4 m_Color{};

    // normalized 3d free vector representing direction of light
    XMFLOAT3 m_Direction{};

    float m_ShadowBias{};

    // For shadow mapping
    XMFLOAT3 m_Position{};
    XMFLOAT3 m_LookAt{};
    XMMATRIX m_OrthoMatrix{};
    XMMATRIX m_ViewMatrix{};
};

