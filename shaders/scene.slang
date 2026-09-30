struct VSInput
{
    float3 Pos;
    float3 Normal;
    float2 UV;
};

struct FrameData
{
    float4x4 viewProjection;
};

// Must match DrawConstants in app.h.
struct DrawConstants
{
    FrameData* frame;
    float4x4* transforms;
    uint32_t transformIndex;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 UV;
};

[shader("vertex")]
VSOutput main(VSInput input, uniform DrawConstants constants)
{
    float4x4 model = constants.transforms[constants.transformIndex];
    VSOutput output;
    output.Pos = mul(constants.frame->viewProjection, mul(model, float4(input.Pos, 1.0)));
    output.Normal = mul((float3x3)model, input.Normal);
    output.UV = input.UV;
    return output;
}

[shader("fragment")]
float4 main(VSOutput input)
{
    return float4(normalize(input.Normal) * 0.5 + 0.5, 1.0);
}
