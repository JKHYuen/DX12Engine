#pragma once
#include "dxgiformat.h"
#include <array>
#include <cstdint>
#include <memory>

class Device;
class RenderTarget;
class CommandList;

// An array of 2 render targets used to chain post processing effects.
// This is needed because post processing effects often need to read and write to the same textures,
// this adds a buffer so it is allowed by DX
class RenderTargetPair {
public:
    RenderTargetPair(Device& device, DXGI_FORMAT colorFormat, uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);

    void ClearInputRT(CommandList& directCommandList);
    void ClearOutputRT(CommandList& directCommandList);
    void SwapRTs();
    RenderTarget& GetInputRT();
    RenderTarget& GetOutputRT();

private:
    enum RTType {
        input  = 0,
        output = 1
    };

    // Non multisampled floating point render textures
    std::array<std::unique_ptr<RenderTarget>, 2> m_RTs {};
};

