#pragma once

#include <volk.h>

#include <slang-com-ptr.h>
#include <slang.h>

#include "vk/handle.h"

#include <expected>
#include <filesystem>
#include <string>

class Device;

// Compiles Slang source to SPIR-V at runtime. The global session is expensive to create, so it's
// made once and kept. Each compile() gets a fresh session so edited files are always re-read.
class ShaderCompiler
{
  public:
    ShaderCompiler();

    [[nodiscard]] std::expected<DeviceHandle<VkShaderModule>, std::string> compile(
        const Device& device, const std::filesystem::path& path) const;

  private:
    Slang::ComPtr<slang::IGlobalSession> globalSession_;
};
