#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include "render/shadow_pass.h"
#include "vk/shader_pipeline.h"

#include <array>
#include <cstdint>
#include <filesystem>

class BindlessTextures;
class Device;
class Scene;
class PipelineCache;
class ShaderCompiler;
class ViewportTarget;

struct FrameData
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 directionToLight;
    glm::vec4 cameraPosition;
    glm::vec4 lightRadiance;
    std::array<glm::mat4, ShadowPass::cascadeCount> lightViewProjections;
    // Per cascade: the view-space depth where it ends, and the world-space size of one of its texels.
    glm::vec4 cascadeSplitDepths;
    glm::vec4 cascadeTexelSizes;
    uint32_t shadowMapTextureIndex;
    float normalOffset;
    uint32_t showCascades;
    uint32_t environmentTextureIndex;
    uint32_t irradianceTextureIndex;
    uint32_t showIrradiance;
    float environmentIntensity;
};
static_assert(ShadowPass::cascadeCount == 4, "cascadeSplitDepths and cascadeTexelSizes hold one float per cascade");
static_assert(sizeof(FrameData) == 492);

struct DrawConstants
{
    VkDeviceAddress frame;
    VkDeviceAddress draws;
    VkDeviceAddress transforms;
    VkDeviceAddress materials;
    uint32_t drawIndex;
    uint32_t cascadeIndex;
};

// Draws the scene into the viewport's color and depth images, then leaves the color image ready to be sampled.
// Throws std::runtime_error from the constructor if the shader fails to compile.
class ScenePass
{
  public:
    ScenePass(const Device& device, const ShaderCompiler& shaderCompiler, const PipelineCache& pipelineCache,
              const BindlessTextures& textures, std::filesystem::path shaderPath,
              std::filesystem::path skyboxShaderPath);

    // Keeps the current pipelines if a shader fails to compile.
    void reloadShaders()
    {
        pipeline_.reload();
        skyboxPipeline_.reload();
    }

    void record(VkCommandBuffer cb, const ViewportTarget& target, const Scene& scene, VkDeviceAddress frameData) const;

  private:
    const BindlessTextures& textures_;
    ShaderPipeline pipeline_;
    ShaderPipeline skyboxPipeline_;
};
