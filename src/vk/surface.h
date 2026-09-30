#pragma once

#include <volk.h>

class Instance;
class Window;

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
