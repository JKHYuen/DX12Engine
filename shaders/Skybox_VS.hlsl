cbuffer MatCB : register(b0) {
    matrix ViewProjectionMatrix;
};

struct VertexShaderInput {
    float3 Position : POSITION;
};

struct VertexShaderOutput {
    float4 Position : SV_POSITION;
    // 3d skybox coord
    float3 uvw : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput i) {
    VertexShaderOutput o;

    o.Position = mul(ViewProjectionMatrix, float4(i.Position, 1.0f)).xyww;
    o.uvw = i.Position;

    return o;
}
