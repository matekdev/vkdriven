import common;

// Laid out like VkDrawIndexedIndirectCommand. firstInstance carries the draw's index, so the scene shaders
// can still find their Draw once there's no per-draw push constant.
struct DrawCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

struct CullConstants
{
    FrameData* frame;
    Draw* draws;
    Transform* transforms;
    Primitive* primitives;
    DrawCommand* commands;
    uint* commandCount;
    uint drawCount;
};

[[vk::push_constant]] CullConstants cull;

// Scaled by the largest axis scale, so the sphere still contains the mesh under non-uniform scale.
BoundingSphere WorldBounds(BoundingSphere bounds, float4x4 model)
{
    float3 axisScales = float3(length(mul(model, float4(1.0, 0.0, 0.0, 0.0)).xyz),
                               length(mul(model, float4(0.0, 1.0, 0.0, 0.0)).xyz),
                               length(mul(model, float4(0.0, 0.0, 1.0, 0.0)).xyz));
    BoundingSphere world;
    world.center = mul(model, float4(bounds.center, 1.0)).xyz;
    world.radius = bounds.radius * max(axisScales.x, max(axisScales.y, axisScales.z));
    return world;
}

// Only rejects spheres entirely outside a single plane. A sphere near a frustum corner can be outside two
// planes at once without being fully outside either, and passes; that only costs a wasted draw.
bool IsInFrustum(BoundingSphere sphere)
{
    for (uint plane = 0; plane < 6; plane++)
    {
        float4 frustumPlane = cull.frame->frustumPlanes[plane];
        if (dot(frustumPlane.xyz, sphere.center) + frustumPlane.w < -sphere.radius)
            return false;
    }
    return true;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 threadId: SV_DispatchThreadID)
{
    uint drawIndex = threadId.x;
    if (drawIndex >= cull.drawCount)
        return;

    Draw draw = cull.draws[drawIndex];
    Primitive primitive = cull.primitives[draw.primitiveIndex];
    BoundingSphere sphere = WorldBounds(primitive.bounds, cull.transforms[draw.transformIndex].model);
    if (!IsInFrustum(sphere))
        return;

    uint slot;
    InterlockedAdd(*cull.commandCount, 1, slot);

    DrawCommand command;
    command.indexCount = primitive.indexCount;
    command.instanceCount = 1;
    command.firstIndex = primitive.firstIndex;
    command.vertexOffset = primitive.vertexOffset;
    command.firstInstance = drawIndex;
    cull.commands[slot] = command;
}
