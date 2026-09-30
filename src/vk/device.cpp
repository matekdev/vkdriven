#include "vk/device.h"

#include "vk/check.h"
#include "vk/instance.h"
#include "vk/surface.h"

#include <array>
#include <cstdlib>
#include <optional>
#include <print>
#include <vector>

namespace
{

struct PhysicalDeviceChoice
{
    VkPhysicalDevice physical{VK_NULL_HANDLE};
    uint32_t queueFamily{0};
};

std::optional<uint32_t> findGraphicsPresentQueueFamily(VkPhysicalDevice physical, VkSurfaceKHR surface)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        if (!(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            continue;

        VkBool32 presentSupported = VK_FALSE;
        chk(vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, surface, &presentSupported));
        if (presentSupported)
            return i;
    }
    return std::nullopt;
}

std::optional<PhysicalDeviceChoice> pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
    uint32_t deviceCount = 0;
    chk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
    std::vector<VkPhysicalDevice> devices(deviceCount);
    chk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    std::optional<PhysicalDeviceChoice> choice;
    for (VkPhysicalDevice candidate : devices)
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(candidate, &properties);
        if (properties.apiVersion < VK_API_VERSION_1_3)
            continue;

        const auto queueFamily = findGraphicsPresentQueueFamily(candidate, surface);
        if (!queueFamily)
            continue;

        const bool isDiscrete = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        if (!choice || isDiscrete)
            choice = PhysicalDeviceChoice{.physical = candidate, .queueFamily = *queueFamily};
        if (isDiscrete)
            break;
    }
    return choice;
}

} // namespace

Device::Device(const Instance& instance, const Surface& surface)
{
    const auto choice = pickPhysicalDevice(instance.handle(), surface.handle());
    if (!choice)
    {
        std::println(stderr, "No GPU with Vulkan 1.3 and a graphics queue that can present to the window");
        std::abort();
    }
    physical_ = choice->physical;
    queueFamily_ = choice->queueFamily;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical_, &properties);
    std::println("Using GPU: {} ({}, vendor: {}, device: {})", properties.deviceName,
                 string_VkPhysicalDeviceType(properties.deviceType), properties.vendorID, properties.deviceID);

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCI{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                    .queueFamilyIndex = queueFamily_,
                                    .queueCount = 1,
                                    .pQueuePriorities = &queuePriority};

    const auto deviceExtensions = std::to_array<const char*>({VK_KHR_SWAPCHAIN_EXTENSION_NAME});

    VkPhysicalDeviceVulkan12Features enabledVk12Features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                                                         .descriptorIndexing = true,
                                                         .shaderSampledImageArrayNonUniformIndexing = true,
                                                         .descriptorBindingVariableDescriptorCount = true,
                                                         .runtimeDescriptorArray = true,
                                                         .bufferDeviceAddress = true};
    VkPhysicalDeviceVulkan13Features enabledVk13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &enabledVk12Features,
        .synchronization2 = true,
        .dynamicRendering = true,
    };
    VkPhysicalDeviceFeatures enabledVk10Features{.samplerAnisotropy = VK_TRUE};

    VkDeviceCreateInfo deviceCI{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                .pNext = &enabledVk13Features,
                                .queueCreateInfoCount = 1,
                                .pQueueCreateInfos = &queueCI,
                                .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
                                .ppEnabledExtensionNames = deviceExtensions.data(),
                                .pEnabledFeatures = &enabledVk10Features};
    chk(vkCreateDevice(physical_, &deviceCI, nullptr, &device_));
    volkLoadDevice(device_);
    vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
}

Device::~Device()
{
    vkDestroyDevice(device_, nullptr);
}

void Device::waitIdle() const
{
    chk(vkDeviceWaitIdle(device_));
}
