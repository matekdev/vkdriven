#include "vk/frame_resources.h"

#include "vk/allocator.h"
#include "vk/command_pool.h"
#include "vk/device.h"
#include "vk/sync.h"

#include <array>
#include <cstdint>
#include <span>

FrameResources::FrameResources(const Device& device, const Allocator& allocator, const CommandPool& commandPool,
                               VkDeviceSize shaderDataSize)
{
    for (Frame& frame : frames_)
    {
        commandPool.allocate(std::span{&frame.commandBuffer, 1});
        frame.inFlight = createFence(device.handle(), VK_FENCE_CREATE_SIGNALED_BIT);
        frame.imageAcquired = createSemaphore(device.handle());
        frame.gpuTimer = GpuTimer{device};
        frame.visibleDrawCount =
            Buffer{allocator, sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT};
        frame.visibleDrawCount.write(std::array<uint32_t, 1>{0});
        frame.shaderData =
            Buffer{allocator, shaderDataSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT};
    }
}
