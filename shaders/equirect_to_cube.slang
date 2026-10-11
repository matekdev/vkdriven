import cube;

static const float PI = 3.14159265359;

[[vk::binding(0, 0)]] Sampler2D equirect;
[[vk::binding(1, 0)]] [[vk::image_format("rgba16f")]] RWTexture2DArray<float4> environment;

// Longitude around the Y axis becomes u, latitude from straight up (v = 0) to straight down (v = 1) becomes v.
float2 equirectUV(float3 direction)
{
    return float2(atan2(direction.z, direction.x) / (2.0 * PI) + 0.5, acos(clamp(direction.y, -1.0, 1.0)) / PI);
}

// One thread per cube texel: x and y pick the texel, z picks the face.
[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint size, height, faceCount;
    environment.GetDimensions(size, height, faceCount);
    if (dispatchThreadID.x >= size || dispatchThreadID.y >= size)
        return;

    float3 direction = cubeTexelDirection(dispatchThreadID, size);
    environment[dispatchThreadID] = float4(equirect.SampleLevel(equirectUV(direction), 0.0).rgb, 1.0);
}
