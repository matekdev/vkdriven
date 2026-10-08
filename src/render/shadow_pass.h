#pragma once

#include <volk.h>

#include "vk/handle.h"
#include "vk/image.h"
#include "vk/shader_pipeline.h"

#include <cstdint>
#include <filesystem>

class Allocator;
class BindlessTextures;
class Device;
class Scene;
class ShaderCompiler;

// Renders the scene's depth as seen from the directional light into the shadow map, then leaves the shadow map
// ready to be sampled by the scene pass. Throws std::runtime_error from the constructor if the shader fails to compile.
class ShadowPass
{
  public:
    static constexpr VkFormat format = VK_FORMAT_D32_SFLOAT;
    static constexpr uint32_t size = 2048;

    ShadowPass(const Device& device, const Allocator& allocator, const ShaderCompiler& shaderCompiler,
               const BindlessTextures& textures, std::filesystem::path shaderPath);

    // Keeps the current pipeline if the shader fails to compile.
    void reloadShaders()
    {
        pipeline_.reload();
    }

    [[nodiscard]] VkDescriptorImageInfo descriptorInfo() const
    {
        return {.sampler = sampler_.get(),
                .imageView = shadowMap_.view(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    }

    void record(VkCommandBuffer cb, const Scene& scene, VkDeviceAddress frameData) const;

  private:
    const BindlessTextures& textures_;
    Image shadowMap_;
    DeviceHandle<VkSampler> sampler_;
    ShaderPipeline pipeline_;
};
