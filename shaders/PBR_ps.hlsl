cbuffer MaterialCB : register(b0, space1) {
    float4 Time;
    float4 DirLight;
    float4 Pad1;
    float4 Pad2;
    matrix Pad3;
    matrix Pad4;
    matrix Pad5;
};

Texture2D AlbedoTex : register(t0);
Texture2D NormalTex : register(t1);

SamplerState AnisoSampler : register(s0);

struct PixelInputType {
    float4 position       : SV_Position;
    float3 normal         : NORMAL;
    float3 tangent        : TANGENT;
    float3 bitangent      : BITANGENT;
    float2 uv             : TEXCOORD0;
    float4 worldPosition  : TEXCOORD1;
    float3 cameraPosition : TEXCOORD2;
};

float4 main(PixelInputType i) : SV_Target {
    i.uv *= 2;
    
    float3 normalMap = NormalTex.Sample(AnisoSampler, i.uv).xyz * 2.0 - 1.0;
    float3 normal = normalize((normalMap.x * i.tangent) + (normalMap.y * i.bitangent) + (normalMap.z * i.normal));
   
    // Diffuse
    float3 L = -DirLight.xyz;
    float diffuseMag = max(dot(normal, L), 0.0);
    float3 diffuse = diffuseMag * float3(1, 1, 1);
    
    // Specular
    float3 viewDir = normalize(i.cameraPosition - i.worldPosition.xyz);
    float3 H = normalize(viewDir + L);
    
    float specMag = pow(max(dot(normal, H), 0.0), 64);
    float3 specular = specMag * float3(1, 1, 1);
    
    // Color
    float4 albedo = AlbedoTex.Sample(AnisoSampler, i.uv);
    
    //float4 albedo = {0.7, 0.4, 0.3, 1};
   
    float3 ambient = { 0.1, 0.1, 0.1 };
    
    return float4(albedo.rgb * (ambient + diffuse + specular), 1.0);

}