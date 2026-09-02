// Simple one color render

struct PixelInputType {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 main(PixelInputType i) : SV_TARGET {
    return i.color;
}