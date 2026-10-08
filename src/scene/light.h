#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

// A light infinitely far away, so it shines in the same direction everywhere. Azimuth turns around +Y starting
// from +X, elevation tilts up from the ground plane.
struct DirectionalLight
{
    float azimuthRadians{glm::radians(30.0f)};
    float elevationRadians{glm::radians(40.0f)};
    glm::vec3 color{1.0f};
    float intensity{3.0f};
    // The shadow map covers a sphere around this point. Sized to fit Sponza.
    glm::vec3 shadowCenter{0.0f, 6.0f, 0.0f};
    float shadowRadius{20.0f};

    [[nodiscard]] glm::vec3 directionToLight() const
    {
        return glm::vec3{std::cos(elevationRadians) * std::cos(azimuthRadians), std::sin(elevationRadians),
                         std::cos(elevationRadians) * std::sin(azimuthRadians)};
    }

    [[nodiscard]] glm::vec3 radiance() const
    {
        return color * intensity;
    }

    // Orthographic, because a directional light's rays are parallel. The light sits shadowRadius away from
    // shadowCenter, so the covered sphere spans 0 to 2 * shadowRadius in front of it.
    [[nodiscard]] glm::mat4 viewProjection() const
    {
        const glm::vec3 direction = directionToLight();
        const glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3{0.0f, 0.0f, 1.0f} : glm::vec3{0.0f, 1.0f, 0.0f};
        const glm::mat4 view = glm::lookAt(shadowCenter + direction * shadowRadius, shadowCenter, up);
        // Reverse-Z: near and far are swapped so depth 1 is nearest the light and 0 is farthest.
        const glm::mat4 projection =
            glm::ortho(-shadowRadius, shadowRadius, -shadowRadius, shadowRadius, 2.0f * shadowRadius, 0.0f);
        return projection * view;
    }
};
