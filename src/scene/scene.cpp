#include "scene/scene.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "scene/gltf.h"
#include "scene/vertex.h"
#include "vk/upload.h"

#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <utility>
#include <variant>

namespace
{

std::expected<Texture, std::string> loadGltfTexture(const Device& device, const Allocator& allocator,
                                                    const CommandPool& commandPool, const fastgltf::Asset& asset,
                                                    const std::filesystem::path& directory, size_t textureIndex,
                                                    VkFormat format)
{
    const fastgltf::Texture& texture = asset.textures[textureIndex];
    if (!texture.imageIndex.has_value())
        return std::unexpected{std::format("Texture {} has no PNG/JPEG image", textureIndex)};

    const fastgltf::Image& image = asset.images[texture.imageIndex.value()];
    const auto* source = std::get_if<fastgltf::sources::URI>(&image.data);
    if (source == nullptr || !source->uri.isLocalPath())
        return std::unexpected{std::format("Image '{}' isn't an external file; embedded images aren't supported yet",
                                           std::string_view{image.name})};

    return Texture::loadImage(device, allocator, commandPool, directory / source->uri.fspath(), format);
}

} // namespace

std::expected<Scene, std::string> Scene::loadGltf(const Device& device, const Allocator& allocator,
                                                  const CommandPool& commandPool, const std::filesystem::path& path)
{
    const auto asset = parseGltf(path);
    if (!asset)
        return std::unexpected{asset.error()};

    Scene scene;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Primitive>& primitives = scene.primitives_;
    std::vector<SceneMesh>& meshes = scene.meshes_;
    std::vector<Material>& materials = scene.materials_;

    std::vector<Texture>& textures = scene.textures_;

    constexpr std::array<std::byte, 4> whitePixel{std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}};
    auto whiteTexture = Texture::fromPixels(device, allocator, commandPool, 1, 1, whitePixel, VK_FORMAT_R8G8B8A8_SRGB);
    if (!whiteTexture)
        return std::unexpected{whiteTexture.error()};
    textures.push_back(std::move(*whiteTexture));

    std::vector<std::optional<uint32_t>> loadedTextureIndices(asset->textures.size());
    for (const fastgltf::Material& material : asset->materials)
    {
        Material& sceneMaterial =
            materials.emplace_back(Material{.baseColorFactor = glm::make_vec4(material.pbrData.baseColorFactor.data())});
        if (!material.pbrData.baseColorTexture.has_value())
            continue;

        const size_t gltfTextureIndex = material.pbrData.baseColorTexture->textureIndex;
        if (!loadedTextureIndices[gltfTextureIndex].has_value())
        {
            auto texture = loadGltfTexture(device, allocator, commandPool, *asset, path.parent_path(),
                                           gltfTextureIndex, VK_FORMAT_R8G8B8A8_SRGB);
            if (!texture)
                return std::unexpected{texture.error()};
            loadedTextureIndices[gltfTextureIndex] = static_cast<uint32_t>(textures.size());
            textures.push_back(std::move(*texture));
        }
        sceneMaterial.baseColorTextureIndex = loadedTextureIndices[gltfTextureIndex].value();
    }
    const auto defaultMaterialIndex = static_cast<uint32_t>(materials.size());
    materials.push_back({});

    for (const fastgltf::Mesh& mesh : asset->meshes)
    {
        meshes.push_back({.firstPrimitive = static_cast<uint32_t>(primitives.size())});
        for (const fastgltf::Primitive& primitive : mesh.primitives)
        {
            if (primitive.type != fastgltf::PrimitiveType::Triangles)
                continue;

            const auto position = primitive.findAttribute("POSITION");
            if (position == primitive.attributes.end())
                return std::unexpected{std::format("{}: primitive in mesh '{}' has no POSITION", path.string(),
                                                   std::string_view{mesh.name})};

            const fastgltf::Accessor& positionAccessor = asset->accessors[position->accessorIndex];
            const size_t firstVertex = vertices.size();
            vertices.resize(firstVertex + positionAccessor.count);
            fastgltf::iterateAccessorWithIndex<glm::vec3>(*asset, positionAccessor, [&](glm::vec3 value, size_t index)
                                                          { vertices[firstVertex + index].pos = value; });

            if (const auto normal = primitive.findAttribute("NORMAL"); normal != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(*asset, asset->accessors[normal->accessorIndex],
                                                              [&](glm::vec3 value, size_t index)
                                                              { vertices[firstVertex + index].normal = value; });
            }

            if (const auto uv = primitive.findAttribute("TEXCOORD_0"); uv != primitive.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(*asset, asset->accessors[uv->accessorIndex],
                                                              [&](glm::vec2 value, size_t index)
                                                              { vertices[firstVertex + index].uv = value; });
            }

            const fastgltf::Accessor& indexAccessor = asset->accessors[primitive.indicesAccessor.value()];
            const size_t firstIndex = indices.size();
            indices.resize(firstIndex + indexAccessor.count);
            fastgltf::copyFromAccessor<uint32_t>(*asset, indexAccessor, indices.data() + firstIndex);

            primitives.push_back({
                .firstIndex = static_cast<uint32_t>(firstIndex),
                .indexCount = static_cast<uint32_t>(indexAccessor.count),
                .vertexOffset = static_cast<int32_t>(firstVertex),
                .materialIndex = primitive.materialIndex.has_value()
                                     ? static_cast<uint32_t>(primitive.materialIndex.value())
                                     : defaultMaterialIndex,
            });
            meshes.back().primitiveCount++;
        }
    }

    if (vertices.empty())
        return std::unexpected{std::format("{} contains no triangle geometry", path.string())};

    std::vector<glm::mat4>& transforms = scene.transforms_;
    if (!asset->scenes.empty())
    {
        fastgltf::iterateSceneNodes(*asset, asset->defaultScene.value_or(0), fastgltf::math::fmat4x4{},
                                    [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& world)
                                    {
                                        if (!node.meshIndex.has_value())
                                            return;

                                        const auto transformIndex = static_cast<uint32_t>(transforms.size());
                                        transforms.push_back(glm::make_mat4(world.data()));

                                        const SceneMesh& mesh = meshes[node.meshIndex.value()];
                                        for (uint32_t i = 0; i < mesh.primitiveCount; i++)
                                        {
                                            scene.draws_.push_back({.primitiveIndex = mesh.firstPrimitive + i,
                                                                    .transformIndex = transformIndex});
                                        }
                                    });
    }

    if (scene.draws_.empty())
        return std::unexpected{std::format("{}: no node in the default scene references a mesh", path.string())};

    scene.vertexBuffer_ = uploadDeviceLocalBuffer(allocator, commandPool, vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    scene.indexBuffer_ = uploadDeviceLocalBuffer(allocator, commandPool, indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    scene.materialBuffer_ =
        uploadDeviceLocalBuffer(allocator, commandPool, materials, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    scene.vertexCount_ = static_cast<uint32_t>(vertices.size());
    scene.indexCount_ = static_cast<uint32_t>(indices.size());
    return scene;
}
