// Very simple one color render for masking (can't use depth, we need this in colo rbuffer to for bloom pass)
struct PixelInputType {
    float4 position : SV_POSITION;
};

float4 main() : SV_TARGET {
    // This needs to be white, outline is colored by multiplication after bloom pass
    return float4(1.0, 1.0, 1.0, 1.0);
}