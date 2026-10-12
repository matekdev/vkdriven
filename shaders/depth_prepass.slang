import common;

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float4 CurrentClip;
    float4 PreviousClip;
    float3 ViewNormal;
    float2 UV;
};

struct PSOutput
{
    float4 Normal : SV_Target0;
    float2 Motion : SV_Target1;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    Draw draw = constants.draws[constants.drawIndex];
    Transform transform = constants.transforms[draw.transformIndex];
    float4 worldPos = mul(transform.model, float4(input.Pos, 1.0));

    VSOutput output;
    output.Pos = ClipPosition(input.Pos, transform);
    output.CurrentClip = output.Pos;
    // The scene doesn't animate, so only the camera moves the point: same world position, last frame's camera.
    output.PreviousClip = mul(constants.frame->previousViewProjection, worldPos);
    float3 worldNormal = mul((float3x3)transform.normal, input.Normal);
    output.ViewNormal = mul((float3x3)constants.frame->view, worldNormal);
    output.UV = input.UV;
    return output;
}

// Alpha-masked surfaces cut the same holes here as in the scene pass, or the EQUAL test there would fail
// on the leaves and the holes would hide whatever is behind them.
[shader("fragment")]
PSOutput main(VSOutput input)
{
    Draw draw = constants.draws[constants.drawIndex];
    Material material = constants.materials[draw.materialIndex];
    float alpha = material.baseColorFactor.a * textures[material.baseColorTextureIndex].Sample(input.UV).a;
    if (alpha < material.alphaCutoff)
        discard;

    // The divide has to happen per pixel, not per vertex, so the clip positions are interpolated undivided.
    float2 currentNdc = input.CurrentClip.xy / input.CurrentClip.w;
    float2 previousNdc = input.PreviousClip.xy / input.PreviousClip.w;

    PSOutput output;
    output.Normal = float4(normalize(input.ViewNormal), 0.0);
    // In UV units: where this pixel was last frame is uv - motion.
    output.Motion = (currentNdc - previousNdc) * 0.5;
    return output;
}
