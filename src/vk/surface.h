#pragma once

#include <volk.h>

class Instance;
class Window;

// Vulkan's view of the window. The swapchain presents to this.
// Has to be destroyed before both the instance and the window.
class Surface
{
  public:
    Surface(const Instance& instance, const Window& window);
    ~Surface();

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    Surface(Surface&&) = delete;
    Surface& operator=(Surface&&) = delete;

    [[nodiscard]] VkSurfaceKHR handle() const
    {
        return surface_;
    }

  private:
    VkInstance instance_{VK_NULL_HANDLE};
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
};
