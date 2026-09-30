#pragma once

#include <volk.h>

#pragma warning(push, 0)
#include <vma/vk_mem_alloc.h>
#pragma warning(pop)

class Instance;
class Device;

// VMA allocator. Buffers and images get their memory from this, so they all
// need to be destroyed before it is.
class Allocator
{
  public:
    Allocator(const Instance& instance, const Device& device);
    ~Allocator();

    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;
    Allocator(Allocator&&) = delete;
    Allocator& operator=(Allocator&&) = delete;

    [[nodiscard]] VmaAllocator handle() const
    {
        return allocator_;
    }

  private:
    VmaAllocator allocator_{VK_NULL_HANDLE};
};
