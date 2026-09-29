#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

#pragma warning(push, 0)
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#pragma warning(pop)

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <slang-com-ptr.h>
#include <slang.h>

#include <ktx.h>
#include <ktxvulkan.h>

#include <tiny_obj_loader.h>

#include <vulkan/vk_enum_string_helper.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <source_location>
#include <string>
#include <vector>

VkDevice device{VK_NULL_HANDLE};
VkQueue queue{VK_NULL_HANDLE};
VmaAllocator allocator{VK_NULL_HANDLE};
VkSurfaceKHR surface{VK_NULL_HANDLE};
VkSwapchainKHR swapchain{VK_NULL_HANDLE};
std::vector<VkImage> swapchainImages;
std::vector<VkImageView> swapchainImageViews;

VkImage depthImage{VK_NULL_HANDLE};
VmaAllocation depthImageAllocation{VK_NULL_HANDLE};
VkImageView depthImageView{VK_NULL_HANDLE};

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

static void chk(VkResult result, std::source_location location = std::source_location::current())
{
    if (result >= VK_SUCCESS)
        return;

    std::println(stderr, "Vulkan error {} at {}:{}", string_VkResult(result), location.file_name(), location.line());
    std::abort();
}

static void chk(bool result, std::source_location location = std::source_location::current())
{
    if (result)
        return;

    std::println(stderr, "SDL error \"{}\" at {}:{}", SDL_GetError(), location.file_name(), location.line());
    std::abort();
}

int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::println(stderr, "SDL_Init failed: {}", SDL_GetError());
        return 1;
    }

    chk(SDL_Vulkan_LoadLibrary(nullptr));
    chk(volkInitialize());

    // Setup vulkan instance.
    auto appInfo = VkApplicationInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "vkdriven",
        .apiVersion = VK_API_VERSION_1_3,
    };

    uint32_t instanceExtensionsCount = 0;
    char const* const* instanceExtensions{SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount)};

    auto instanceCreateInfo = VkInstanceCreateInfo{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                                   .pApplicationInfo = &appInfo,
                                                   .enabledExtensionCount = instanceExtensionsCount,
                                                   .ppEnabledExtensionNames = instanceExtensions};
    VkInstance instance;
    chk(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));
    volkLoadInstance(instance);

    // Choose a physical device.
    uint32_t deviceCount = 0;
    chk(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr));
    std::vector<VkPhysicalDevice> devices(deviceCount);
    chk(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    // Device information
    constexpr int deviceIndex = 0; // hardcoded to use my GPU for now...
    auto deviceProperties = VkPhysicalDeviceProperties2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };
    vkGetPhysicalDeviceProperties2(devices[deviceIndex], &deviceProperties);
    std::println("Using GPU: {} (vendor: {}, device: {})", deviceProperties.properties.deviceName,
                 deviceProperties.properties.vendorID, deviceProperties.properties.deviceID);

    // Find a queue family that supports graphics and presentation.
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIndex], &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIndex], &queueFamilyCount, queueFamilies.data());

    uint32_t queueFamily = 0;
    for (uint32_t i = 0; i < queueFamilies.size(); ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queueFamily = i;
            break;
        }
    }
    chk(SDL_Vulkan_GetPresentationSupport(instance, devices[deviceIndex], queueFamily));

    const auto queuePriority = 1.0f;
    auto queueInfo = VkDeviceQueueCreateInfo{.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                             .queueFamilyIndex = queueFamily,
                                             .queueCount = 1,
                                             .pQueuePriorities = &queuePriority};

    // Device setup.
    const auto deviceExtensions = std::array<const char*, 1>{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

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
                                .pQueueCreateInfos = &queueInfo,
                                .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
                                .ppEnabledExtensionNames = deviceExtensions.data(),
                                .pEnabledFeatures = &enabledVk10Features};
    chk(vkCreateDevice(devices[deviceIndex], &deviceCI, nullptr, &device));
    volkLoadDevice(device);
    vkGetDeviceQueue(device, queueFamily, 0, &queue);

    // Setup VMA (Vulkan Memory Allocator).
    VmaVulkanFunctions vkFunctions{.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                                   .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
                                   .vkCreateImage = vkCreateImage};
    VmaAllocatorCreateInfo allocatorCI{.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
                                       .physicalDevice = devices[deviceIndex],
                                       .device = device,
                                       .pVulkanFunctions = &vkFunctions,
                                       .instance = instance};
    chk(vmaCreateAllocator(&allocatorCI, &allocator));

    constexpr auto windowWidth = 1280;
    constexpr auto windowHeight = 720;
    constexpr auto windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    auto* const window = SDL_CreateWindow("vkdriven", windowWidth, windowHeight, windowFlags);
    if (!window)
    {
        std::println(stderr, "SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    chk(SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface));

    // Query surface capabilities.
    VkSurfaceCapabilitiesKHR surfaceCaps{};
    chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[deviceIndex], surface, &surfaceCaps));

    // Swapchain setup.
    VkExtent2D swapchainExtent{surfaceCaps.currentExtent};
    swapchainExtent = {.width = static_cast<uint32_t>(windowWidth), .height = static_cast<uint32_t>(windowHeight)};

    const VkFormat imageFormat{VK_FORMAT_B8G8R8A8_SRGB};
    VkSwapchainCreateInfoKHR swapchainCI{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                         .surface = surface,
                                         .minImageCount = surfaceCaps.minImageCount,
                                         .imageFormat = imageFormat,
                                         .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                                         .imageExtent{.width = swapchainExtent.width, .height = swapchainExtent.height},
                                         .imageArrayLayers = 1,
                                         .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                         .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                                         .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                         .presentMode = VK_PRESENT_MODE_FIFO_KHR};
    chk(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapchain));

    uint32_t imageCount{0};
    chk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
    swapchainImages.resize(imageCount);
    chk(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()));
    swapchainImageViews.resize(imageCount);

    // Depth attachment setup.
    std::vector<VkFormat> depthFormatList{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    VkFormat depthFormat{VK_FORMAT_UNDEFINED};
    for (VkFormat& format : depthFormatList)
    {
        VkFormatProperties2 formatProperties{.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
        vkGetPhysicalDeviceFormatProperties2(devices[deviceIndex], format, &formatProperties);
        if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depthFormat = format;
            break;
        }
    }

    VkImageCreateInfo depthImageCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthFormat,
        .extent{.width = static_cast<uint32_t>(windowWidth), .height = static_cast<uint32_t>(windowHeight), .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocCI{.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                                    .usage = VMA_MEMORY_USAGE_AUTO};
    chk(vmaCreateImage(allocator, &depthImageCI, &allocCI, &depthImage, &depthImageAllocation, nullptr));

    VkImageViewCreateInfo depthViewCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depthFormat,
        .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1}};
    chk(vkCreateImageView(device, &depthViewCI, nullptr, &depthImageView));

    // Model loading.
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    chk(tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, nullptr, "assets/suzanne.obj"));

    auto running = true;
    while (running)
    {
        auto event = SDL_Event{};
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
        }

        SDL_Delay(16);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
