module cube;

// The direction through a point on a cube face, with uv in [-1, 1] and v pointing down the face.
// Faces are in the order Vulkan stores them: +X, -X, +Y, -Y, +Z, -Z.
public float3 cubeDirection(uint face, float2 uv)
{
    switch (face)
    {
    case 0:
        return float3(1.0, -uv.y, -uv.x);
    case 1:
        return float3(-1.0, -uv.y, uv.x);
    case 2:
        return float3(uv.x, 1.0, uv.y);
    case 3:
        return float3(uv.x, -1.0, -uv.y);
    case 4:
        return float3(uv.x, -uv.y, 1.0);
    default:
        return float3(-uv.x, -uv.y, -1.0);
    }
}

// The direction through the center of texel `texel` on face `face` of a size × size cube.
public float3 cubeTexelDirection(uint3 texel, uint size)
{
    float2 uv = (float2(texel.xy) + 0.5) / float(size) * 2.0 - 1.0;
    return normalize(cubeDirection(texel.z, uv));
}
