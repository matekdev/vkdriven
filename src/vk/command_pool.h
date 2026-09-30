#pragma once

#include <volk.h>

#include "vk/handle.h"

#include <concepts>
#include <functional>
#include <span>

class Device;

// Command pool for the device's queue family. Command buffers allocated from it are freed
// together with the pool. submitImmediate() is for one-off work like uploads: record, submit, wait.
class CommandPool
{
  public:
    explicit CommandPool(const Device& device,
                         VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    void allocate(std::span<VkCommandBuffer> commandBuffers) const;

    template <std::invocable<VkCommandBuffer> Record> void submitImmediate(Record&& record) const
    {
        const VkCommandBuffer commandBuffer = beginOneTime();
        std::invoke(std::forward<Record>(record), commandBuffer);
        submitAndWait(commandBuffer);
    }

    [[nodiscard]] VkCommandPool handle() const
    {
        return pool_.get();
    }

  private:
    [[nodiscard]] VkCommandBuffer beginOneTime() const;
    void submitAndWait(VkCommandBuffer commandBuffer) const;

    VkDevice device_{VK_NULL_HANDLE};
    VkQueue queue_{VK_NULL_HANDLE};
    DeviceHandle<VkCommandPool> pool_;
};
