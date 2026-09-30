#include "vk/buffer.h"

#include "vk/check.h"

#include <cassert>
#include <cstring>
#include <utility>

Buffer::Buffer(const Allocator& allocator, VkDeviceSize size, VkBufferUsageFlags usage,
               VmaAllocationCreateFlags allocationFlags)
    : allocator_{allocator.handle()}, size_{size}
{
    VkBufferCreateInfo bufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage};
    VmaAllocationCreateInfo allocationCI{.flags = allocationFlags, .usage = VMA_MEMORY_USAGE_AUTO};
    VmaAllocationInfo allocationInfo{};
    chk(vmaCreateBuffer(allocator_, &bufferCI, &allocationCI, &buffer_, &allocation_, &allocationInfo));
    mapped_ = static_cast<std::byte*>(allocationInfo.pMappedData);

    if (!(usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT))
        return;

    VmaAllocatorInfo allocatorInfo{};
    vmaGetAllocatorInfo(allocator_, &allocatorInfo);
    VkBufferDeviceAddressInfo addressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer_};
    deviceAddress_ = vkGetBufferDeviceAddress(allocatorInfo.device, &addressInfo);
}

Buffer::~Buffer()
{
    reset();
}

Buffer::Buffer(Buffer&& other) noexcept
    : allocator_{std::exchange(other.allocator_, VK_NULL_HANDLE)},
      buffer_{std::exchange(other.buffer_, VK_NULL_HANDLE)},
      allocation_{std::exchange(other.allocation_, VK_NULL_HANDLE)}, size_{std::exchange(other.size_, 0)},
      mapped_{std::exchange(other.mapped_, nullptr)}, deviceAddress_{std::exchange(other.deviceAddress_, 0)}
{
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
    if (this == &other)
        return *this;

    reset();
    allocator_ = std::exchange(other.allocator_, VK_NULL_HANDLE);
    buffer_ = std::exchange(other.buffer_, VK_NULL_HANDLE);
    allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
    size_ = std::exchange(other.size_, 0);
    mapped_ = std::exchange(other.mapped_, nullptr);
    deviceAddress_ = std::exchange(other.deviceAddress_, 0);
    return *this;
}

void Buffer::reset()
{
    if (buffer_ == VK_NULL_HANDLE)
        return;

    vmaDestroyBuffer(allocator_, buffer_, allocation_);
    buffer_ = VK_NULL_HANDLE;
    allocation_ = VK_NULL_HANDLE;
    size_ = 0;
    mapped_ = nullptr;
    deviceAddress_ = 0;
}

void Buffer::write(std::span<const std::byte> bytes, VkDeviceSize offset)
{
    assert(mapped_ != nullptr && "Buffer::write needs a buffer created with VMA_ALLOCATION_CREATE_MAPPED_BIT");
    assert(offset + bytes.size() <= size_);
    std::memcpy(mapped_ + offset, bytes.data(), bytes.size());
}
