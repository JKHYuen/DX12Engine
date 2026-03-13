cbuffer VertexCB : register(b0, space0) {
    matrix SRT;
    matrix MVP;
    matrix directionalLightMVP;
    float4 CameraPosition;
};

struct VertexInput {
    float3 vertexPosition : POSITION;
    float3 normal         : NORMAL;
    float3 tangent        : TANGENT;
    float3 bitangent      : BITANGENT;
    float2 uv             : TEXCOORD0;
};

struct PixelInputType {
    float4 position                     : SV_POSITION;
    float3 tangent                      : TANGENT;
    float3 bitangent                    : BITANGENT;
    float3 normal                       : NORMAL;
    float2 uv                           : TEXCOORD0;
    float4 worldPosition                : TEXCOORD1;
    float4 cameraPosition               : TEXCOORD2;
    float4 directionalLightViewPosition : TEXCOORD3;
};

PixelInputType main(VertexInput i) {
    PixelInputType o;
    
     // TBN
    o.tangent =   normalize(mul((float3x3) SRT, i.tangent));
    o.bitangent = normalize(mul((float3x3) SRT, i.bitangent));
    o.normal =    normalize(mul((float3x3) SRT, i.normal));
    
    // For parallax mapping only
    //float3x3 TBN = float3x3(o.tangent, o.bitangent, o.normal);
    //o.tangentViewDirection = mul(TBN, cameraPosition - o.worldPosition.xyz);
    
    float4 hVertexPos = float4(i.vertexPosition, 1.0f);
    o.position = mul(MVP, hVertexPos);
    o.uv = i.uv;
    o.worldPosition = mul(SRT, hVertexPos);
    o.cameraPosition = CameraPosition;
    
    o.directionalLightViewPosition = mul(directionalLightMVP, hVertexPos);
    
    return o;
}