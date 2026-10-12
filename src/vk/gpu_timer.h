#pragma once

#include <volk.h>

#include "vk/handle.h"

#include <array>
#include <cstdint>
#include <vector>

class Device;

struct GpuTiming
{
    const char* name{nullptr};
    float milliseconds{0.0f};
};

// Measures how long sections of a command buffer take on the GPU with a pair of timestamp queries each.
// Meant to be one per frame in flight: read() returns the times from this timer's previous submission,
// so only call it after waiting on that submission's fence.
class GpuTimer
{
  public:
    static constexpr uint32_t maxScopes = 8;

    GpuTimer() = default;
    explicit GpuTimer(const Device& device);

    [[nodiscard]] std::vector<GpuTiming> read() const;

    // Starts a new recording; the previous results are gone after this, so read() them first.
    void reset(VkCommandBuffer cb);
    [[nodiscard]] uint32_t begin(VkCommandBuffer cb, const char* name);
    void end(VkCommandBuffer cb, uint32_t scope) const;

  private:
    VkDevice device_{VK_NULL_HANDLE};
    DeviceHandle<VkQueryPool> queryPool_;
    float nanosecondsPerTick_{0.0f};
    uint64_t validBitsMask_{0};
    std::array<const char*, maxScopes> names_{};
    uint32_t scopeCount_{0};
};
