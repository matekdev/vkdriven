#pragma once

#include <volk.h>

#include "vk/handle.h"

#include <cstdint>
#include <vector>

class Device;
class Surface;
class Window;

enum class SwapchainStatus
{
    Optimal,
    Suboptimal,
    OutOfDate,
};

struct AcquiredImage
{
    SwapchainStatus status{SwapchainStatus::Optimal};
    uint32_t index{0};
};

// The images we present to the window, plus a view and a "render complete" semaphore per image.
// The images themselves belong to the swapchain, so they're never destroyed directly.
class Swapchain
{
  public:
    Swapchain(const Device& device, const Surface& surface, const Window& window);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    // Waits for the device to go idle and rebuilds everything for the window's current size.
    // Returns false (and does nothing) while the window is minimized.
    bool recreate();

    AcquiredImage acquireNextImage(VkSemaphore imageAcquired);
    SwapchainStatus present(VkQueue queue, uint32_t imageIndex);

    [[nodiscard]] VkFormat format() const
    {
        return format_;
    }

    [[nodiscard]] VkExtent2D extent() const
    {
        return extent_;
    }

    [[nodiscard]] VkImage image(uint32_t index) const
    {
        return images_[index];
    }

    [[nodiscard]] VkImageView view(uint32_t index) const
    {
        return views_[index].get();
    }

    [[nodiscard]] VkSemaphore renderComplete(uint32_t index) const
    {
        return renderCompleteSemaphores_[index].get();
    }

  private:
    [[nodiscard]] VkSurfaceCapabilitiesKHR surfaceCapabilities() const;
    [[nodiscard]] VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    void create(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D extent);

    VkDevice device_{VK_NULL_HANDLE};
    VkPhysicalDevice physical_{VK_NULL_HANDLE};
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    const Window& window_;

    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
    VkFormat format_{VK_FORMAT_B8G8R8A8_SRGB};
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<DeviceHandle<VkImageView>> views_;
    std::vector<DeviceHandle<VkSemaphore>> renderCompleteSemaphores_;
};
