#include "ui/viewport_target.h"

#include <imgui_impl_vulkan.h>

#include "vk/allocator.h"
#include "vk/check.h"

#include <array>
#include <bit>
#include <cstdint>

ViewportTarget::ViewportTarget(const Allocator& allocator, VkFormat depthFormat, VkExtent2D extent)
    : allocator_{allocator}, depthFormat_{depthFormat}
{
    create(extent);
}

ViewportTarget::~ViewportTarget()
{
    destroy();
}

bool ViewportTarget::needsResize(VkExtent2D extent) const
{
    if (extent.width == 0 || extent.height == 0)
        return false;
    return extent.width != extent_.width || extent.height != extent_.height;
}

void ViewportTarget::resize(VkExtent2D extent)
{
    destroy();
    create(extent);
}

ImTextureID ViewportTarget::texture() const
{
    return std::bit_cast<ImTextureID>(texture_);
}

void ViewportTarget::create(VkExtent2D extent)
{
    extent_ = extent;

    const auto viewFormats = std::to_array({colorFormat, displayFormat});
    VkImageFormatListCreateInfo formatListCI{.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO,
                                             .viewFormatCount = static_cast<uint32_t>(viewFormats.size()),
                                             .pViewFormats = viewFormats.data()};
    VkImageCreateInfo colorCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &formatListCI,
        .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = colorFormat,
        .extent{.width = extent.width, .height = extent.height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    color_ = Image{allocator_, colorCI, VK_IMAGE_ASPECT_COLOR_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT};

    VmaAllocatorInfo allocatorInfo{};
    vmaGetAllocatorInfo(allocator_.handle(), &allocatorInfo);
    VkImageViewCreateInfo displayViewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = color_.handle(),
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = displayFormat,
        .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
    VkImageView displayView{VK_NULL_HANDLE};
    chk(vkCreateImageView(allocatorInfo.device, &displayViewCI, nullptr, &displayView));
    displayView_ = DeviceHandle<VkImageView>{allocatorInfo.device, displayView};

    VkImageCreateInfo depthCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthFormat_,
        .extent{.width = extent.width, .height = extent.height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    depth_ = Image{allocator_, depthCI, VK_IMAGE_ASPECT_DEPTH_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT};

    VkImageCreateInfo hdrCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = hdrFormat,
        .extent{.width = extent.width, .height = extent.height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    hdr_ = Image{allocator_, hdrCI, VK_IMAGE_ASPECT_COLOR_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT};

    VkSamplerCreateInfo hdrSamplerCI{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_LINEAR,
                                     .minFilter = VK_FILTER_LINEAR,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
    VkSampler hdrSampler{VK_NULL_HANDLE};
    chk(vkCreateSampler(allocatorInfo.device, &hdrSamplerCI, nullptr, &hdrSampler));
    hdrSampler_ = DeviceHandle<VkSampler>{allocatorInfo.device, hdrSampler};

    texture_ = ImGui_ImplVulkan_AddTexture(displayView_.get(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ViewportTarget::destroy()
{
    if (texture_ != VK_NULL_HANDLE)
        ImGui_ImplVulkan_RemoveTexture(texture_);

    texture_ = VK_NULL_HANDLE;
    displayView_.reset();
    color_.reset();
    depth_.reset();
    hdr_.reset();
    hdrSampler_.reset();
}
