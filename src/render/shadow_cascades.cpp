#include "render/shadow_cascades.h"

#include <glm/gtc/matrix_transform.hpp>

#include "scene/camera.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{

// Casters between a slice and the light (a pillar shading the floor in front of the camera) can sit well outside
// the slice itself. Each light box reaches this far back toward the light so they still land in the map.
// Sized to cover Sponza.
constexpr float casterDistance = 50.0f;

// Practical split scheme: logarithmic splits match how perspective shrinks texels with distance, but crowd
// everything near the camera, so they're blended with uniform splits.
std::array<float, ShadowPass::cascadeCount> computeSplitDepths(float nearPlane, float farPlane, float lambda)
{
    std::array<float, ShadowPass::cascadeCount> splits{};
    for (uint32_t cascade = 0; cascade < ShadowPass::cascadeCount; cascade++)
    {
        const float fraction = static_cast<float>(cascade + 1) / static_cast<float>(ShadowPass::cascadeCount);
        const float logarithmic = nearPlane * std::pow(farPlane / nearPlane, fraction);
        const float uniform = nearPlane + (farPlane - nearPlane) * fraction;
        splits[cascade] = std::lerp(uniform, logarithmic, lambda);
    }
    return splits;
}

std::array<glm::vec3, 8> frustumCorners(const glm::mat4& viewProjection)
{
    const glm::mat4 inverse = glm::inverse(viewProjection);
    std::array<glm::vec3, 8> corners{};
    size_t i = 0;
    for (const float x : {-1.0f, 1.0f})
    {
        for (const float y : {-1.0f, 1.0f})
        {
            for (const float z : {0.0f, 1.0f})
            {
                const glm::vec4 corner = inverse * glm::vec4{x, y, z, 1.0f};
                corners[i++] = glm::vec3{corner} / corner.w;
            }
        }
    }
    return corners;
}

} // namespace

ShadowCascades computeShadowCascades(const Camera& camera, float aspect, glm::vec3 directionToLight,
                                     const ShadowSettings& settings)
{
    const std::array<float, ShadowPass::cascadeCount> splits =
        computeSplitDepths(camera.nearPlane(), settings.distance, settings.splitLambda);
    const glm::vec3 up =
        std::abs(directionToLight.y) > 0.99f ? glm::vec3{0.0f, 0.0f, 1.0f} : glm::vec3{0.0f, 1.0f, 0.0f};

    ShadowCascades cascades{};
    float sliceNear = camera.nearPlane();
    for (uint32_t cascade = 0; cascade < ShadowPass::cascadeCount; cascade++)
    {
        const float sliceFar = splits[cascade];
        const std::array<glm::vec3, 8> corners =
            frustumCorners(camera.projection(aspect, sliceNear, sliceFar) * camera.view());
        sliceNear = sliceFar;

        glm::vec3 center{0.0f};
        for (const glm::vec3& corner : corners)
            center += corner;
        center /= static_cast<float>(corners.size());

        // A sphere's size doesn't depend on the camera's rotation, so the box keeps the same texel size as it turns.
        // Rounding the radius up stops float noise from changing it frame to frame.
        float radius = 0.0f;
        for (const glm::vec3& corner : corners)
            radius = std::max(radius, glm::length(corner - center));
        radius = std::ceil(radius * 16.0f) / 16.0f;

        const glm::mat4 view = glm::lookAt(center + directionToLight * (radius + casterDistance), center, up);
        // Orthographic, because a directional light's rays are parallel.
        // Reverse-Z: near and far are swapped so depth 1 is nearest the light and 0 is farthest.
        glm::mat4 projection = glm::ortho(-radius, radius, -radius, radius, 2.0f * radius + casterDistance, 0.0f);

        // Texel snapping: shift the box so the world origin lands exactly on a texel. The light's rotation is fixed,
        // so every world point then stays on the same spot within its texel as the camera moves.
        const float halfSize = static_cast<float>(ShadowPass::size) / 2.0f;
        const glm::vec2 originTexels = glm::vec2{projection * view * glm::vec4{0.0f, 0.0f, 0.0f, 1.0f}} * halfSize;
        const glm::vec2 snapOffset = (glm::round(originTexels) - originTexels) / halfSize;
        projection[3].x += snapOffset.x;
        projection[3].y += snapOffset.y;

        cascades.viewProjections[cascade] = projection * view;
        cascades.splitDepths[static_cast<glm::length_t>(cascade)] = sliceFar;
        cascades.texelSizes[static_cast<glm::length_t>(cascade)] = 2.0f * radius / static_cast<float>(ShadowPass::size);
    }
    return cascades;
}
