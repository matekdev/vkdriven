#include "vk/image.h"

#include "vk/check.h"

#include <utility>

Image::Image(const Allocator& allocator, const VkImageCreateInfo& imageCI, VkImageAspectFlags aspect,
             VmaAllocationCreateFlags allocationFlags)
    : allocator_{allocator.handle()}, format_{imageCI.format}, extent_{imageCI.extent}, mipLevels_{imageCI.mipLevels}
{
    VmaAllocationCreateInfo allocationCI{.flags = allocationFlags, .usage = VMA_MEMORY_USAGE_AUTO};
    chk(vmaCreateImage(allocator_, &imageCI, &allocationCI, &image_, &allocation_, nullptr));

    VmaAllocatorInfo allocatorInfo{};
    vmaGetAllocatorInfo(allocator_, &allocatorInfo);

    VkImageViewCreateInfo viewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image_,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format_,
        .subresourceRange{.aspectMask = aspect, .levelCount = imageCI.mipLevels, .layerCount = imageCI.arrayLayers}};
    VkImageView view{VK_NULL_HANDLE};
    chk(vkCreateImageView(allocatorInfo.device, &viewCI, nullptr, &view));
    view_ = DeviceHandle<VkImageView>{allocatorInfo.device, view};
}

Image::~Image()
{
    reset();
}

Image::Image(Image&& other) noexcept
    : allocator_{std::exchange(other.allocator_, VK_NULL_HANDLE)}, image_{std::exchange(other.image_, VK_NULL_HANDLE)},
      allocation_{std::exchange(other.allocation_, VK_NULL_HANDLE)}, view_{std::move(other.view_)},
      format_{std::exchange(other.format_, VK_FORMAT_UNDEFINED)}, extent_{std::exchange(other.extent_, {})},
      mipLevels_{std::exchange(other.mipLevels_, 0)}
{
}

Image& Image::operator=(Image&& other) noexcept
{
    if (this == &other)
        return *this;

    reset();
    allocator_ = std::exchange(other.allocator_, VK_NULL_HANDLE);
    image_ = std::exchange(other.image_, VK_NULL_HANDLE);
    allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
    view_ = std::move(other.view_);
    format_ = std::exchange(other.format_, VK_FORMAT_UNDEFINED);
    extent_ = std::exchange(other.extent_, {});
    mipLevels_ = std::exchange(other.mipLevels_, 0);
    return *this;
}

void Image::reset()
{
    view_.reset();
    if (image_ == VK_NULL_HANDLE)
        return;

    vmaDestroyImage(allocator_, image_, allocation_);
    image_ = VK_NULL_HANDLE;
    allocation_ = VK_NULL_HANDLE;
    format_ = VK_FORMAT_UNDEFINED;
    extent_ = {};
    mipLevels_ = 0;
}
