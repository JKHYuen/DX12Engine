cbuffer MaterialCB : register(b0, space1) {
    float4 Time;
    float4 DirLight;
    float4 Pad1;
    float4 Pad2;
    matrix Pad3;
    matrix Pad4;
    matrix Pad5;
};

struct PixelInputType {
    float4 position       : SV_Position;
    float3 normal         : NORMAL;
    float2 uv             : TEXCOORD0;
    float4 worldPosition  : TEXCOORD1;
    float3 cameraPosition : TEXCOORD2;
};

float4 main(PixelInputType i) : SV_Target {
    float3 viewDirection = normalize(i.cameraPosition - i.worldPosition.xyz);
    float3 L = -DirLight;
    //float3 H = normalize(viewDirection + L);
    
    float diffuse = max(dot(normalize(i.normal), L), 0.0);
    
    float4 albedo = float4(1, 1, 1, 1);
    float4 ambient = float4(0.1, 0.1, 0.1, 1);

    return albedo * (ambient + diffuse);

}