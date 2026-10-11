import common;

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

[shader("fragment")]
float4 main(VSOutput input)
{
    FrameData* frame = constants.frame;

    // Undo the projection to get the view-space ray through this pixel, then rotate it into world space.
    // The view matrix's rotation is orthonormal, so mul(v, M) (which multiplies by the transpose) inverts it.
    float3 viewDirection = float3(input.NDC.x / frame->projection[0][0], input.NDC.y / frame->projection[1][1], -1.0);
    float3 worldDirection = normalize(mul(viewDirection, (float3x3)frame->view));

    uint cubeIndex = frame->showIrradiance != 0 ? frame->irradianceTextureIndex : frame->environmentTextureIndex;
    float3 radiance = cubeTextures[cubeIndex].SampleLevel(worldDirection, 0.0).rgb * frame->environmentIntensity;
    return float4(radiance, 1.0);
}
