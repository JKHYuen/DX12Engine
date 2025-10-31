#pragma once

#include <DirectXMath.h>

// For translations only
enum class Space {
    Local,
    World,
};

class Camera {
public:
    // Note: member values are updated in setProjection function, which should be called on window resize (called on window startup)
    Camera(float vFOV = 45.0f, float aspectRatio = 1.0f, float zNear = 0.1f, float zFar = 100.0f);
    ~Camera();

    void XM_CALLCONV Set_LookAt(DirectX::FXMVECTOR eye, DirectX::FXMVECTOR target, DirectX::FXMVECTOR up);
    DirectX::XMMATRIX Get_ViewMatrix() const;

    void Set_Projection(float fovy, float aspect, float zNear, float zFar);
    DirectX::XMMATRIX Get_ProjectionMatrix() const;

    void Set_FoV(float fovy);
    float Get_FoV() const;

    /**
     * Set the camera's position in world-space.
     */
    void XM_CALLCONV Set_Translation(DirectX::FXMVECTOR translation);
    DirectX::XMVECTOR Get_Translation() const;

    /**
     * Set the camera's rotation in world-space.
     * @param rotation The rotation quaternion.
     */
    void XM_CALLCONV Set_Rotation(DirectX::FXMVECTOR rotation);
    DirectX::XMVECTOR Get_Rotation() const;

    void XM_CALLCONV Translate(DirectX::FXMVECTOR translation, Space space = Space::Local);
    void Rotate(DirectX::FXMVECTOR quaternion);

protected:
    virtual void UpdateViewMatrix() const;
    virtual void UpdateProjectionMatrix() const;

    // This data must be aligned otherwise the SSE intrinsics fail and throw exceptions.
    __declspec(align(16)) struct AlignedData {
        // World-space position of the camera.
        DirectX::XMVECTOR m_Translation;
        // World-space rotation of the camera.
        // THIS IS A QUATERNION!!!!
        DirectX::XMVECTOR m_Rotation;

        DirectX::XMMATRIX m_ViewMatrix, m_InverseViewMatrix;
        DirectX::XMMATRIX m_ProjectionMatrix, m_InverseProjectionMatrix;
    };
    AlignedData* pData;

    // projection parameters
    float m_vFOV;   
    float m_AspectRatio; 
    float m_NearZ;     
    float m_FarZ;      

    // True if the view matrices needs to be updated
    mutable bool m_ViewDirty;
    mutable bool m_ProjectionDirty;
};

