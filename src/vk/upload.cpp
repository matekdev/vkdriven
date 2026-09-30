#include "vk/upload.h"

#include "vk/allocator.h"
#include "vk/command_pool.h"

Buffer uploadDeviceLocalBuffer(const Allocator& allocator, const CommandPool& commandPool,
                               std::span<const std::byte> data, VkBufferUsageFlags usage)
{
    Buffer stagingBuffer{allocator, data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT};
    stagingBuffer.write(data);

    Buffer buffer{allocator, data.size(), usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT};

    commandPool.submitImmediate(
        [&](VkCommandBuffer cb)
        {
            VkBufferCopy region{.size = data.size()};
            vkCmdCopyBuffer(cb, stagingBuffer.handle(), buffer.handle(), 1, &region);

            // Make the copy visible to whatever reads the buffer next (vertex fetch, index fetch, shaders).
            VkBufferMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                           .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                                           .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                           .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                           .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
                                           .buffer = buffer.handle(),
                                           .size = VK_WHOLE_SIZE};
            VkDependencyInfo dependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                            .bufferMemoryBarrierCount = 1,
                                            .pBufferMemoryBarriers = &barrier};
            vkCmdPipelineBarrier2(cb, &dependencyInfo);
        });

    return buffer;
}
