#pragma once

#include <volk.h>

#include "vk/graphics_pipeline.h"

#include <expected>
#include <filesystem>
#include <string>

class Device;
class ShaderCompiler;

// A graphics pipeline built from a Slang file, rebuilt from the same file and settings on reload().
// Throws std::runtime_error from the constructor if the shader fails to compile.
class ShaderPipeline
{
  public:
    ShaderPipeline(const Device& device, const ShaderCompiler& shaderCompiler, std::filesystem::path shaderPath,
                   const GraphicsPipelineInfo& info);

    // Keeps the current pipeline if the shader fails to compile.
    void reload();

    [[nodiscard]] VkPipeline handle() const
    {
        return pipeline_.handle();
    }

    [[nodiscard]] VkPipelineLayout layout() const
    {
        return pipeline_.layout();
    }

  private:
    [[nodiscard]] std::expected<GraphicsPipeline, std::string> build() const;

    const Device& device_;
    const ShaderCompiler& shaderCompiler_;
    std::filesystem::path shaderPath_;
    GraphicsPipelineInfo info_;
    GraphicsPipeline pipeline_;
};
