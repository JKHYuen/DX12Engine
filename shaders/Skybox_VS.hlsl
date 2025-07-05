cbuffer MatCB : register(b0) {
    matrix ViewProjectionMatrix;
};

struct VertexShaderInput {
    float3 Position : POSITION;
};

struct VertexShaderOutput {
    // 3d skybox coord
    float3 uv : TEXCOORD;
    float4 Position : SV_POSITION;
};

VertexShaderOutput main(VertexShaderInput IN) {
    VertexShaderOutput OUT;

    OUT.Position = mul(ViewProjectionMatrix, float4(IN.Position, 1.0f));
    OUT.uv = IN.Position;

    return OUT;
}
