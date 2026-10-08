#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include "vk/shader_pipeline.h"

#include <cstdint>
#include <filesystem>

class BindlessTextures;
class Device;
class Scene;
class ShaderCompiler;
class ViewportTarget;

struct FrameData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 directionToLight;
    glm::vec4 cameraPosition;
    glm::vec4 lightRadiance;
};
static_assert(sizeof(FrameData) == 176);

struct DrawConstants
{
    VkDeviceAddress frame;
    VkDeviceAddress draws;
    VkDeviceAddress transforms;
    VkDeviceAddress materials;
    uint32_t drawIndex;
};

// Draws the scene into the viewport's color and depth images, then leaves the color image ready to be sampled.
// Throws std::runtime_error from the constructor if the shader fails to compile.
class ScenePass
{
  public:
    ScenePass(const Device& device, const ShaderCompiler& shaderCompiler, const BindlessTextures& textures,
              std::filesystem::path shaderPath);

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
