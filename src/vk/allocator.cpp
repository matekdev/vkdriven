#define VMA_IMPLEMENTATION
#include "vk/allocator.h"

#include "vk/check.h"
#include "vk/device.h"
#include "vk/instance.h"

Allocator::Allocator(const Instance& instance, const Device& device)
{
    VmaVulkanFunctions vkFunctions{.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                                   .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
                                   .vkCreateImage = vkCreateImage};
    VmaAllocatorCreateInfo allocatorCI{.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
                                       .physicalDevice = device.physical(),
                                       .device = device.handle(),
                                       .pVulkanFunctions = &vkFunctions,
                                       .instance = instance.handle()};
    chk(vmaCreateAllocator(&allocatorCI, &allocator_));
}

Allocator::~Allocator()
{
    vmaDestroyAllocator(allocator_);
}
