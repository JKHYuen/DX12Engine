#include "DirectionalLight.h"
#include <cmath>

using namespace DirectX;

DirectionalLight::DirectionalLight(XMFLOAT3 color, XMFLOAT3 eulerDir, int shadowMapResolution, float shadowDistance, float shadowMapNearZ, float shadowMapFarZ, float shadowBias)
    : m_ShadowBias(shadowBias)
    , m_Color(XMFLOAT4(color.x, color.y, color.z, 1.0f))
    , m_ViewPort(D3D12_VIEWPORT(0.0f, 0.0f, (float)shadowMapResolution, (float)shadowMapResolution, 0.0f, 1.0f)) {

    XMStoreFloat4x4(&m_OrthoMatrix, XMMatrixOrthographicLH(shadowDistance, shadowDistance, shadowMapNearZ, shadowMapFarZ));
    SetDirection(eulerDir.x, eulerDir.y, eulerDir.z);
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