struct Mat {
    matrix Model;
    matrix ModelView;
    matrix InverseTransposeModelView;
    matrix MVP;
};

ConstantBuffer<Mat> MatCB : register(b0);

struct VertexInput {
    float3 Position : POSITION;
    //float3 Color : COLOR;
};

struct VertexShaderOutput {
    //float4 Color    : COLOR;
    float4 Position : SV_Position;
};

VertexShaderOutput main(VertexInput IN) {
    VertexShaderOutput OUT;
    OUT.Position = mul(MatCB.MVP, float4(IN.Position, 1.0f));
    //OUT.Color = float4(IN.Color, 1.0f);
    return OUT;
}