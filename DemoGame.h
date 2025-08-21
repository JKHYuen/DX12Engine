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
class EditorGui;

class DemoGame : public IGame {
public:
    DemoGame(const std::wstring& name, uint32_t width, uint32_t height, bool vSync = false);
    virtual ~DemoGame();

    uint32_t Run()       override;
    bool Initialize()    override;
    void UnloadContent() override;

    void OnUpdate(UpdateEventArgs& e)          override;
    void OnKeyPressed(KeyEventArgs& e)         override;
    void OnKeyReleased(KeyEventArgs& e)        override;
    void OnMouseWheel(MouseWheelEventArgs& e)  override;
    void OnMouseMoved(MouseMotionEventArgs& e) override;
    void OnResize(ResizeEventArgs& e)          override;

private:
    void OnRender(UpdateEventArgs& e);
    void ShowImGuiWindow(CommandList& directCommandList);

    // NOTE: Can be unique_ptrs?
    std::shared_ptr<Device>    m_Device;
    std::shared_ptr<Window>    m_Window;
    std::shared_ptr<SwapChain> m_SwapChain;

    std::unique_ptr<EditorGui> m_EditorGui;

    RenderTarget m_HDR_MSAA_RenderTarget;
    RenderTarget m_Float_RenderTarget;
    std::shared_ptr<RootSignature> m_PBRRootSignature;
    std::shared_ptr<RootSignature> m_PostProcessRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PBR_PSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_TonemapPSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PostprocessPSO;

    D3D12_VIEWPORT m_ScreenViewport;
    D3D12_RECT     m_DefaultScissorRect;

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

    bool m_IsShiftPressed;

    int  m_Width;
    int  m_Height;
    bool m_IsVsync;

    bool m_ShowImGuiWindow;

    int m_CurrentAvgFPS;
    static const int sk_frameTimeSamples = 128;
    double m_frameTimeHistory[sk_frameTimeSamples] = {};
};

