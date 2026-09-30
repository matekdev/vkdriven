#include "vk/command_pool.h"

#include "vk/check.h"
#include "vk/device.h"
#include "vk/sync.h"

#include <cstdint>
#include <limits>

CommandPool::CommandPool(const Device& device, VkCommandPoolCreateFlags flags)
    : device_{device.handle()}, queue_{device.queue()}
{
    VkCommandPoolCreateInfo poolCI{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = flags, .queueFamilyIndex = device.queueFamily()};
    VkCommandPool pool{VK_NULL_HANDLE};
    chk(vkCreateCommandPool(device_, &poolCI, nullptr, &pool));
    pool_ = DeviceHandle<VkCommandPool>{device_, pool};
}

void CommandPool::allocate(std::span<VkCommandBuffer> commandBuffers) const
{
    VkCommandBufferAllocateInfo allocateInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                             .commandPool = pool_.get(),
                                             .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                             .commandBufferCount = static_cast<uint32_t>(commandBuffers.size())};
    chk(vkAllocateCommandBuffers(device_, &allocateInfo, commandBuffers.data()));
}

VkCommandBuffer CommandPool::beginOneTime() const
{
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    allocate(std::span{&commandBuffer, 1});

    VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                       .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    chk(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    return commandBuffer;
}

void CommandPool::submitAndWait(VkCommandBuffer commandBuffer) const
{
    chk(vkEndCommandBuffer(commandBuffer));

    const auto fence = createFence(device_);
    VkCommandBufferSubmitInfo commandBufferInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                                .commandBuffer = commandBuffer};
    VkSubmitInfo2 submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                             .commandBufferInfoCount = 1,
                             .pCommandBufferInfos = &commandBufferInfo};
    chk(vkQueueSubmit2(queue_, 1, &submitInfo, fence.get()));
    chk(vkWaitForFences(device_, 1, fence.ptr(), VK_TRUE, std::numeric_limits<uint64_t>::max()));

    vkFreeCommandBuffers(device_, pool_.get(), 1, &commandBuffer);
}
