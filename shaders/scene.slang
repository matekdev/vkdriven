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

struct Material
{
    float4 baseColorFactor;
    uint baseColorTextureIndex;
    float alphaCutoff;
    uint padding0;
    uint padding1;
};

struct Draw
{
    uint primitiveIndex;
    uint materialIndex;
    uint transformIndex;
};

struct Transform
{
    float4x4 model;
    float4x4 normal;
};

struct DrawConstants
{
    FrameData* frame;
    Draw* draws;
    Transform* transforms;
    Material* materials;
    uint drawIndex;
};

[[vk::push_constant]] DrawConstants constants;

[[vk::binding(0, 0)]] Sampler2D textures[];

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float3 Normal;
    float2 UV;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    Draw draw = constants.draws[constants.drawIndex];
    Transform transform = constants.transforms[draw.transformIndex];
    float4x4 view = constants.frame->view;
    float4x4 projection = constants.frame->projection;

    // Column vectors, so read right to left: model -> world -> view -> clip.
    float4x4 mvp = mul(projection, mul(view, transform.model));

    VSOutput output;
    output.Pos = mul(mvp, float4(input.Pos, 1.0));
    output.Normal = mul((float3x3)transform.normal, input.Normal);
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

    Draw draw = constants.draws[constants.drawIndex];
    Material material = constants.materials[draw.materialIndex];
    float4 baseColor = material.baseColorFactor * textures[material.baseColorTextureIndex].Sample(input.UV);
    if (baseColor.a < material.alphaCutoff)
        discard;
    return float4((ambient + diffuse) * baseColor.rgb, 1.0);
}
