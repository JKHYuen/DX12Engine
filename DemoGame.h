#pragma once
#include <DirectXMath.h>
#include <memory>
#include <wrl/client.h>

#include "Events.h"
#include "Camera.h"
#include "Scene.h"
#include "RenderTarget.h"
#include "IGame.h"

class CommandList;
class RootSignature;
class Device;
class SwapChain;
class Texture;
class Window;
class EditorGui;
class Skybox;
class PBRObjectPSO;
class ShaderResourceView;

class DemoGame : public IGame {
public:
    DemoGame(const std::wstring& name, uint32_t width, uint32_t height, bool vSync = false);

    uint32_t Run()       override;
    bool Initialize()    override;
    void UnloadContent() override;

    void OnUpdate(UpdateEventArgs& e)                   override;
    void OnResize(ResizeEventArgs& e)                   override;
    void OnKeyPressed(KeyEventArgs& e)                  override;
    void OnKeyReleased(KeyEventArgs& e)                 override;
    void OnMouseWheel(MouseWheelEventArgs& e)           override;
    void OnMouseMove(MouseMotionEventArgs& e)           override;
    void OnMouseButtonPressed(MouseButtonEventArgs& e)  override;
    void OnMouseButtonReleased(MouseButtonEventArgs& e) override;

private:
    void OnRender(UpdateEventArgs& e);
    
    // Debug window, this shouldn't be implemented in this class if it ever becomes a real editor UI
    void ShowImGui(CommandList& directCommandList);

    std::shared_ptr<Device>    m_Device;
    std::shared_ptr<Window>    m_Window;
    std::shared_ptr<SwapChain> m_SwapChain;

    std::unique_ptr<EditorGui> m_EditorGui;

    RenderTarget m_HDR_MSAA_RenderTarget;
    RenderTarget m_FloatRenderTarget;

    /// TODO: make these unique_ptr?
    std::shared_ptr<PBRObjectPSO> m_PBR_PSO;

    std::shared_ptr<RootSignature> m_PostProcessRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_TonemapPSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PostprocessPSO;

    D3D12_VIEWPORT m_ScreenViewport;
    D3D12_RECT     m_DefaultScissorRect;

    // Pointer because this can not be initialized on DemoGame construction
    std::unique_ptr<Scene> m_Scene;

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

    int  m_WindowWidth;
    int  m_WindowHeight;
    bool m_IsVsync;

    bool m_ShowImGuiWindow;

    int m_CurrentAvgFPS;
    static const int sk_frameTimeSamples = 128;
    double m_frameTimeHistory[sk_frameTimeSamples] = {};
};

