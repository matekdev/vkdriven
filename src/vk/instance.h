#pragma once

#include <volk.h>

#include <span>

// Connection to the Vulkan driver. Loads the function pointers through volk,
// so nothing Vulkan works until one of these exists.
// Debug builds also enable the Khronos validation layer and print its messages to stderr.
class Instance
{
  public:
    Instance(const char* applicationName, std::span<const char* const> extensions);
    ~Instance();

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) = delete;
    Instance& operator=(Instance&&) = delete;

    [[nodiscard]] VkInstance handle() const
    {
        return instance_;
    }

  private:
    VkInstance instance_{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debugMessenger_{VK_NULL_HANDLE};
};
