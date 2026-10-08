struct TonemapConstants
{
    uint hdrTextureIndex;
    float exposure;
};

[[vk::push_constant]] TonemapConstants constants;
[[vk::binding(0, 0)]] Sampler2D textures[];

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

// Exposure tonemapping: maps [0, inf) to [0, 1). Output stays linear, the sRGB attachment encodes it.
[shader("fragment")]
float4 main(VSOutput input)
{
    float3 hdr = textures[constants.hdrTextureIndex].Sample(input.UV).rgb;
    float3 mapped = 1.0 - exp(-hdr * constants.exposure);
    return float4(mapped, 1.0);
}
