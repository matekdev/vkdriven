#include "vk/shader_pipeline.h"

#include "util/expected.h"
#include "vk/device.h"
#include "vk/shader_compiler.h"

#include <print>
#include <utility>

ShaderPipeline::ShaderPipeline(const Device& device, const ShaderCompiler& shaderCompiler,
                               std::filesystem::path shaderPath, const GraphicsPipelineInfo& info)
    : device_{device}, shaderCompiler_{shaderCompiler}, shaderPath_{std::move(shaderPath)}, info_{info},
      pipeline_{orThrow(build())}
{
}

void ShaderPipeline::reload()
{
    auto reloadedPipeline = build();
    if (!reloadedPipeline)
    {
        std::println(stderr, "{}", reloadedPipeline.error());
        return;
    }
    device_.waitIdle();
    pipeline_ = std::move(*reloadedPipeline);
    std::println("Reloaded {}", shaderPath_.string());
}

std::expected<GraphicsPipeline, std::string> ShaderPipeline::build() const
{
    return shaderCompiler_.compile(device_, shaderPath_)
        .transform([&](const DeviceHandle<VkShaderModule>& shaderModule)
                   { return GraphicsPipeline{device_, shaderModule.get(), info_}; });
}
