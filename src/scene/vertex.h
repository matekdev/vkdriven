#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include <array>
#include <cstddef>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    // xyz is the tangent, w (+1 or -1) the bitangent's handedness. All zero when the mesh has no tangents.
    glm::vec4 tangent;

    static constexpr VkVertexInputBindingDescription bindingDescription()
    {
        return {.binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static constexpr std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions()
    {
        return {{
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, pos)},
            {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal)},
            {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, uv)},
            {.location = 3, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(Vertex, tangent)},
        }};
    }
};
