static const float3x3 RotateUV[6] = {
    float3x3(0, 0, 1,   0, -1, 0,   -1, 0, 0), // +X
    float3x3(0, 0, -1,  0, -1, 0,   1, 0, 0),  // -X
    float3x3(1, 0, 0,   0, 0, 1,    0, 1, 0),  // +Y
    float3x3(1, 0, 0,   0, 0, -1,   0, -1, 0), // -Y
    float3x3(1, 0, 0,   0, -1, 0,   0, 0, 1),  // +Z
    float3x3(-1, 0, 0,  0, -1, 0,   0, 0, -1)  // -Z
};

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
}