import common;

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float2 UV;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    Draw draw = constants.draws[constants.drawIndex];
    Transform transform = constants.transforms[draw.transformIndex];

    VSOutput output;
    output.Pos = mul(constants.frame->lightViewProjection, mul(transform.model, float4(input.Pos, 1.0)));
    output.UV = input.UV;
    return output;
}

// Alpha-masked surfaces (Sponza's plants and chains) cut the same holes into the shadow map as into the scene.
[shader("fragment")]
void main(VSOutput input)
{
    Draw draw = constants.draws[constants.drawIndex];
    Material material = constants.materials[draw.materialIndex];
    float alpha = material.baseColorFactor.a * textures[material.baseColorTextureIndex].Sample(input.UV).a;
    if (alpha < material.alphaCutoff)
        discard;
}
