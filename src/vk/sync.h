#pragma once

#include <volk.h>

#include "vk/check.h"
#include "vk/handle.h"

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
