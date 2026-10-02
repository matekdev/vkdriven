#pragma once

#include "vk/handle.h"
#include "vk/image.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>

class Allocator;
class CommandPool;
class Device;

// A sampled texture: the image with all its mips, plus a sampler.
// The image is left in READ_ONLY_OPTIMAL, ready for the fragment shader.
class Texture
{
  public:
    // Decodes a PNG/JPEG with stb_image and generates the full mip chain on the GPU.
    static std::expected<Texture, std::string> loadImage(const Device& device, const Allocator& allocator,
                                                         const CommandPool& commandPool,
                                                         const std::filesystem::path& path, VkFormat format);

    // Uploads tightly packed RGBA8 pixels as mip 0 and blits each mip from the one above it.
    static std::expected<Texture, std::string> fromPixels(const Device& device, const Allocator& allocator,
                                                          const CommandPool& commandPool, uint32_t width,
                                                          uint32_t height, std::span<const std::byte> pixels,
                                                          VkFormat format);

    [[nodiscard]] VkDescriptorImageInfo descriptorInfo() const
    {
        return {
            .sampler = sampler_.get(), .imageView = image_.view(), .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    }

  private:
    Texture(Image image, DeviceHandle<VkSampler> sampler);

    Image image_;
    DeviceHandle<VkSampler> sampler_;
};
