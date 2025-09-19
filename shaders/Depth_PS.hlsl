struct PixelInputType {
    float4 position : SV_POSITION;
};

// Simple shader to render depth value 
float4 main(PixelInputType i) : SV_TARGET {
    return float4(i.position.z / i.position.w, 0.0f, 0.0f, 1.0f);
}