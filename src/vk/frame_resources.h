#pragma once

#include <volk.h>

#include "vk/buffer.h"
#include "vk/gpu_timer.h"
#include "vk/handle.h"

#include <array>
#include <cstdint>

class Allocator;
class CommandPool;
class Device;

// Everything the CPU writes or records for one frame. Each frame waits on its own fence before
// touching any of this, so the GPU is guaranteed to be done with the previous use.
struct Frame
{
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    DeviceHandle<VkFence> inFlight;
    DeviceHandle<VkSemaphore> imageAcquired;
    Buffer shaderData;
    GpuTimer gpuTimer;
    // A CPU-readable copy of how many draws the cull pass found visible in this frame's last submission.
    Buffer visibleDrawCount;
};

class FrameResources
{
  public:
    static constexpr uint32_t maxFramesInFlight = 2;

    FrameResources(const Device& device, const Allocator& allocator, const CommandPool& commandPool,
                   VkDeviceSize shaderDataSize);

    [[nodiscard]] Frame& current()
    {
        return frames_[index_];
    }

    void advance()
    {
        index_ = (index_ + 1) % maxFramesInFlight;
    }

  private:
    std::array<Frame, maxFramesInFlight> frames_;
    uint32_t index_{0};
};
