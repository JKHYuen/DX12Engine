Texture2D MaterialTex              : register(t2); // alpha channel is height map
SamplerState TrilinearClampSampler : register(s1); // for BRDF lut and directional shadow map

cbuffer VertexCB : register(b0, space0) {
    matrix SRT;
    matrix MVP;
	float4 cameraPosition;
    matrix directionalLightMVP;
    float2 uvScale;
    float  heightMapMagnitude;
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

struct ConstantOutputType {
    float edges[3] : SV_TessFactor;
    float inside   : SV_InsideTessFactor;
};

struct DomainInputType {
    float4 position       : POSITION;
    float3 normal         : NORMAL;
    float3 tangent        : TANGENT;
    float3 bitangent      : BITANGENT;
    float2 uv             : TEXCOORD0;
};

#define BARYCENTRIC_INTERPOLATE(fieldName) o.fieldName = \
	patch[0].fieldName * uvwCoord.x + \
	patch[1].fieldName * uvwCoord.y + \
	patch[2].fieldName * uvwCoord.z;

#define NUM_CONTROL_POINTS 3

[domain("tri")]
PixelInputType main(
    ConstantOutputType tessConstantOutput,
    float3 uvwCoord : SV_DomainLocation,
    const OutputPatch<DomainInputType, NUM_CONTROL_POINTS> patch) 
{
    PixelInputType o;
    
    // Patch Interpolation
    float4 vertexPosition = BARYCENTRIC_INTERPOLATE(position); vertexPosition.w = 1.0f;
    float2 uv             = BARYCENTRIC_INTERPOLATE(uv);
    float3 normal         = BARYCENTRIC_INTERPOLATE(normal); normal = normalize(normal);
    float3 tangent        = BARYCENTRIC_INTERPOLATE(tangent);
    float3 bitangent      = BARYCENTRIC_INTERPOLATE(bitangent);
    
    o.uv = uv * uvScale;
    
    // Vertex displacement
    if (heightMapMagnitude != 0) {
        float displacement = MaterialTex.SampleLevel(TrilinearClampSampler, o.uv, 0).a;
        // should substract "displacement" by 0.5 so vertices can be displaced both directions
        // omitted this here to be consistent with parallax occulsion mapping
        vertexPosition.xyz += normal * displacement * heightMapMagnitude;
    }
    
    o.position       = mul(MVP, vertexPosition);
    o.worldPosition  = mul(SRT, vertexPosition);
    o.cameraPosition = cameraPosition;
    
    // TBN
    o.tangent   = normalize(mul((float3x3) SRT, tangent));
    o.bitangent = normalize(mul((float3x3) SRT, bitangent));
    o.normal    = normalize(mul((float3x3) SRT, normal));
    
    // For parallax mapping only
    float3x3 TBN = float3x3(o.tangent, o.bitangent, o.normal);
    o.tangentViewDirection = normalize(mul(TBN, cameraPosition.xyz - o.worldPosition.xyz));
    
    o.directionalLightViewPosition = mul(directionalLightMVP, vertexPosition);
    
    return o;
}