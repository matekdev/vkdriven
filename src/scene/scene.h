#pragma once

#include <volk.h>

#include "vk/buffer.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

class Allocator;
class CommandPool;

// One draw's worth of geometry: a range in the scene's shared index buffer, plus the offset added
// to every index so it points at this primitive's vertices in the shared vertex buffer.
struct Primitive
{
    uint32_t firstIndex{0};
    uint32_t indexCount{0};
    int32_t vertexOffset{0};
    std::optional<uint32_t> materialIndex;
};

// A glTF mesh: a contiguous run of primitives. Nodes refer to meshes by index.
struct SceneMesh
{
    uint32_t firstPrimitive{0};
    uint32_t primitiveCount{0};
};

// One thing to draw: which primitive, and which world transform (index into the transforms buffer).
// A mesh placed by several nodes produces one Draw per node per primitive.
struct Draw
{
    uint32_t primitiveIndex{0};
    uint32_t transformIndex{0};
};

// All geometry of a glTF file in one device-local vertex buffer and one 32-bit index buffer, plus the
// node hierarchy flattened into a draw list and a buffer of world matrices the shaders read by address.
class Scene
{
  public:
    static constexpr VkIndexType indexType = VK_INDEX_TYPE_UINT32;

    static std::expected<Scene, std::string> loadGltf(const Allocator& allocator, const CommandPool& commandPool,
                                                      const std::filesystem::path& path);

    [[nodiscard]] VkBuffer vertexBuffer() const
    {
        return vertexBuffer_.handle();
    }

    [[nodiscard]] VkBuffer indexBuffer() const
    {
        return indexBuffer_.handle();
    }

    [[nodiscard]] std::span<const Primitive> primitives() const
    {
        return primitives_;
    }

    [[nodiscard]] std::span<const SceneMesh> meshes() const
    {
        return meshes_;
    }

    [[nodiscard]] std::span<const Draw> draws() const
    {
        return draws_;
    }

    [[nodiscard]] VkDeviceAddress transformsAddress() const
    {
        return transformsBuffer_.deviceAddress();
    }

    [[nodiscard]] uint32_t vertexCount() const
    {
        return vertexCount_;
    }

    [[nodiscard]] uint32_t indexCount() const
    {
        return indexCount_;
    }

    [[nodiscard]] uint32_t transformCount() const
    {
        return transformCount_;
    }

  private:
    Scene() = default;

    Buffer vertexBuffer_;
    Buffer indexBuffer_;
    Buffer transformsBuffer_;
    std::vector<Primitive> primitives_;
    std::vector<SceneMesh> meshes_;
    std::vector<Draw> draws_;
    uint32_t vertexCount_{0};
    uint32_t indexCount_{0};
    uint32_t transformCount_{0};
};
