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

struct FrameData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 directionToLight;
};

struct DrawConstants
{
    glm::mat4 model;
    glm::vec4 baseColorFactor;
    VkDeviceAddress frame;
};

// Draws the scene into the viewport's color and depth images, then leaves the color image ready to be sampled.
// Throws std::runtime_error from the constructor if the shader fails to compile.
class ScenePass
{
  public:
    ScenePass(const Device& device, const ShaderCompiler& shaderCompiler, std::filesystem::path shaderPath);

    // Keeps the current pipeline if the shader fails to compile.
    void reloadShaders();

    void record(VkCommandBuffer cb, const ViewportTarget& target, const Scene& scene, VkDeviceAddress frameData) const;

  private:
    [[nodiscard]] std::expected<GraphicsPipeline, std::string> buildPipeline() const;

    const Device& device_;
    const ShaderCompiler& shaderCompiler_;
    std::filesystem::path shaderPath_;
    GraphicsPipeline pipeline_;
};
