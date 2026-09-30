#pragma once

#include "vk/allocator.h"
#include "vk/handle.h"

#include <cstdint>

// A VkImage, its VMA allocation and a view covering every mip level and layer.
class Image
{
  public:
    Image() = default;
    Image(const Allocator& allocator, const VkImageCreateInfo& imageCI, VkImageAspectFlags aspect,
          VmaAllocationCreateFlags allocationFlags = 0);
    ~Image();

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    Image(Image&& other) noexcept;
    Image& operator=(Image&& other) noexcept;

    void reset();

    [[nodiscard]] VkImage handle() const
    {
        return image_;
    }

    [[nodiscard]] VkImageView view() const
    {
        return view_.get();
    }

    [[nodiscard]] VkFormat format() const
    {
        return format_;
    }

    [[nodiscard]] VkExtent3D extent() const
    {
        return extent_;
    }

    [[nodiscard]] uint32_t mipLevels() const
    {
        return mipLevels_;
    }

  private:
    VmaAllocator allocator_{VK_NULL_HANDLE};
    VkImage image_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
    DeviceHandle<VkImageView> view_;
    VkFormat format_{VK_FORMAT_UNDEFINED};
    VkExtent3D extent_{};
    uint32_t mipLevels_{0};
};
