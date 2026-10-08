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
    public float4x4 lightViewProjection;
    public uint shadowMapTextureIndex;
    public uint padding[3];
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
    public uint drawIndex;
};

[[vk::push_constant]] public DrawConstants constants;

[[vk::binding(0, 0)]] public Sampler2D textures[];
