#include "vk/swapchain.h"

#include "platform/window.h"
#include "vk/check.h"
#include "vk/device.h"
#include "vk/surface.h"
#include "vk/sync.h"

#include <algorithm>
#include <limits>

Swapchain::Swapchain(const Device& device, const Surface& surface, const Window& window)
    : device_{device.handle()}, physical_{device.physical()}, surface_{surface.handle()}, window_{window}
{
    const VkSurfaceCapabilitiesKHR capabilities = surfaceCapabilities();
    create(capabilities, chooseExtent(capabilities));
}

Swapchain::~Swapchain()
{
    views_.clear();
    renderCompleteSemaphores_.clear();
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
}

bool Swapchain::recreate()
{
    const VkSurfaceCapabilitiesKHR capabilities = surfaceCapabilities();
    const VkExtent2D extent = chooseExtent(capabilities);
    if (extent.width == 0 || extent.height == 0)
        return false;

    chk(vkDeviceWaitIdle(device_));
    create(capabilities, extent);
    return true;
}

AcquiredImage Swapchain::acquireNextImage(VkSemaphore imageAcquired)
{
    AcquiredImage acquired{};
    const VkResult result = vkAcquireNextImageKHR(device_, swapchain_, std::numeric_limits<uint64_t>::max(),
                                                  imageAcquired, VK_NULL_HANDLE, &acquired.index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        acquired.status = SwapchainStatus::OutOfDate;
        return acquired;
    }

    chk(result);
    if (result == VK_SUBOPTIMAL_KHR)
        acquired.status = SwapchainStatus::Suboptimal;
    return acquired;
}

SwapchainStatus Swapchain::present(VkQueue queue, uint32_t imageIndex)
{
    VkPresentInfoKHR presentInfo{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                 .waitSemaphoreCount = 1,
                                 .pWaitSemaphores = renderCompleteSemaphores_[imageIndex].ptr(),
                                 .swapchainCount = 1,
                                 .pSwapchains = &swapchain_,
                                 .pImageIndices = &imageIndex};
    const VkResult result = vkQueuePresentKHR(queue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
        return SwapchainStatus::OutOfDate;

    chk(result);
    if (result == VK_SUBOPTIMAL_KHR)
        return SwapchainStatus::Suboptimal;
    return SwapchainStatus::Optimal;
}

VkSurfaceCapabilitiesKHR Swapchain::surfaceCapabilities() const
{
    VkSurfaceCapabilitiesKHR capabilities{};
    chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_, surface_, &capabilities));
    return capabilities;
}

VkExtent2D Swapchain::chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabilities.currentExtent;

    const VkExtent2D windowExtent = window_.sizeInPixels();
    return {
        .width = std::clamp(windowExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        .height =
            std::clamp(windowExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}

void Swapchain::create(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D extent)
{
    const VkSwapchainKHR oldSwapchain = swapchain_;
    VkSwapchainCreateInfoKHR swapchainCI{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                         .surface = surface_,
                                         .minImageCount = capabilities.minImageCount,
                                         .imageFormat = format_,
                                         .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                                         .imageExtent = extent,
                                         .imageArrayLayers = 1,
                                         .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                         .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                                         .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                         .presentMode = VK_PRESENT_MODE_FIFO_KHR,
                                         .oldSwapchain = oldSwapchain};
    chk(vkCreateSwapchainKHR(device_, &swapchainCI, nullptr, &swapchain_));
    extent_ = extent;

    views_.clear();
    renderCompleteSemaphores_.clear();
    if (oldSwapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(device_, oldSwapchain, nullptr);

    uint32_t imageCount = 0;
    chk(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr));
    images_.resize(imageCount);
    chk(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, images_.data()));

    for (VkImage image : images_)
    {
        VkImageViewCreateInfo viewCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format_,
            .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
        VkImageView view{VK_NULL_HANDLE};
        chk(vkCreateImageView(device_, &viewCI, nullptr, &view));
        views_.emplace_back(device_, view);
        renderCompleteSemaphores_.push_back(createSemaphore(device_));
    }
}
