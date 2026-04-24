cbuffer VertexCB : register(b0, space0) {
    matrix SRT;
    matrix MVP;
    matrix directionalLightMVP;
    float4 cameraPosition;
	float2 uvScale;
    float  heightMapMagnitude;
};

/// Register indices based on pixel shader
Texture2D MaterialTex              : register(t2);
SamplerState TrilinearClampSampler : register(s1); // for BRDF lut and directional shadow map
///

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
    float3 normal                       : NORMAL0;
    float2 uv                           : TEXCOORD0;
    float4 worldPosition                : TEXCOORD1;
    float4 cameraPosition               : TEXCOORD2;
    float4 directionalLightViewPosition : TEXCOORD3; // vertex position with directional light's view/projection
    float3 tangentViewDirection         : TEXCOORD4;
};

PixelInputType main(VertexInput i) {
    PixelInputType o;
    
    // uv scale
    o.uv = i.uv * uvScale;
    
    if (heightMapMagnitude != 0) {
        float displacement = MaterialTex.SampleLevel(TrilinearClampSampler, o.uv, 0).a;
        // should substract "displacement" by 0.5 so vertices can be displaced both directions
        // omitted this here to be consistent with parallax occulsion mapping
        i.vertexPosition.xyz += i.normal * displacement * heightMapMagnitude;
    }
    
    float4 hVertexPos = float4(i.vertexPosition, 1.0f);
    o.position = mul(MVP, hVertexPos);
    o.worldPosition = mul(SRT, hVertexPos);
    o.cameraPosition = cameraPosition;
    
    // TBN
    o.tangent = normalize(mul((float3x3) SRT, i.tangent));
    o.bitangent = normalize(mul((float3x3) SRT, i.bitangent));
    o.normal = normalize(mul((float3x3) SRT, i.normal));
    
    // For parallax mapping only
    float3x3 TBN = float3x3(o.tangent, o.bitangent, o.normal);
    o.tangentViewDirection = mul(TBN, cameraPosition.xyz - o.worldPosition.xyz);
    
    o.directionalLightViewPosition = mul(directionalLightMVP, hVertexPos);
    
    return o;
}