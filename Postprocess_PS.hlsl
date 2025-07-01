Texture2D screenTexture : register(t0);
SamplerState ClampSampler : register(s0);

// WIP: Simple passthrough shader
float4 main(float2 uv : TEXCOORD) : SV_Target0 {
    return screenTexture.Sample(ClampSampler, uv);
}