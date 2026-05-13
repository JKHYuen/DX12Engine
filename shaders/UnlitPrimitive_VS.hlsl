cbuffer VertexCB : register(b0, space0) {
    matrix SRT;
    matrix MVP;
    float4 cameraPosition;
    matrix directionalLightMVP;
    float2 uvScale;
    float heightMapMagnitude;
    float pad1;
    float4 color;
};

struct VertexInputType {
    float4 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv : TEXCOORD0;
};

struct PixelInputType {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

PixelInputType main(VertexInputType i) {
    PixelInputType o;
    
    float4 hVertexPos = float4(i.position.xyz, 1.0);
    o.position = mul(MVP, hVertexPos);
    
    o.color = color;
    
    return o;
}