import common;

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV;
    nointerpolation uint DrawIndex;
};

// Every draw has one instance whose firstInstance is the draw's index, so the instance index is the draw index.
[shader("vertex")]
VSOutput main(VSInput input, uint drawIndex: SV_VulkanInstanceID)
{
    Draw draw = constants.draws[drawIndex];
    Transform transform = constants.transforms[draw.transformIndex];

    VSOutput output;
    float4x4 lightViewProjection = constants.frame->lightViewProjections[constants.cascadeIndex];
    output.Pos = mul(lightViewProjection, mul(transform.model, float4(input.Pos, 1.0)));
    output.UV = input.UV;
    output.DrawIndex = drawIndex;
    return output;
}

// Alpha-masked surfaces (Sponza's plants and chains) cut the same holes into the shadow map as into the scene.
[shader("fragment")]
void main(VSOutput input)
{
    Draw draw = constants.draws[input.DrawIndex];
    Material material = constants.materials[draw.materialIndex];
    float alpha = material.baseColorFactor.a * textures[material.baseColorTextureIndex].Sample(input.UV).a;
    if (alpha < material.alphaCutoff)
        discard;
}
