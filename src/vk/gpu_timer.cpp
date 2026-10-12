#include "vk/gpu_timer.h"

#include "vk/check.h"
#include "vk/device.h"

#include <cassert>

GpuTimer::GpuTimer(const Device& device)
    : device_{device.handle()}, nanosecondsPerTick_{device.timestampPeriod()},
      validBitsMask_{device.timestampValidBits() >= 64 ? ~0ull : (1ull << device.timestampValidBits()) - 1}
{
    VkQueryPoolCreateInfo queryPoolCI{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                      .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                      .queryCount = maxScopes * 2};
    VkQueryPool queryPool{VK_NULL_HANDLE};
    chk(vkCreateQueryPool(device_, &queryPoolCI, nullptr, &queryPool));
    queryPool_ = {device_, queryPool};
}

std::vector<GpuTiming> GpuTimer::read() const
{
    if (scopeCount_ == 0)
        return {};

    // Each query is written as a value plus an availability word, so a scope that never ran reads as unavailable
    // instead of failing the whole call.
    std::array<uint64_t, maxScopes * 2 * 2> results{};
    chk(vkGetQueryPoolResults(device_, queryPool_.get(), 0, scopeCount_ * 2, sizeof(results), results.data(),
                              2 * sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT));

    std::vector<GpuTiming> timings;
    for (uint32_t scope = 0; scope < scopeCount_; scope++)
    {
        const uint64_t start = results[scope * 4];
        const bool startAvailable = results[scope * 4 + 1] != 0;
        const uint64_t end = results[scope * 4 + 2];
        const bool endAvailable = results[scope * 4 + 3] != 0;
        if (!startAvailable || !endAvailable)
            continue;

        const uint64_t ticks = ((end & validBitsMask_) - (start & validBitsMask_)) & validBitsMask_;
        timings.push_back({.name = names_[scope],
                           .milliseconds = static_cast<float>(ticks) * nanosecondsPerTick_ / 1'000'000.0f});
    }
    return timings;
}

void GpuTimer::reset(VkCommandBuffer cb)
{
    vkCmdResetQueryPool(cb, queryPool_.get(), 0, maxScopes * 2);
    scopeCount_ = 0;
}

// ALL_COMMANDS: the timestamp is written once everything recorded before it has finished, so a scope covers
// exactly the work between its two timestamps instead of overlapping the previous pass.
uint32_t GpuTimer::begin(VkCommandBuffer cb, const char* name)
{
    assert(scopeCount_ < maxScopes);
    const uint32_t scope = scopeCount_++;
    names_[scope] = name;
    vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, queryPool_.get(), scope * 2);
    return scope;
}

void GpuTimer::end(VkCommandBuffer cb, uint32_t scope) const
{
    vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, queryPool_.get(), scope * 2 + 1);
}
