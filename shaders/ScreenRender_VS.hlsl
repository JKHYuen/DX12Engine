//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//
// A vertex shader for full-screen effects without a vertex buffer.  The
// intent is to output an over-sized triangle that encompasses the entire
// screen.  By doing so, we avoid rasterization inefficiency that could
// result from drawing two triangles with a shared edge.
//
// Use null input layout
// Draw(3)
// source: https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/ScreenQuadPresentVS.hlsl

struct PixelInputType {
    float2 TexCoord : TEXCOORD0;
    float4 Position : SV_POSITION;
};

PixelInputType main(uint VertexID : SV_VertexID) {
    PixelInputType OUT;
    // Texture coordinates range [0, 2], but only [0, 1] appears on screen.
    OUT.TexCoord = float2(uint2(VertexID, VertexID << 1) & 2);
    OUT.Position = float4(lerp(float2(-1, 1), float2(1, -1), OUT.TexCoord), 0, 1);
    
    return OUT;
}