#include "scene/mesh.h"

#include <tiny_obj_loader.h>

#include <format>
#include <limits>
#include <utility>
#include <vector>

Mesh::Mesh(Buffer buffer, VkDeviceSize indexOffset, uint32_t indexCount)
    : buffer_{std::move(buffer)}, indexOffset_{indexOffset}, indexCount_{indexCount}
{
}

std::expected<Mesh, std::string> Mesh::loadObj(const Allocator& allocator, const std::filesystem::path& path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string error;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, &error, path.string().c_str()))
        return std::unexpected{std::format("Failed to load mesh {}: {}", path.string(), error)};
    if (shapes.empty())
        return std::unexpected{std::format("Mesh {} has no shapes", path.string())};

    const auto& objIndices = shapes[0].mesh.indices;
    if (objIndices.size() > std::numeric_limits<uint16_t>::max())
        return std::unexpected{std::format("Mesh {} has too many vertices for 16-bit indices", path.string())};

    std::vector<Vertex> vertices{};
    std::vector<uint16_t> indices{};
    for (const auto& index : objIndices)
    {
        Vertex v{
            .pos = {attrib.vertices[index.vertex_index * 3], -attrib.vertices[index.vertex_index * 3 + 1],
                    attrib.vertices[index.vertex_index * 3 + 2]},
            .normal = {attrib.normals[index.normal_index * 3], -attrib.normals[index.normal_index * 3 + 1],
                       attrib.normals[index.normal_index * 3 + 2]},
            .uv = {attrib.texcoords[index.texcoord_index * 2], 1.0f - attrib.texcoords[index.texcoord_index * 2 + 1]}};
        vertices.push_back(v);
        indices.push_back(static_cast<uint16_t>(indices.size()));
    }

    const VkDeviceSize vertexBytes{sizeof(Vertex) * vertices.size()};
    const VkDeviceSize indexBytes{sizeof(uint16_t) * indices.size()};
    Buffer buffer{allocator, vertexBytes + indexBytes,
                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT};
    buffer.write(vertices);
    buffer.write(indices, vertexBytes);

    return Mesh{std::move(buffer), vertexBytes, static_cast<uint32_t>(indices.size())};
}
