#pragma once

#include <glm/glm.hpp>

#include <cmath>

// A light infinitely far away, so it shines in the same direction everywhere. Azimuth turns around +Y starting
// from +X, elevation tilts up from the ground plane.
struct DirectionalLight
{
    float azimuthRadians{glm::radians(30.0f)};
    float elevationRadians{glm::radians(40.0f)};
    glm::vec3 color{1.0f};
    float intensity{3.0f};

    [[nodiscard]] glm::vec3 directionToLight() const
    {
        return glm::vec3{std::cos(elevationRadians) * std::cos(azimuthRadians), std::sin(elevationRadians),
                         std::cos(elevationRadians) * std::sin(azimuthRadians)};
    }

    [[nodiscard]] glm::vec3 radiance() const
    {
        return color * intensity;
    }
};
