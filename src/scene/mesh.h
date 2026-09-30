#pragma once

#include <glm/glm.hpp>

#include "vk/buffer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;

    static constexpr VkVertexInputBindingDescription bindingDescription()
    {
        return {.binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static constexpr std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions()
    {
        return {{
            {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, pos)},
            {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal)},
            {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, uv)},
        }};
    }
};

// One indexed mesh in a single buffer: vertices first, then 16-bit indices at indexOffset().
class Mesh
{
  public:
    static constexpr VkIndexType indexType = VK_INDEX_TYPE_UINT16;

    static std::expected<Mesh, std::string> loadObj(const Allocator& allocator, const std::filesystem::path& path);

    [[nodiscard]] VkBuffer buffer() const
    {
        return buffer_.handle();
    }

    [[nodiscard]] VkDeviceSize indexOffset() const
    {
        return indexOffset_;
    }

    [[nodiscard]] uint32_t indexCount() const
    {
        return indexCount_;
    }

  private:
    Mesh(Buffer buffer, VkDeviceSize indexOffset, uint32_t indexCount);

    Buffer buffer_;
    VkDeviceSize indexOffset_{0};
    uint32_t indexCount_{0};
};
