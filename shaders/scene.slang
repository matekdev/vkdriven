static const float PI = 3.14159265359;

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
    float4 cameraPosition;
    float4 lightRadiance;
};

struct Material
{
    float4 baseColorFactor;
    uint baseColorTextureIndex;
    uint metallicRoughnessTextureIndex;
    float metallicFactor;
    float roughnessFactor;
    float alphaCutoff;
    uint padding0;
    uint padding1;
    uint padding2;
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
    float3 WorldPos;
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
    output.WorldPos = mul(transform.model, float4(input.Pos, 1.0)).xyz;
    output.Normal = mul((float3x3)transform.normal, input.Normal);
    output.UV = input.UV;
    return output;
}

static const float ambientStrength = 0.03;
static const float minRoughness = 0.04;

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// diffuse = kD * albedo / PI
float3 Diffuse(float3 F, float3 albedo, float metallic)
{
    float3 kD = (1.0 - F) * (1.0 - metallic);
    return kD * albedo / PI;
}

// specular = D * F * G / (4 * NdotV * NdotL)
float3 Specular(float3 N, float3 V, float3 L, float3 H, float3 F, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);

    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);

    return D * F * G / (4.0 * NdotV * NdotL + 0.0001);
}

[shader("fragment")]
float4 main(VSOutput input)
{
    Draw draw = constants.draws[constants.drawIndex];
    Material material = constants.materials[draw.materialIndex];
    float4 baseColor = material.baseColorFactor * textures[material.baseColorTextureIndex].Sample(input.UV);
    if (baseColor.a < material.alphaCutoff)
        discard;

    float3 albedo = baseColor.rgb;
    float4 metallicRoughness = textures[material.metallicRoughnessTextureIndex].Sample(input.UV);
    float metallic = material.metallicFactor * metallicRoughness.b;
    float roughness = max(material.roughnessFactor * metallicRoughness.g, minRoughness);

    float3 N = normalize(input.Normal);
    float3 L = normalize(constants.frame->directionToLight.xyz);
    float3 V = normalize(constants.frame->cameraPosition.xyz - input.WorldPos);
    float3 H = normalize(L + V);

    // radiance * NdotL
    float3 radiance = constants.frame->lightRadiance.rgb;
    float NdotL = max(dot(N, L), 0.0);

    // Shared by both terms: F is the light reflected as specular, and what's left over becomes diffuse.
    float3 F0 = lerp(float3(0.04), albedo, metallic);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    // BRDF equation
    // Lo = (diffuse  +  specular) * radiance * NdotL
    float3 diffuse = Diffuse(F, albedo, metallic);
    float3 specular = Specular(N, V, L, H, F, roughness);
    float3 Lo = (diffuse + specular) * radiance * NdotL;

    float3 ambient = ambientStrength * albedo;
    return float4(ambient + Lo, 1.0);
}
