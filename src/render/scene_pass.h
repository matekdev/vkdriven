#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include "vk/graphics_pipeline.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

class Device;
class Scene;
class ShaderCompiler;
class ViewportTarget;

// Written once per frame into the frame's shader data buffer. Must match FrameData in shaders/scene.slang.
struct FrameData
{
    glm::mat4 view;
    glm::mat4 projection;
};

// Pushed before every draw. Must match DrawConstants in shaders/scene.slang.
struct DrawConstants
{
    VkDeviceAddress frame;
    VkDeviceAddress transforms;
    uint32_t transformIndex;
};

// Draws the scene into the viewport's color and depth images, then leaves the color image ready to be sampled.
// Throws std::runtime_error from the constructor if the shader fails to compile.
class ScenePass
{
  public:
    ScenePass(const Device& device, const ShaderCompiler& shaderCompiler, std::filesystem::path shaderPath);

    // Keeps the current pipeline if the shader fails to compile.
    void reloadShaders();

    void record(VkCommandBuffer cb, const ViewportTarget& target, const Scene& scene,
                VkDeviceAddress frameData) const;

  private:
    [[nodiscard]] std::expected<GraphicsPipeline, std::string> buildPipeline() const;

    const Device& device_;
    const ShaderCompiler& shaderCompiler_;
    std::filesystem::path shaderPath_;
    GraphicsPipeline pipeline_;
};
