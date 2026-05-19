#pragma once

#include "Events.h"
#include "Camera.h"
#include "Scene.h"
#include "RenderTarget.h"
#include "Texture.h"
#include "IGame.h"

#include <memory>
#include <format>
#include <wrl/client.h>
#include <DirectXMath.h>

class CommandList;
class RootSignature;
class Device;
class SwapChain;
class Texture;
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
class ShaderResourceView;

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

private:
    /// TODO: Add some convenient way to get next set of RTs, currently manually indexing
    // An array of 2 render targets used to chain post processing effects.
    // This is needed because post processing effects often need to read and write to the same textures,
    // this adds a buffer so it is allowed by DX
    struct PostProcessRenderTargets {
        PostProcessRenderTargets() = default;
        PostProcessRenderTargets(Device& device, DXGI_FORMAT colorFormat, uint32_t width, uint32_t height) {
            auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                colorFormat, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
            );

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
            srvDesc.Format = colorFormat;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;

            for(int i = 0; i < 2; i++) {
                auto texture = std::make_shared<Texture>(device, textureDesc, nullptr, true);
                texture->SetName(std::format(L"Post Process Render Target {}", i));
                RTs[i].AttachTexture(AttachmentPoint::Color0, texture);
                texture->CreateShaderResourceView(srvDesc);
            }
        }

        void Resize(uint32_t width, uint32_t height) {
            RTs[0].Resize(width, height);
            RTs[1].Resize(width, height);
        }

        // Non multisampled floating point render textures
        RenderTarget RTs[2] {};
    } m_PostProcessRTs {};

    void OnRender(const UpdateEventArgs& e);

    std::shared_ptr<Device>    m_Device;
    std::shared_ptr<Window>    m_Window;
    std::shared_ptr<SwapChain> m_SwapChain;

    RenderTarget m_HDR_MSAA_RT {};

    /// TODO: figure out more generalized PSO loading system
    std::unique_ptr<PBRObjectPSO> m_PBR_PSO;
    std::unique_ptr<UnlitPSO> m_Unlit_PSO;
    std::unique_ptr<UnlitPrimitivePSO> m_UnlitPrimitive_PSO;
    std::unique_ptr<ImageBasedLightingPSO> m_IBL_PSO;
    std::unique_ptr<BloomPSO> m_Bloom_PSO;
    ///
    std::unique_ptr<BloomEffect> m_BloomEffect;
    std::unique_ptr<OutlineEffect> m_OutlineEffect;

    std::shared_ptr<RootSignature> m_PostProcessRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_TonemapPSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PostprocessPSO;

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

