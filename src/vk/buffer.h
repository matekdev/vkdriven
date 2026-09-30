#pragma once

#include "vk/allocator.h"

#include <cstddef>
#include <ranges>
#include <span>

// A VkBuffer and its VMA allocation. If it's created with VMA_ALLOCATION_CREATE_MAPPED_BIT
// it stays mapped for its whole lifetime and write() copies straight into it.
class Buffer
{
  public:
    Buffer() = default;
    Buffer(const Allocator& allocator, VkDeviceSize size, VkBufferUsageFlags usage,
           VmaAllocationCreateFlags allocationFlags = 0);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    void reset();

    void write(std::span<const std::byte> bytes, VkDeviceSize offset = 0);

    template <std::ranges::contiguous_range Range> void write(const Range& data, VkDeviceSize offset = 0)
    {
        write(std::as_bytes(std::span{data}), offset);
    }

    [[nodiscard]] VkBuffer handle() const
    {
        return buffer_;
    }

    [[nodiscard]] VkDeviceSize size() const
    {
        return size_;
    }

    [[nodiscard]] VkDeviceAddress deviceAddress() const
    {
        return deviceAddress_;
    }

  private:
    VmaAllocator allocator_{VK_NULL_HANDLE};
    VkBuffer buffer_{VK_NULL_HANDLE};
    VmaAllocation allocation_{VK_NULL_HANDLE};
    VkDeviceSize size_{0};
    std::byte* mapped_{nullptr};
    VkDeviceAddress deviceAddress_{0};
};
