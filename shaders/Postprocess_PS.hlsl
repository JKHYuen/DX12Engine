Texture2D screenTexture : register(t0);
SamplerState ClampSampler : register(s0);

struct PixelInputType {
    float4 Position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

// WIP: Simple passthrough shader
float4 main(PixelInputType i) : SV_TARGET {
    return screenTexture.Sample(ClampSampler, i.uv);
}