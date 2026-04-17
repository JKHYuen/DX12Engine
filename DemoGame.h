#pragma once

#include "Events.h"
#include "Camera.h"
#include "Scene.h"
#include "RenderTarget.h"
#include "IGame.h"

#include <DirectXMath.h>
#include <memory>
#include <wrl/client.h>

class CommandList;
class RootSignature;
class Device;
class SwapChain;
class Texture;
class Window;
class EditorGui;
class Skybox;
class PBRObjectPSO;
class OutlinePSO;
class BloomPSO;
class BloomPass;
class ImageBasedLightingPSO;
class ShaderResourceView;

class DemoGame : public IGame {
public:
    DemoGame(const std::wstring& name, uint32_t width, uint32_t height, bool vSync = false);

    // Called by main
    uint32_t Run()       override;
    bool Initialize()    override;

    void OnUpdate(const UpdateEventArgs & e)                  override;
    void OnResize(const ResizeEventArgs & e)                  override;
    void OnKeyPressed(const KeyEventArgs& e)                  override;
    void OnKeyReleased(const KeyEventArgs& e)                 override;
    void OnMouseWheel(const MouseWheelEventArgs& e)           override;
    void OnMouseMove(const MouseMotionEventArgs& e)           override;
    void OnMouseButtonPressed(const MouseButtonEventArgs& e)  override;
    void OnMouseButtonReleased(const MouseButtonEventArgs& e) override;

private:
    void OnRender(const UpdateEventArgs& e);
    
    // Debug window, this shouldn't be implemented in this class if it ever becomes a real editor UI
    void RenderImGui(CommandList& directCommandList);

    std::shared_ptr<Device>    m_Device;
    std::shared_ptr<Window>    m_Window;
    std::shared_ptr<SwapChain> m_SwapChain;

    std::unique_ptr<EditorGui> m_EditorGui;

    RenderTarget m_HDR_MSAA_RT;
    RenderTarget m_MSAAResolveDstRT;    // destination of MSAA resolve
    RenderTarget m_PostProcessOutputRT; // Intermediate render target between post process passes

    /// TODO: figure out more generalized PSO loading system
    std::unique_ptr<PBRObjectPSO> m_PBR_PSO;
    std::unique_ptr<OutlinePSO> m_Outline_PSO;
    std::unique_ptr<ImageBasedLightingPSO> m_IBL_PSO;
    std::unique_ptr<BloomPSO> m_Bloom_PSO;
    ///
    std::unique_ptr<BloomPass> m_BloomPass;

    std::shared_ptr<RootSignature> m_PostProcessRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_TonemapPSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PostprocessPSO;

    D3D12_VIEWPORT m_ScreenViewport;
    D3D12_RECT     m_DefaultScissorRect;

    // Selectable skyboxes loaded from assets/cubemaps
    std::vector<std::wstring> m_SkyboxNames;

    // Pointer because this can not be initialized on DemoGame construction
    std::unique_ptr<Scene> m_TestScene;

    // Camera Controller
    float m_Forward;
    float m_Backward;
    float m_Left;
    float m_Right;
    float m_Up;
    float m_Down;

    float m_Pitch;
    float m_Yaw;

    // A better input system would not need these variables
    bool m_IsShiftPressed;
    bool m_IsLeftClickPressed;
    bool m_IsRightClickPressed;

    int  m_WindowWidth;
    int  m_WindowHeight;
    bool m_IsVsync;

    int m_CurrentAvgFPS;
    static const int sk_frameTimeSamples = 128;
    double m_frameTimeHistory[sk_frameTimeSamples] = {};

};

