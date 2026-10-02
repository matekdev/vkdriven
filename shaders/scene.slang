struct VSInput
{
    float3 Pos;
    float3 Normal;
    float2 UV;
};

struct FrameData
{
    float4x4 view;
    float4x4 projection;
    float4 directionToLight;
};

struct DrawConstants
{
    float4x4 model;
    float4 baseColorFactor;
    FrameData* frame;
};

[[vk::push_constant]] DrawConstants constants;

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 UV;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    float4x4 model = constants.model;
    float4x4 view = constants.frame->view;
    float4x4 projection = constants.frame->projection;

    // Column vectors, so read right to left: model -> world -> view -> clip.
    float4x4 mvp = mul(projection, mul(view, model));

    VSOutput output;
    output.Pos = mul(mvp, float4(input.Pos, 1.0));
    output.Normal = mul((float3x3)model, input.Normal);
    output.UV = input.UV;
    return output;
}

static const float3 lightColor = float3(1.0, 1.0, 1.0);
static const float ambientStrength = 0.1;

[shader("fragment")]
float4 main(VSOutput input)
{
    float3 ambient = ambientStrength * lightColor;

    float3 normal = normalize(input.Normal);
    float3 directionToLight = constants.frame->directionToLight.xyz;
    float3 diffuse = max(dot(normal, directionToLight), 0.0) * lightColor;

    float3 baseColor = constants.baseColorFactor.rgb;
    return float4((ambient + diffuse) * baseColor, 1.0);
}
