#include "Camera.h"

using namespace DirectX;

Camera::Camera(float vFOV, float aspectRatio, float zNear, float zFar)
    : m_ViewDirty(true)
    , m_ProjectionDirty(true)
    , m_vFOV(vFOV)
    , m_AspectRatio(aspectRatio)
    , m_NearZ(zNear)
    , m_FarZ(zFar) {
    pData = (AlignedData*)_aligned_malloc(sizeof(AlignedData), 16);
    pData->m_Translation = XMVectorZero();
    pData->m_Rotation = XMQuaternionIdentity();
}

Camera::~Camera() {
    _aligned_free(pData);
}

void XM_CALLCONV Camera::Set_LookAt(FXMVECTOR eye, FXMVECTOR target, FXMVECTOR up) {
    pData->m_ViewMatrix = XMMatrixLookAtLH(eye, target, up);

    pData->m_Translation = eye;
    pData->m_Rotation = XMQuaternionRotationMatrix(XMMatrixTranspose(pData->m_ViewMatrix));

    m_ViewDirty = false;
}

XMMATRIX Camera::Get_ViewMatrix() const {
    if(m_ViewDirty) {
        UpdateViewMatrix();
    }
    return pData->m_ViewMatrix;
}

void Camera::Set_Projection(float fovy, float aspect, float zNear, float zFar) {
    m_vFOV = fovy;
    m_AspectRatio = aspect;
    m_NearZ = zNear;
    m_FarZ = zFar;

    m_ProjectionDirty = true;
}

XMMATRIX Camera::Get_ProjectionMatrix() const {
    if(m_ProjectionDirty) {
        UpdateProjectionMatrix();
    }

    return pData->m_ProjectionMatrix;
}

void Camera::Set_FoV(float fovy) {
    if(m_vFOV != fovy) {
        m_vFOV = fovy;
        m_ProjectionDirty = true;
    }
}

float Camera::Get_FoV() const {
    return m_vFOV;
}


void XM_CALLCONV Camera::Set_Translation(FXMVECTOR translation) {
    pData->m_Translation = translation;
    m_ViewDirty = true;
}

XMVECTOR Camera::Get_Translation() const {
    return pData->m_Translation;
}

void Camera::Set_Rotation(FXMVECTOR rotation) {
    pData->m_Rotation = rotation;
}

XMVECTOR Camera::Get_Rotation() const {
    return pData->m_Rotation;
}

void XM_CALLCONV Camera::Translate(FXMVECTOR translation, Space space) {
    if(space == Space::Local) {
        pData->m_Translation += XMVector3Rotate(translation, pData->m_Rotation);
    }
    else {
        pData->m_Translation += translation;
    }

    pData->m_Translation = XMVectorSetW(pData->m_Translation, 1.0f);

    m_ViewDirty = true;
}

void Camera::Rotate(FXMVECTOR quaternion) {
    pData->m_Rotation = XMQuaternionMultiply(quaternion, pData->m_Rotation);
    m_ViewDirty = true;
}

void Camera::UpdateViewMatrix() const {
    XMMATRIX rotationMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(pData->m_Rotation));
    XMMATRIX translationMatrix = XMMatrixTranslationFromVector(-(pData->m_Translation));

    pData->m_ViewMatrix = translationMatrix * rotationMatrix;
    m_ViewDirty = false;
}

void Camera::UpdateProjectionMatrix() const {
    pData->m_ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_vFOV), m_AspectRatio, m_NearZ, m_FarZ);
    m_ProjectionDirty = false;
}