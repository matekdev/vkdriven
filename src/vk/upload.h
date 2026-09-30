#pragma once

#include <volk.h>

#include "vk/buffer.h"

#include <cstddef>
#include <ranges>
#include <span>

class Allocator;
class CommandPool;

// Creates a device-local buffer (not CPU-visible) and fills it through a staging buffer.
// Blocks until the copy has finished, so it's meant for load time, not per frame.
Buffer uploadDeviceLocalBuffer(const Allocator& allocator, const CommandPool& commandPool,
                               std::span<const std::byte> data, VkBufferUsageFlags usage);

template <std::ranges::contiguous_range Range>
Buffer uploadDeviceLocalBuffer(const Allocator& allocator, const CommandPool& commandPool, const Range& data,
                               VkBufferUsageFlags usage)
{
    return uploadDeviceLocalBuffer(allocator, commandPool, std::as_bytes(std::span{data}), usage);
}
