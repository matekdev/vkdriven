module common;

public struct VSInput
{
    public float3 Pos;
    public float3 Normal;
    public float2 UV;
    public float4 Tangent;
};

public struct FrameData
{
    public float4x4 view;
    public float4x4 projection;
    public float4 directionToLight;
    public float4 cameraPosition;
    public float4 lightRadiance;
    public float4x4 lightViewProjections[4];
    public float4 cascadeSplitDepths;
    public float4 cascadeTexelSizes;
    public uint shadowMapTextureIndex;
    public float normalOffset;
    public uint showCascades;
    public uint environmentTextureIndex;
    // World space, normals pointing inwards: p is on the inside when dot(plane.xyz, p) + plane.w >= 0.
    public float4 frustumPlanes[6];
};

public struct Material
{
    public float4 baseColorFactor;
    public uint baseColorTextureIndex;
    public uint metallicRoughnessTextureIndex;
    public float metallicFactor;
    public float roughnessFactor;
    public float alphaCutoff;
    public uint normalTextureIndex;
    public float normalScale;
    public uint padding;
};

public struct BoundingSphere
{
    public float3 center;
    public float radius;
};

public struct Primitive
{
    public BoundingSphere bounds;
    public uint firstIndex;
    public uint indexCount;
    public int vertexOffset;
    public uint materialIndex;
};

public struct Draw
{
    public uint primitiveIndex;
    public uint materialIndex;
    public uint transformIndex;
};

public struct Transform
{
    public float4x4 model;
    public float4x4 normal;
};

public struct DrawConstants
{
    public FrameData* frame;
    public Draw* draws;
    public Transform* transforms;
    public Material* materials;
    public uint cascadeIndex;
};

[[vk::push_constant]] public DrawConstants constants;

[[vk::binding(0, 0)]] public Sampler2D textures[];
