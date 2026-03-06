
cbuffer MaterialCB : register(b0, space1) {
    float4 outlineColor;
};

struct PixelInputType {
    float4 position : SV_POSITION;
};

float4 main() : SV_TARGET {
    return outlineColor;
}