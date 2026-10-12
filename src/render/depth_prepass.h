#pragma once

#include <volk.h>

#include "vk/shader_pipeline.h"

#include <filesystem>

class BindlessTextures;
class Device;
class Scene;
class PipelineCache;
class ShaderCompiler;
class ViewportTarget;

// Draws the opaque scene into the viewport's depth image, plus view-space normals and motion vectors.
// Leaves depth as an attachment for the scene pass, and the normal and motion images ready to be sampled.
// Throws std::runtime_error from the constructor if the shader fails to compile.
class DepthPrepass
{
  public:
    DepthPrepass(const Device& device, const ShaderCompiler& shaderCompiler, const PipelineCache& pipelineCache,
                 const BindlessTextures& textures, std::filesystem::path shaderPath);

    // Keeps the current pipeline if the shader fails to compile.
    void reloadShaders()
    {
        pipeline_.reload();
    }

    void record(VkCommandBuffer cb, const ViewportTarget& target, const Scene& scene, VkDeviceAddress frameData) const;

  private:
    const BindlessTextures& textures_;
    ShaderPipeline pipeline_;
};
