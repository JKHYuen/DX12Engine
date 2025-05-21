#pragma once
#include <DirectXMath.h>
#include <memory>
#include <wrl/client.h>

#include "Events.h"
#include "Camera.h"
#include "RenderTarget.h"
#include "IGame.h"

class CommandList;
class RootSignature;
class Device;
class SwapChain;
class Texture;
class Window;

class DemoGame : public IGame {
public:
    DemoGame(const std::wstring& name, uint32_t width, uint32_t height, bool vSync = false);
    virtual ~DemoGame();

    uint32_t Run()       override;
    bool LoadContent()   override;
    void UnloadContent() override;

    void OnUpdate(UpdateEventArgs& e)          override;
    void OnKeyPressed(KeyEventArgs& e)         override;
    void OnKeyReleased(KeyEventArgs& e)        override;
    void OnMouseWheel(MouseWheelEventArgs& e)  override;
    void OnMouseMoved(MouseMotionEventArgs& e) override;
    void OnResize(ResizeEventArgs& e)          override;

private:
    void OnRender(UpdateEventArgs& e);

    // NOTE: Can be unique_ptr?
    std::shared_ptr<Window>    m_Window;
    std::shared_ptr<Device>    m_Device;
    std::shared_ptr<SwapChain> m_SwapChain;

    RenderTarget m_RenderTarget;
    std::shared_ptr<RootSignature> m_RootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_D3d12PipelineState;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT     m_ScissorRect;

    // TODO: use MiniEngine Math headers
    Camera m_Camera;
    struct alignas(16) CameraData {
        DirectX::XMVECTOR m_InitialCamPos;
        DirectX::XMVECTOR m_InitialCamRot;
    };
    CameraData* m_pAlignedCameraData;

    // Camera Controller
    float m_Forward;
    float m_Backward;
    float m_Left;
    float m_Right;
    float m_Up;
    float m_Down;

    float m_Pitch;
    float m_Yaw;

    bool m_ShiftPressed;

    int  m_Width;
    int  m_Height;
    bool m_Vsync;
};

