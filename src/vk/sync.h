#pragma once

#include <volk.h>

#include "vk/check.h"
#include "vk/handle.h"

#include <cstdint>
#include <span>

inline DeviceHandle<VkFence> createFence(VkDevice device, VkFenceCreateFlags flags = 0)
{
    VkFenceCreateInfo fenceCI{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = flags};
    VkFence fence{VK_NULL_HANDLE};
    chk(vkCreateFence(device, &fenceCI, nullptr, &fence));
    return {device, fence};
}

inline DeviceHandle<VkSemaphore> createSemaphore(VkDevice device)
{
    VkSemaphoreCreateInfo semaphoreCI{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore semaphore{VK_NULL_HANDLE};
    chk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &semaphore));
    return {device, semaphore};
}

inline VkImageSubresourceRange subresourceRange(VkImageAspectFlags aspect)
{
    return {.aspectMask = aspect, .levelCount = 1, .layerCount = 1};
}

inline void pipelineBarrier(VkCommandBuffer cb, std::span<const VkImageMemoryBarrier2> barriers)
{
    VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                    .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
                                    .pImageMemoryBarriers = barriers.data()};
    vkCmdPipelineBarrier2(cb, &dependencyInfo);
}

inline void pipelineBarrier(VkCommandBuffer cb, const VkImageMemoryBarrier2& barrier)
{
    pipelineBarrier(cb, std::span{&barrier, 1});
}
