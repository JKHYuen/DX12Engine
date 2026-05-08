struct VertexInputType {
    float4 position  : POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv        : TEXCOORD0;
};

struct HullInputType {
    float4 position  : POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv        : TEXCOORD0;
};

HullInputType main(VertexInputType i) {
    HullInputType o;
    o.position  = i.position;
    o.uv        = i.uv;
    o.normal    = i.normal;
    o.tangent   = i.tangent;
    o.bitangent = i.bitangent;
    return o;
}