#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include "vk/buffer.h"
#include "vk/texture.h"

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

class Allocator;
class CommandPool;
class Device;

// One draw's worth of geometry: a range in the scene's shared index buffer, plus the offset added
// to every index so it points at this primitive's vertices in the shared vertex buffer.
struct Primitive
{
    uint32_t firstIndex{0};
    uint32_t indexCount{0};
    int32_t vertexOffset{0};
    uint32_t materialIndex{0};
};

constexpr uint32_t whiteSrgbTextureIndex = 0;
constexpr uint32_t whiteLinearTextureIndex = 1;
constexpr uint32_t flatNormalTextureIndex = 2;

// Primitives without a glTF material point at a default material appended after the file's own.
// Texture indices point into the scene's textures; materials without a texture use a 1x1 white one, so the
// factors pass through unchanged. Color textures are sRGB, data textures (metallic-roughness) are linear.
// Without a normal map, a 1x1 flat normal (0, 0, 1) leaves the vertex normal unchanged.
// Uploaded to the GPU as-is, so it must match Material in shaders/scene.slang.
struct Material
{
    glm::vec4 baseColorFactor{1.0f};
    uint32_t baseColorTextureIndex{whiteSrgbTextureIndex};
    uint32_t metallicRoughnessTextureIndex{whiteLinearTextureIndex};
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
    float alphaCutoff{0.0f};
    uint32_t normalTextureIndex{flatNormalTextureIndex};
    float normalScale{1.0f};
    uint32_t padding{0};
};
static_assert(sizeof(Material) == 48);

// A glTF mesh: a contiguous run of primitives. Nodes refer to meshes by index.
struct SceneMesh
{
    uint32_t firstPrimitive{0};
    uint32_t primitiveCount{0};
};

// One thing to draw: which primitive, its material, and which world transform (index into the scene's transforms).
// A mesh placed by several nodes produces one Draw per node per primitive.
// Uploaded to the GPU as-is, so it must match Draw in shaders/scene.slang.
struct Draw
{
    uint32_t primitiveIndex{0};
    uint32_t materialIndex{0};
    uint32_t transformIndex{0};
};
static_assert(sizeof(Draw) == 12);

// A node's world matrix, plus its inverse-transpose for normals (only the upper 3x3 is used).
// Uploaded to the GPU as-is, so it must match Transform in shaders/scene.slang.
struct Transform
{
    glm::mat4 model{1.0f};
    glm::mat4 normal{1.0f};
};
static_assert(sizeof(Transform) == 128);

// All geometry of a glTF file in one device-local vertex buffer and one 32-bit index buffer, plus the
// node hierarchy flattened into a draw list and a list of world matrices.
class Scene
{
  public:
    static constexpr VkIndexType indexType = VK_INDEX_TYPE_UINT32;

    static std::expected<Scene, std::string> loadGltf(const Device& device, const Allocator& allocator,
                                                      const CommandPool& commandPool,
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

    [[nodiscard]] std::span<const Material> materials() const
    {
        return materials_;
    }

    [[nodiscard]] VkDeviceAddress materialBufferAddress() const
    {
        return materialBuffer_.deviceAddress();
    }

    [[nodiscard]] std::span<const Texture> textures() const
    {
        return textures_;
    }

    [[nodiscard]] std::span<const SceneMesh> meshes() const
    {
        return meshes_;
    }

    [[nodiscard]] std::span<const Draw> draws() const
    {
        return draws_;
    }

    [[nodiscard]] VkDeviceAddress drawBufferAddress() const
    {
        return drawBuffer_.deviceAddress();
    }

    [[nodiscard]] std::span<const Transform> transforms() const
    {
        return transforms_;
    }

    [[nodiscard]] VkDeviceAddress transformBufferAddress() const
    {
        return transformBuffer_.deviceAddress();
    }

    [[nodiscard]] uint32_t vertexCount() const
    {
        return vertexCount_;
    }

    [[nodiscard]] uint32_t indexCount() const
    {
        return indexCount_;
    }

  private:
    Scene() = default;

    Buffer vertexBuffer_;
    Buffer indexBuffer_;
    Buffer materialBuffer_;
    Buffer drawBuffer_;
    Buffer transformBuffer_;
    std::vector<Primitive> primitives_;
    std::vector<Material> materials_;
    std::vector<Texture> textures_;
    std::vector<SceneMesh> meshes_;
    std::vector<Draw> draws_;
    std::vector<Transform> transforms_;
    uint32_t vertexCount_{0};
    uint32_t indexCount_{0};
};
