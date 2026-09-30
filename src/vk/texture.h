#pragma once

#include "vk/handle.h"
#include "vk/image.h"

#include <expected>
#include <filesystem>
#include <string>

class Allocator;
class CommandPool;
class Device;

// A sampled texture loaded from a KTX file: the image with all its mips, plus a sampler.
// The image is left in READ_ONLY_OPTIMAL, ready for the fragment shader.
class Texture
{
  public:
    static std::expected<Texture, std::string> loadKtx(const Device& device, const Allocator& allocator,
                                                       const CommandPool& commandPool,
                                                       const std::filesystem::path& path);

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
