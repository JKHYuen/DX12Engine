cbuffer VertexCB : register(b0) {
    matrix SRT;
    matrix MVP;
    float3 CameraPosition;
    float4 Pad1;
    float4 Pad2;
    float4 Pad3;
    matrix pad4;
};

struct VertexInput {
    float3 vertexPosition : POSITION;
    float3 normal         : NORMAL;
    float2 uv             : TEXCOORD0;
};

struct PixelInputType {
    float4 position       : SV_Position;
    float3 normal         : NORMAL;
    float2 uv             : TEXCOORD0;
    float4 worldPosition  : TEXCOORD1;
    float3 cameraPosition : TEXCOORD2;
};

PixelInputType main(VertexInput i) {
    PixelInputType o;
    
    float4 homogVertexPos = float4(i.vertexPosition, 1.0f);
    o.position = mul(MVP, homogVertexPos);
    o.normal = i.normal;
    o.uv = i.uv;
    o.worldPosition = mul(SRT, homogVertexPos);
    o.cameraPosition = CameraPosition;
    
    return o;
}