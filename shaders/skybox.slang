import common;

static const float PI = 3.14159265359;

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 NDC;
};

// One triangle that covers the whole screen, placed on the far plane (depth 0 with reverse-Z),
// so the depth test only lets it through where the scene drew nothing.
[shader("vertex")]
VSOutput main(uint vertexID : SV_VulkanVertexID)
{
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    VSOutput output;
    output.NDC = uv * 2.0 - 1.0;
    output.Pos = float4(output.NDC, 0.0, 1.0);
    return output;
}

// Longitude around the Y axis becomes u, latitude from straight up (v = 0) to straight down (v = 1) becomes v.
float2 equirectUV(float3 direction)
{
    return float2(atan2(direction.z, direction.x) / (2.0 * PI) + 0.5, acos(clamp(direction.y, -1.0, 1.0)) / PI);
}

[shader("fragment")]
float4 main(VSOutput input)
{
    FrameData* frame = constants.frame;

    // Undo the projection to get the view-space ray through this pixel, then rotate it into world space.
    // The view matrix's rotation is orthonormal, so mul(v, M) (which multiplies by the transpose) inverts it.
    float3 viewDirection = float3(input.NDC.x / frame->projection[0][0], input.NDC.y / frame->projection[1][1], -1.0);
    float3 worldDirection = normalize(mul(viewDirection, (float3x3)frame->view));

    // Mip 0 explicitly: at the u = 0/1 seam the UV derivatives jump, which would otherwise pick a tiny mip
    // and draw a visible line down the sky.
    float3 radiance = textures[frame->environmentTextureIndex].SampleLevel(equirectUV(worldDirection), 0.0).rgb;
    return float4(radiance, 1.0);
}
