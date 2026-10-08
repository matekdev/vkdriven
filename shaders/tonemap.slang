struct TonemapConstants
{
    uint hdrTextureIndex;
    float exposure;
    uint tonemapper;
};

[[vk::push_constant]] TonemapConstants constants;
[[vk::binding(0, 0)]] Sampler2D textures[];

// Must match the Tonemapper enum in tonemap_pass.h.
static const uint TONEMAPPER_EXPONENTIAL = 0;
static const uint TONEMAPPER_REINHARD = 1;
static const uint TONEMAPPER_ACES = 2;
static const uint TONEMAPPER_AGX = 3;
static const uint TONEMAPPER_PBR_NEUTRAL = 4;

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV;
};

// One triangle that covers the whole screen: UVs (0,0), (2,0), (0,2), clipped to the [0,1] square.
[shader("vertex")]
VSOutput main(uint vertexID : SV_VulkanVertexID)
{
    VSOutput output;
    output.UV = float2((vertexID << 1) & 2, vertexID & 2);
    output.Pos = float4(output.UV * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

float3 Exponential(float3 color)
{
    return 1.0 - exp(-color);
}

float3 Reinhard(float3 color)
{
    return color / (1.0 + color);
}

// Krzysztof Narkowicz's fit of the ACES filmic curve.
float3 Aces(float3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

// Benjamin Wrensch's minimal AgX: inset matrix, log2 encoding, sigmoid curve fit, outset matrix.
// The curve outputs display-encoded values, so the final pow(2.2) brings them back to linear for the sRGB attachment.
float3 AgxContrastApprox(float3 x)
{
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

float3 Agx(float3 color)
{
    const float3x3 inset = float3x3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,
                                    0.0784335999999992, 0.878468636469772, 0.0784336,
                                    0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    const float3x3 outset = float3x3(1.19687900512017, -0.0528968517574562, -0.0529716355144438,
                                     -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
                                     -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    const float minEv = -12.47393;
    const float maxEv = 4.026069;

    color = mul(color, inset);
    color = clamp(log2(max(color, 1e-10)), minEv, maxEv);
    color = (color - minEv) / (maxEv - minEv);
    color = AgxContrastApprox(color);
    color = mul(color, outset);
    return pow(max(color, 0.0), 2.2);
}

// Khronos PBR Neutral: leaves colors untouched below the compression point, then compresses and desaturates highlights.
float3 PbrNeutral(float3 color)
{
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression)
        return color;

    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return lerp(color, float3(newPeak), g);
}

// Maps [0, inf) to [0, 1). Output stays linear, the sRGB attachment encodes it.
[shader("fragment")]
float4 main(VSOutput input)
{
    float3 hdr = textures[constants.hdrTextureIndex].Sample(input.UV).rgb * constants.exposure;

    float3 mapped;
    switch (constants.tonemapper)
    {
    case TONEMAPPER_REINHARD:
        mapped = Reinhard(hdr);
        break;
    case TONEMAPPER_ACES:
        mapped = Aces(hdr);
        break;
    case TONEMAPPER_AGX:
        mapped = Agx(hdr);
        break;
    case TONEMAPPER_PBR_NEUTRAL:
        mapped = PbrNeutral(hdr);
        break;
    default:
        mapped = Exponential(hdr);
        break;
    }
    return float4(mapped, 1.0);
}
