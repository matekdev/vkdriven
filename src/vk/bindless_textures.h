#pragma once

#include <volk.h>

#include "vk/handle.h"

#include <cstdint>
#include <span>

class Device;
class Texture;

// One descriptor set holding a variable-length array of combined image samplers at binding 0.
// Shaders index into it by texture index, so every draw can use the same set.
class BindlessTextures
{
  public:
    BindlessTextures(const Device& device, uint32_t capacity);

    void write(std::span<const Texture> textures, uint32_t firstIndex = 0) const;
    void write(std::span<const VkDescriptorImageInfo> imageInfos, uint32_t firstIndex = 0) const;

    [[nodiscard]] VkDescriptorSetLayout layout() const
    {
        return layout_.get();
    }

    [[nodiscard]] VkDescriptorSet set() const
    {
        return set_;
    }

    [[nodiscard]] uint32_t capacity() const
    {
        return capacity_;
    }

  private:
    VkDevice device_{VK_NULL_HANDLE};
    DeviceHandle<VkDescriptorSetLayout> layout_;
    DeviceHandle<VkDescriptorPool> pool_;
    VkDescriptorSet set_{VK_NULL_HANDLE};
    uint32_t capacity_{0};
};
