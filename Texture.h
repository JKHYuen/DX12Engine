#pragma once

/*
 *  Copyright(c) 2018 Jeremiah van Oosten
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files(the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions :
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

 /**
  *  @file Texture.h
  *  @date October 24, 2018
  *  @author Jeremiah van Oosten
  *
  *  @brief A wrapper for a DX12 Texture object.
  */


#include "Resource.h"
#include "DescriptorAllocation.h"

class Device;
class ShaderResourceView;

class Texture : public Resource {
public:
    // Creates COMMITTED resource (through resource constructor), 
    // automatically added to resource state tracker ONLY if first constructor is used (i.e. one that accepts const D3D12_RESOURCE_DESC& resourceDesc)
    // b_CreateDefaultView: optionally creates descriptor (resource view) based on resourceDesc/resource description
    Texture(Device& device, const D3D12_RESOURCE_DESC& resourceDesc, const D3D12_CLEAR_VALUE* clearValue = nullptr, bool b_CreateDefaultView = true);
    Texture(Device& device, Microsoft::WRL::ComPtr<ID3D12Resource> resource, const D3D12_CLEAR_VALUE* clearValue = nullptr, bool b_CreateDefaultView = true);

    /**
     * Resize the texture by recreating resource
     */
    void Resize(uint32_t width, uint32_t height, uint32_t depthOrArraySize = 1);

    /**
     * Create non-default RTV
     */
    void CreateRenderTargetView(const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc);

    /**
     * Create non-default SRV
     */
    void CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc);

    /**
     * Get the RTV for the texture.
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetViewHandle() const;

    /**
     * Get the DSV for the texture.
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilViewHandle() const;

    /**
     * Get the default SRV for the texture.
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetShaderResourceViewHandle() const;

    /**
     * Get the UAV for the texture at a specific mip level.
     * Note: Only only supported for 1D and 2D textures.
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetUnorderedAccessViewHandle(uint32_t mip) const;

    bool CheckSRVSupport() const {
        return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE);
    }

    bool CheckRTVSupport() const {
        return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_RENDER_TARGET);
    }

    bool CheckUAVSupport() const {
        return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
               CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
               CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
    }

    bool CheckDSVSupport() const {
        return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL);
    }

    /**
     * Check to see if the image format has an alpha channel.
     */
    bool HasAlpha() const;

    /**
     * Check the number of bits per pixel.
     */
    size_t BitsPerPixel() const;

    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }

    static bool IsUAVCompatibleFormat(DXGI_FORMAT format);
    static bool IsSRGBFormat(DXGI_FORMAT format);
    static bool IsBGRFormat(DXGI_FORMAT format);
    static bool IsDepthFormat(DXGI_FORMAT format);

    // Return a typeless format from the given format.
    static DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT format);
    // Return an sRGB format in the same format family.
    static DXGI_FORMAT GetSRGBFormat(DXGI_FORMAT format);
    static DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT format);

private:
    void CreateDefaultViews();

    uint32_t m_Width;
    uint32_t m_Height;

    DescriptorAllocation m_RTVAlloc;
    DescriptorAllocation m_DSVAlloc;
    DescriptorAllocation m_SRVAlloc;
    DescriptorAllocation m_UAVAlloc;
};




