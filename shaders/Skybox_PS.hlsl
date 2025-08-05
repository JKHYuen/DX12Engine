struct PixelShaderInput {
    float4 Position : SV_POSITION;
    // 3d skybox coord
    float3 uvw : TEXCOORD0;
};

TextureCube<float4> SkyboxTexture : register(t0);
SamplerState LinearClampSampler : register(s0);

float4 main(PixelShaderInput IN) : SV_Target {
    return SkyboxTexture.Sample(LinearClampSampler, IN.uvw);
}