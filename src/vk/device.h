#pragma once

#include <volk.h>

#include <cstdint>

class Instance;
class Surface;

// The GPU we picked plus the logical device and the one graphics/present queue we use.
// Everything created from the device has to be destroyed before this is.
class Device
{
  public:
    Device(const Instance& instance, const Surface& surface);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    [[nodiscard]] VkPhysicalDevice physical() const
    {
        return physical_;
    }

    [[nodiscard]] VkDevice handle() const
    {
        return device_;
    }

    [[nodiscard]] VkQueue queue() const
    {
        return queue_;
    }

    [[nodiscard]] uint32_t queueFamily() const
    {
        return queueFamily_;
    }

    [[nodiscard]] VkFormat depthFormat() const
    {
        return depthFormat_;
    }

    void waitIdle() const;

  private:
    VkPhysicalDevice physical_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue queue_{VK_NULL_HANDLE};
    uint32_t queueFamily_{0};
    VkFormat depthFormat_{VK_FORMAT_UNDEFINED};
};
