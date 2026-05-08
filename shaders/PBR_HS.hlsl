
#define TESS_MODE_UNIFORM /// TODO: TEMP
#define NUM_CONTROL_POINTS 3

cbuffer TessellationCB : register(b2) {
    float4 cameraPosition;
    matrix SRT;
    float4 cullingPlanes[4];
    float  cullBias;
    float2 screenDimensions;
    float  tessellationMagnitude;
};

struct HullInputType {
    float4 position  : POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv        : TEXCOORD0;
};

struct ConstantOutputType {
    float edges[3] : SV_TessFactor;
    float inside   : SV_InsideTessFactor;
};

struct DomainInputType {
    float4 position  : POSITION;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
    float2 uv        : TEXCOORD0;
};

#if defined(TESS_MODE_EDGE)
// Edge tessellation based on: https://catlikecoding.com/unity/tutorials/advanced-rendering/tessellation/
// Note: Distance based tessellation can be improved by adding min and max distance with interpolation between these values
float CalcTessellationFactor(float3 vertexPosition1, float3 vertexPosition2) {
    /// TODO: check if this is right, removes one sqrt
    //float3 viewDistVec = cameraPosition - ((vertexPosition1 + vertexPosition2) * 0.5);
    //float viewDistSquard = dot(viewDistVec, viewDistVec);
    //float3 edgeLengthVec = vertexPosition1 - vertexPosition2;
    //float edgeLengthSquard = dot(edgeLengthVec, edgeLengthVec);
    //return sqrt(edgeLengthSquard * screenDimensions.y) / (viewDistSquard * tessellationMagnitude);
    
    float viewDistance = distance(cameraPosition.xyz, (vertexPosition1 + vertexPosition2) * 0.5);
    float edgeLength   = distance(vertexPosition1, vertexPosition2);
    return (edgeLength * screenDimensions.y) / (viewDistance * tessellationMagnitude);
}
#endif

//bool TriangleIsBelowClipPlane(float3 p0, float3 p1, float3 p2, int cullPlaneIndex) {
//    float4 plane = cullingPlanes[cullPlaneIndex];
//    return
//		dot(float4(p0, 1), plane) < -cullBias &&
//		dot(float4(p1, 1), plane) < -cullBias &&
//		dot(float4(p2, 1), plane) < -cullBias;
//}

// TODO: add bias (vertex displacement scale)
//bool TriangleIsCulled(float3 p0, float3 p1, float3 p2) {
//    return
//		TriangleIsBelowClipPlane(p0, p1, p2, 0) ||
//		TriangleIsBelowClipPlane(p0, p1, p2, 1) ||
//		TriangleIsBelowClipPlane(p0, p1, p2, 2) ||
//		TriangleIsBelowClipPlane(p0, p1, p2, 3);
//}

ConstantOutputType PBRPatchConstantFunction(InputPatch<HullInputType, NUM_CONTROL_POINTS> inputPatch, uint patchId : SV_PrimitiveID) {
    ConstantOutputType output;
    /// TODO: check matrix multiply order
    //float3 vertexPosition0 = mul(SRT, float4(inputPatch[0].position.xyz, 1.0)).xyz;
    //float3 vertexPosition1 = mul(SRT, float4(inputPatch[1].position.xyz, 1.0)).xyz;
    //float3 vertexPosition2 = mul(SRT, float4(inputPatch[2].position.xyz, 1.0)).xyz;
    
    // TODO: Triangle Frustum culling - might only be worth it for very big objects like terrain (we also have object culling)
    //if (TriangleIsCulled(vertexPosition0, vertexPosition1, vertexPosition2)) {
    //    output.edges[0] = output.edges[1] = output.edges[2] = output.inside = 0;
    //    return output;
    //}
    
#if defined(TESS_MODE_UNIFORM)
    output.edges[0] = output.edges[1] = output.edges[2] = output.inside = tessellationMagnitude;
    
#elif defined(TESS_MODE_EDGE)
    output.edges[0] = CalcTessellationFactor(vertexPosition1, vertexPosition2);
    output.edges[1] = CalcTessellationFactor(vertexPosition2, vertexPosition0);
    output.edges[2] = CalcTessellationFactor(vertexPosition0, vertexPosition1);
    output.inside   = (output.edges[0] + output.edges[1] + output.edges[2]) / 3.0;
    
#else // No tessellation
    output.edges[0] = output.edges[1] = output.edges[2] = output.inside = 1.0f;
#endif
    
    return output;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("PBRPatchConstantFunction")]
DomainInputType main(
    InputPatch<HullInputType, NUM_CONTROL_POINTS> patch,
    uint pointId : SV_OutputControlPointID,
    uint patchId : SV_PrimitiveID) 
{
    DomainInputType o;

    o.position  = patch[pointId].position;
    o.normal    = patch[pointId].normal;
    o.tangent   = patch[pointId].tangent;
    o.bitangent = patch[pointId].bitangent;
    o.uv        = patch[pointId].uv;
    
    return o;
}
