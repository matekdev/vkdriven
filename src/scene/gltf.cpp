#include "scene/gltf.h"

#include <fastgltf/core.hpp>

#include <format>
#include <utility>

std::expected<fastgltf::Asset, std::string> parseGltf(const std::filesystem::path& path)
{
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None)
        return std::unexpected{
            std::format("Failed to read {}: {}", path.string(), fastgltf::getErrorMessage(data.error()))};

    fastgltf::Parser parser;
    auto asset = parser.loadGltf(data.get(), path.parent_path(),
                                 fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices);
    if (asset.error() != fastgltf::Error::None)
        return std::unexpected{
            std::format("Failed to parse {}: {}", path.string(), fastgltf::getErrorMessage(asset.error()))};

    return std::move(asset.get());
}
