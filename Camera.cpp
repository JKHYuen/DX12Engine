#include "Camera.h"
#include "Logger.h"
#include <DirectXCollision.h>

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
    m_ViewDirty = true;
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

// Note: XMVector4NormalizeEst is used
// Adapted from https://rastertek.com/dx11win10tut23.html
// Far culling frustum plane is same as camera's, need to edit projection matrix if custom distance needed
void Camera::UpdateFrustum() {
    if(!m_ViewDirty && !m_ProjectionDirty) {
        return;
    }

    // Create the frustum matrix 
    XMMATRIX tempMatrix = XMMatrixMultiply(Get_ViewMatrix(), Get_ProjectionMatrix());
    XMFLOAT4X4 viewProjMat {};
    XMStoreFloat4x4(&viewProjMat, tempMatrix);

    XMVECTOR normalizedVec {};

    // Calc normalized near plane of the frustum
    m_FrustumPlanes[0].x = viewProjMat._13;
    m_FrustumPlanes[0].y = viewProjMat._23;
    m_FrustumPlanes[0].z = viewProjMat._33;
    m_FrustumPlanes[0].w = viewProjMat._43;
    normalizedVec = XMVector4NormalizeEst(XMLoadFloat4(&m_FrustumPlanes[0]));
    XMStoreFloat4(&m_FrustumPlanes[0], normalizedVec);

    // Calc normalized far plane of frustum
    m_FrustumPlanes[1].x = viewProjMat._14 - viewProjMat._13;
    m_FrustumPlanes[1].y = viewProjMat._24 - viewProjMat._23;
    m_FrustumPlanes[1].z = viewProjMat._34 - viewProjMat._33;
    m_FrustumPlanes[1].w = viewProjMat._44 - viewProjMat._43;
    normalizedVec = XMVector4NormalizeEst(XMLoadFloat4(&m_FrustumPlanes[1]));
    XMStoreFloat4(&m_FrustumPlanes[1], normalizedVec);

    // Get normalized left plane of frustum
    m_FrustumPlanes[2].x = viewProjMat._14 + viewProjMat._11;
    m_FrustumPlanes[2].y = viewProjMat._24 + viewProjMat._21;
    m_FrustumPlanes[2].z = viewProjMat._34 + viewProjMat._31;
    m_FrustumPlanes[2].w = viewProjMat._44 + viewProjMat._41;
    normalizedVec = XMVector4NormalizeEst(XMLoadFloat4(&m_FrustumPlanes[2]));
    XMStoreFloat4(&m_FrustumPlanes[2], normalizedVec);

    // Calc normalized right plane of frustum
    m_FrustumPlanes[3].x = viewProjMat._14 - viewProjMat._11;
    m_FrustumPlanes[3].y = viewProjMat._24 - viewProjMat._21;
    m_FrustumPlanes[3].z = viewProjMat._34 - viewProjMat._31;
    m_FrustumPlanes[3].w = viewProjMat._44 - viewProjMat._41;
    normalizedVec = XMVector4NormalizeEst(XMLoadFloat4(&m_FrustumPlanes[3]));
    XMStoreFloat4(&m_FrustumPlanes[3], normalizedVec);

    // Calc normalized top plane of frustum
    m_FrustumPlanes[4].x = viewProjMat._14 - viewProjMat._12;
    m_FrustumPlanes[4].y = viewProjMat._24 - viewProjMat._22;
    m_FrustumPlanes[4].z = viewProjMat._34 - viewProjMat._32;
    m_FrustumPlanes[4].w = viewProjMat._44 - viewProjMat._42;
    normalizedVec = XMVector4NormalizeEst(XMLoadFloat4(&m_FrustumPlanes[4]));
    XMStoreFloat4(&m_FrustumPlanes[4], normalizedVec);

    // Calc normalized bottom plane of frustum
    m_FrustumPlanes[5].x = viewProjMat._14 + viewProjMat._12;
    m_FrustumPlanes[5].y = viewProjMat._24 + viewProjMat._22;
    m_FrustumPlanes[5].z = viewProjMat._34 + viewProjMat._32;
    m_FrustumPlanes[5].w = viewProjMat._44 + viewProjMat._42;
    normalizedVec = XMVector4NormalizeEst(XMLoadFloat4(&m_FrustumPlanes[5]));
    XMStoreFloat4(&m_FrustumPlanes[5], normalizedVec);
}

