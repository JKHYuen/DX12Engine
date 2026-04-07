
cbuffer VertexCB : register(b0, space0) {
    matrix SRT; /// TODO: might not need this
    matrix MVP;
};

struct VertexInput {
    float3 vertexPosition : POSITION;
    float3 normal         : NORMAL;
    float3 tangent        : TANGENT;
    float3 bitangent      : BITANGENT;
    float2 uv             : TEXCOORD0;
};

struct PixelInputType {
    float4 position : SV_POSITION;
};

PixelInputType main(VertexInput i) {
    PixelInputType o;

    o.position = mul(MVP, float4(i.vertexPosition, 1.0f));
    
    return o;
}