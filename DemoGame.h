#pragma once

#include "DX12EngineCore/IGame.h"

#include "d3d12.h"
#include "Events.h"
#include "Scene.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CommandList;
class RootSignature;
class Device;
class SwapChain;
class Window;
class EditorGui;
class Skybox;
class PBRObjectPSO;
class UnlitPSO;
class UnlitPrimitivePSO;
class BloomPSO;
class BloomEffect;
class OutlineEffect;
class ImageBasedLightingPSO;
class TonemapPSO;
class ShaderResourceView;
class RenderTargetPair;
class RenderTarget;

class DemoGame : public IGame {

    friend class EditorGui;

public:
    DemoGame(const std::wstring& name, uint32_t width, uint32_t height, bool vSync = false, bool isFullScreen = true);

    // Called by main
    uint32_t Run()       override;

    void OnUpdate(const UpdateEventArgs & e)                  override;
    void OnResize(const ResizeEventArgs & e)                  override;
    void OnKeyPressed(const KeyEventArgs& e)                  override;
    void OnKeyReleased(const KeyEventArgs& e)                 override;
    void OnMouseWheel(const MouseWheelEventArgs& e)           override;
    void OnMouseMove(const MouseMotionEventArgs& e)           override;
    void OnMouseButtonPressed(const MouseButtonEventArgs& e)  override;
    void OnMouseButtonReleased(const MouseButtonEventArgs& e) override;

    uint32_t GetWindowWidth()  const override { return m_WindowWidth;  }
    uint32_t GetWindowHeight() const override { return m_WindowHeight; }

private:
    void OnRender(const UpdateEventArgs& e);

    std::unique_ptr<RenderTargetPair> m_PostProcessRTs;
    std::unique_ptr<RenderTarget> m_HDR_MSAA_RT;

    std::shared_ptr<Device>    m_Device;
    std::shared_ptr<Window>    m_Window;
    std::shared_ptr<SwapChain> m_SwapChain;

    /// TODO: figure out more generalized PSO loading system
    std::unique_ptr<PBRObjectPSO> m_PBR_PSO;
    std::unique_ptr<UnlitPSO> m_Unlit_PSO;
    std::unique_ptr<UnlitPrimitivePSO> m_UnlitPrimitive_PSO;
    std::unique_ptr<ImageBasedLightingPSO> m_IBL_PSO;
    std::unique_ptr<BloomPSO> m_Bloom_PSO;
    std::unique_ptr<TonemapPSO> m_Tonemap_PSO;
    ///

    std::unique_ptr<BloomEffect> m_BloomEffect;
    std::unique_ptr<OutlineEffect> m_OutlineEffect;

    std::shared_ptr<RootSignature> m_PostProcessRootSignature;

    D3D12_RECT m_DefaultScissorRect;

    // Selectable skyboxes loaded from assets/cubemaps
    std::vector<std::wstring> m_SkyboxNames;

    // Pointer because this can not be initialized on DemoGame construction
    std::unique_ptr<Scene> m_DemoScene;

    // Camera Controller
    float m_Forward {};
    float m_Backward {};
    float m_Left {};
    float m_Right {};
    float m_Up {};
    float m_Down {};

    float m_CameraPitch {};
    float m_CameraYaw {};

    // Input state in current frame
    // A better input system would simplify need these variables
    bool m_LeftShiftPressed {};
    bool m_LeftControlPressed {};
    bool m_LeftClickPressed {};
    bool m_RightClickPressed {};
    bool m_MouseMoved {};

    uint32_t m_WindowWidth;
    uint32_t m_WindowHeight;
    bool m_IsVsync;

    int m_CurrentAvgFPS {};
    static const int sk_frameTimeSamples = 128;
    double m_frameTimeHistory[sk_frameTimeSamples] = {};
};

