#pragma once

#include <DirectXMath.h>

// For translations only
enum class Space {
    Local,
    World,
};

class Camera {
public:
    Camera();
    ~Camera();

    void XM_CALLCONV set_LookAt(DirectX::FXMVECTOR eye, DirectX::FXMVECTOR target, DirectX::FXMVECTOR up);
    DirectX::XMMATRIX get_ViewMatrix() const;

    void set_Projection(float fovy, float aspect, float zNear, float zFar);
    DirectX::XMMATRIX get_ProjectionMatrix() const;

    void set_FoV(float fovy);
    float get_FoV() const;

    /**
     * Set the camera's position in world-space.
     */
    void XM_CALLCONV set_Translation(DirectX::FXMVECTOR translation);
    DirectX::XMVECTOR get_Translation() const;

    /**
     * Set the camera's rotation in world-space.
     * @param rotation The rotation quaternion.
     */
    void XM_CALLCONV set_Rotation(DirectX::FXMVECTOR rotation);
    DirectX::XMVECTOR get_Rotation() const;

    void XM_CALLCONV Translate(DirectX::FXMVECTOR translation, Space space = Space::Local);
    void Rotate(DirectX::FXMVECTOR quaternion);

protected:
    virtual void UpdateViewMatrix() const;
    virtual void UpdateProjectionMatrix() const;

    // TODO: replace with FLOAT4X4A
    // This data must be aligned otherwise the SSE intrinsics fail
    // and throw exceptions.
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
    float m_vFoV;   
    float m_AspectRatio; 
    float m_zNear;     
    float m_zFar;      

    // True if the view matrices needs to be updated
    mutable bool m_ViewDirty;
    mutable bool m_ProjectionDirty;
};

