#include "Common.hlsli"

// Used to render depth from point light perspective for cubemap shadow mapping
// depth is scaled by light radius so we don't have to deal with non-linear depth and compare world distances directly
// Hardcoded for PBR pipeline, note modifying depth disables early Z testing

struct PointLight {
    float4 WorldPosition;
    float4 ColorInvRadius; // x: r, y: g, z: b, a: inverse light radius (for shader optimization)
};

cbuffer LightCB : register(b1) {
    float4 Time;
    float4 DirLight; // vector of directional light
    float4 DirLightColor;
    matrix DirectionalLightMVP;
    PointLight PointLights[MAX_POINT_LIGHT_COUNT];
};

struct PixelInputType {
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
    float4 worldPosition : TEXCOORD1;
    float4 cameraPosition : TEXCOORD2;
    float4 directionalLightViewPosition : TEXCOORD3;
    float3 tangentViewDirection : TEXCOORD4;
};

// Source: https://learnopengl.com/Advanced-Lighting/Shadows/Point-Shadows
float main(PixelInputType i) : SV_Depth {
    // get distance between fragment and light source
    float lightDistance = length(i.worldPosition.xyz - PointLights[0].WorldPosition.xyz);
    
    // map to [0,1] range by dividing by far plane (i.e. light radius) and write as modified depth
    return lightDistance * PointLights[0].ColorInvRadius.a;
}