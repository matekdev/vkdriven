#pragma once

#include <fastgltf/types.hpp>

#include <expected>
#include <filesystem>
#include <string>

// Parses a .gltf/.glb file and loads its buffers (the .bin data) into memory.
// Images are left as file URIs; they get decoded when textures are created.
std::expected<fastgltf::Asset, std::string> parseGltf(const std::filesystem::path& path);
