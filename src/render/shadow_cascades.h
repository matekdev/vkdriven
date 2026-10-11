#pragma once

#include <glm/glm.hpp>

#include "render/shadow_pass.h"

#include <array>

class Camera;

struct ShadowCascades
{
    std::array<glm::mat4, ShadowPass::cascadeCount> viewProjections;
    // Per cascade: the view-space depth where it ends, and the world-space size of one of its texels.
    glm::vec4 splitDepths;
    glm::vec4 texelSizes;
};

// Splits the camera's view out to settings.distance into slices and fits an orthographic light box around each.
// Each box is fitted to the slice's bounding sphere and snapped to whole shadow map texels, so it doesn't change
// size as the camera turns or slide across texels as it moves, which keeps shadow edges from shimmering.
ShadowCascades computeShadowCascades(const Camera& camera, float aspect, glm::vec3 directionToLight,
                                     const ShadowSettings& settings);
