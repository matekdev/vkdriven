#pragma once

#include <volk.h>

#include <utility>

inline void destroyHandle(VkDevice device, VkFence handle)
{
    vkDestroyFence(device, handle, nullptr);
}

inline void destroyHandle(VkDevice device, VkSemaphore handle)
{
    vkDestroySemaphore(device, handle, nullptr);
}

inline void destroyHandle(VkDevice device, VkImageView handle)
{
    vkDestroyImageView(device, handle, nullptr);
}

inline void destroyHandle(VkDevice device, VkSampler handle)
{
    vkDestroySampler(device, handle, nullptr);
}

inline void destroyHandle(VkDevice device, VkShaderModule handle)
{
    vkDestroyShaderModule(device, handle, nullptr);
}

inline void destroyHandle(VkDevice device, VkPipelineLayout handle)
{
    vkDestroyPipelineLayout(device, handle, nullptr);
}

inline void destroyHandle(VkDevice device, VkPipeline handle)
{
    vkDestroyPipeline(device, handle, nullptr);
}

inline void destroyHandle(VkDevice device, VkDescriptorSetLayout handle)
{
    vkDestroyDescriptorSetLayout(device, handle, nullptr);
}

inline void destroyHandle(VkDevice device, VkDescriptorPool handle)
{
    vkDestroyDescriptorPool(device, handle, nullptr);
}

inline void destroyHandle(VkDevice device, VkCommandPool handle)
{
    vkDestroyCommandPool(device, handle, nullptr);
}

template <typename T>
concept DestroyableDeviceHandle = requires(VkDevice device, T handle) { destroyHandle(device, handle); };

template <DestroyableDeviceHandle T> class DeviceHandle
{
  public:
    DeviceHandle() = default;

    DeviceHandle(VkDevice owner, T handle) : device_{owner}, handle_{handle}
    {
    }

    ~DeviceHandle()
    {
        reset();
    }

    DeviceHandle(const DeviceHandle&) = delete;
    DeviceHandle& operator=(const DeviceHandle&) = delete;

    DeviceHandle(DeviceHandle&& other) noexcept
        : device_{std::exchange(other.device_, VK_NULL_HANDLE)}, handle_{std::exchange(other.handle_, VK_NULL_HANDLE)}
    {
    }

    DeviceHandle& operator=(DeviceHandle&& other) noexcept
    {
        if (this == &other)
            return *this;

        reset();
        device_ = std::exchange(other.device_, VK_NULL_HANDLE);
        handle_ = std::exchange(other.handle_, VK_NULL_HANDLE);
        return *this;
    }

    void reset()
    {
        if (handle_ == VK_NULL_HANDLE)
            return;

        destroyHandle(device_, handle_);
        handle_ = VK_NULL_HANDLE;
    }

    [[nodiscard]] T get() const
    {
        return handle_;
    }

    [[nodiscard]] const T* ptr() const
    {
        return &handle_;
    }

    explicit operator bool() const
    {
        return handle_ != VK_NULL_HANDLE;
    }

  private:
    VkDevice device_{VK_NULL_HANDLE};
    T handle_{VK_NULL_HANDLE};
};
