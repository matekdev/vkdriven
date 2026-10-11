import cube;

static const float PI = 3.14159265359;
// Angle between hemisphere samples, in radians: about 250 × 63 samples per texel.
static const float sampleDelta = 0.025;

[[vk::binding(0, 0)]] SamplerCube environment;
[[vk::binding(1, 0)]] [[vk::image_format("rgba16f")]] RWTexture2DArray<float4> irradiance;

// For each texel's direction N, sums the light arriving from the whole hemisphere around N, weighted by
// cos(theta): how much a diffuse surface facing N receives (LearnOpenGL, PBR -> IBL -> Diffuse irradiance).
[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint size, height, faceCount;
    irradiance.GetDimensions(size, height, faceCount);
    if (dispatchThreadID.x >= size || dispatchThreadID.y >= size)
        return;

    float3 normal = cubeTexelDirection(dispatchThreadID, size);

    // A tangent frame around the normal, so hemisphere samples can be written as angles relative to it.
    float3 up = abs(normal.y) < 0.999 ? float3(0.0, 1.0, 0.0) : float3(0.0, 0.0, 1.0);
    float3 right = normalize(cross(up, normal));
    up = cross(normal, right);

    // Each sample stands for about sampleDelta² steradians. Reading the mip whose texels are about that size
    // averages small bright spots (the sun) in, instead of each sample either hitting or missing them.
    uint environmentSize, environmentHeight, mipCount;
    environment.GetDimensions(0, environmentSize, environmentHeight, mipCount);
    float texelSolidAngle = 4.0 * PI / (6.0 * float(environmentSize * environmentSize));
    float lod = 0.5 * log2(sampleDelta * sampleDelta / texelSolidAngle);

    float3 sum = 0.0;
    uint sampleCount = 0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 direction = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
            // cos(theta): light from grazing angles spreads over more surface. sin(theta): rings of samples near
            // the pole are smaller, so they count for less.
            sum += environment.SampleLevel(direction, lod).rgb * cos(theta) * sin(theta);
            sampleCount++;
        }
    }
    irradiance[dispatchThreadID] = float4(PI * sum / float(sampleCount), 1.0);
}
