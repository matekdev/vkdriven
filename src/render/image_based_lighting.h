#pragma once

#include <volk.h>

#include "vk/handle.h"
#include "vk/image.h"

#include <cstdint>
#include <filesystem>

class Allocator;
class CommandPool;
class Device;
class PipelineCache;
class ShaderCompiler;

struct EnvironmentSettings
{
    // Scales the sky and the light it casts on the scene.
    float intensity{0.2f};
    // Draws the irradiance cube as the sky instead of the environment.
    bool showIrradiance{false};
};

// The lighting environment, built once at load time by compute shaders: an equirectangular .hdr is converted
// into a mipmapped cubemap, which is then convolved into a small irradiance cubemap for diffuse lighting.
// Throws std::runtime_error if the image or a shader fails to load.
class ImageBasedLighting
{
  public:
    static constexpr uint32_t environmentSize = 512;
    static constexpr uint32_t irradianceSize = 32;
    static constexpr VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    ImageBasedLighting(const Device& device, const Allocator& allocator, const CommandPool& commandPool,
                       const ShaderCompiler& shaderCompiler, const PipelineCache& pipelineCache,
                       const std::filesystem::path& equirectPath, const std::filesystem::path& shaderDirectory);

    // The environment cubemap with all its mips, for a SamplerCube.
    [[nodiscard]] VkDescriptorImageInfo environmentDescriptorInfo() const
    {
        return {.sampler = sampler_.get(),
                .imageView = environmentCubeView_.get(),
                .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    }

    // The irradiance cubemap, for a SamplerCube: indexed by surface normal, it gives the light a diffuse
    // surface facing that way receives from the whole environment.
    [[nodiscard]] VkDescriptorImageInfo irradianceDescriptorInfo() const
    {
        return {.sampler = sampler_.get(),
                .imageView = irradianceCubeView_.get(),
                .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    }

  private:
    Image environment_;
    DeviceHandle<VkImageView> environmentCubeView_;
    Image irradiance_;
    DeviceHandle<VkImageView> irradianceCubeView_;
    DeviceHandle<VkSampler> sampler_;
};
