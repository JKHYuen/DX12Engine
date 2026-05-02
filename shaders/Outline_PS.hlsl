
cbuffer LightCB : register(b1) {
    float4 Time;
    float4 DirLight; // vector of directional light
    float4 DirLightColor;
    float4 OutlineColor;
};

struct PixelInputType {
    float4 position : SV_POSITION;
};

float4 main() : SV_TARGET {
    return OutlineColor;
}