bool Camera::CheckAABBInFrustum(const BoundingBox& aabb, float bias) const {
    for(int i = 0; i < 6; i++) {
        if (m_FrustumPlanes[i].x * (aabb.Center.x - aabb.Extents.x) +
            m_FrustumPlanes[i].y * (aabb.Center.y - aabb.Extents.y) +
            m_FrustumPlanes[i].z * (aabb.Center.z - aabb.Extents.z) + m_FrustumPlanes[i].w >= -bias) {
            continue;
        }

        if (m_FrustumPlanes[i].x * (aabb.Center.x + aabb.Extents.x) +
            m_FrustumPlanes[i].y * (aabb.Center.y - aabb.Extents.y) +
            m_FrustumPlanes[i].z * (aabb.Center.z - aabb.Extents.z) + m_FrustumPlanes[i].w >= -bias) {
            continue;
        }

        if (m_FrustumPlanes[i].x * (aabb.Center.x - aabb.Extents.x) +
            m_FrustumPlanes[i].y * (aabb.Center.y + aabb.Extents.y) +
            m_FrustumPlanes[i].z * (aabb.Center.z - aabb.Extents.z) + m_FrustumPlanes[i].w >= -bias) {
            continue;
        }

        if (m_FrustumPlanes[i].x * (aabb.Center.x - aabb.Extents.x) +
            m_FrustumPlanes[i].y * (aabb.Center.y - aabb.Extents.y) +
            m_FrustumPlanes[i].z * (aabb.Center.z + aabb.Extents.z) + m_FrustumPlanes[i].w >= -bias) {
            continue;
        }

        if (m_FrustumPlanes[i].x * (aabb.Center.x + aabb.Extents.x) +
            m_FrustumPlanes[i].y * (aabb.Center.y + aabb.Extents.y) +
            m_FrustumPlanes[i].z * (aabb.Center.z - aabb.Extents.z) + m_FrustumPlanes[i].w >= -bias) {
            continue;
        }

        if (m_FrustumPlanes[i].x * (aabb.Center.x + aabb.Extents.x) +
            m_FrustumPlanes[i].y * (aabb.Center.y - aabb.Extents.y) +
            m_FrustumPlanes[i].z * (aabb.Center.z + aabb.Extents.z) + m_FrustumPlanes[i].w >= -bias) {
            continue;
        }

        if (m_FrustumPlanes[i].x * (aabb.Center.x - aabb.Extents.x) +
            m_FrustumPlanes[i].y * (aabb.Center.y + aabb.Extents.y) +
            m_FrustumPlanes[i].z * (aabb.Center.z + aabb.Extents.z) + m_FrustumPlanes[i].w >= -bias) {
            continue;
        }

        if (m_FrustumPlanes[i].x * (aabb.Center.x + aabb.Extents.x) +
            m_FrustumPlanes[i].y * (aabb.Center.y + aabb.Extents.y) +
            m_FrustumPlanes[i].z * (aabb.Center.z + aabb.Extents.z) + m_FrustumPlanes[i].w >= -bias) {
            continue;
        }

        return false;
    }

    return true;
}

void Camera::UpdateViewMatrix() const {
    XMMATRIX rotationMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(pData->m_Rotation));
    XMMATRIX translationMatrix = XMMatrixTranslationFromVector(-(pData->m_Translation));

    pData->m_ViewMatrix = XMMatrixMultiply(translationMatrix, rotationMatrix);
    m_ViewDirty = false;
}

void Camera::UpdateProjectionMatrix() const {
    pData->m_ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_vFOV), m_AspectRatio, m_NearZ, m_FarZ);
    m_ProjectionDirty = false;
}