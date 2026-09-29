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
#include <cstddef>
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

VmaAllocation vBufferAllocation{VK_NULL_HANDLE};
VkBuffer vBuffer{VK_NULL_HANDLE};

struct ShaderData
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 model[3];
    glm::vec4 lightPos{0.0f, -10.0f, 10.0f, 0.0f};
    uint32_t selected{1};
} shaderData{};

struct ShaderDataBuffer
{
    VmaAllocation allocation{VK_NULL_HANDLE};
    VmaAllocationInfo allocationInfo{};
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceAddress deviceAddress{};
};

constexpr uint32_t maxFramesInFlight = 2;
std::array<ShaderDataBuffer, maxFramesInFlight> shaderDataBuffers;
VkCommandPool commandPool{VK_NULL_HANDLE};
std::array<VkCommandBuffer, maxFramesInFlight> commandBuffers;
std::array<VkFence, maxFramesInFlight> fences;
std::array<VkSemaphore, maxFramesInFlight> imageAcquiredSemaphores;
std::vector<VkSemaphore> renderCompleteSemaphores;

struct Texture
{
    VmaAllocation allocation{VK_NULL_HANDLE};
    VkImage image{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
};
std::array<Texture, 3> textures{};
VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
VkDescriptorSetLayout descriptorSetLayoutTex{VK_NULL_HANDLE};
VkDescriptorSet descriptorSetTex{VK_NULL_HANDLE};

Slang::ComPtr<slang::IGlobalSession> slangGlobalSession;

VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
VkPipeline pipeline{VK_NULL_HANDLE};

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

static void chk(SlangResult result, std::source_location location = std::source_location::current())
{
    if (SLANG_SUCCEEDED(result))
        return;

    std::println(stderr, "Slang error {:#x} at {}:{}", static_cast<uint32_t>(result), location.file_name(),
                 location.line());
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
    if (surfaceCaps.currentExtent.width == 0xFFFFFFFF)
    {
        int pixelWidth = 0;
        int pixelHeight = 0;
        chk(SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight));
        swapchainExtent = {.width = static_cast<uint32_t>(pixelWidth), .height = static_cast<uint32_t>(pixelHeight)};
    }

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
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkImageViewCreateInfo viewCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = imageFormat,
            .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
        chk(vkCreateImageView(device, &viewCI, nullptr, &swapchainImageViews[i]));
    }

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
    if (depthFormat == VK_FORMAT_UNDEFINED)
    {
        std::println(stderr, "No supported depth/stencil format found");
        return 1;
    }

    VkImageCreateInfo depthImageCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthFormat,
        .extent{.width = swapchainExtent.width, .height = swapchainExtent.height, .depth = 1},
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

    const VkDeviceSize indexCount{shapes[0].mesh.indices.size()};
    std::vector<Vertex> vertices{};
    std::vector<uint16_t> indices{};
    for (auto& index : shapes[0].mesh.indices)
    {
        Vertex v{
            .pos = {attrib.vertices[index.vertex_index * 3], -attrib.vertices[index.vertex_index * 3 + 1],
                    attrib.vertices[index.vertex_index * 3 + 2]},
            .normal = {attrib.normals[index.normal_index * 3], -attrib.normals[index.normal_index * 3 + 1],
                       attrib.normals[index.normal_index * 3 + 2]},
            .uv = {attrib.texcoords[index.texcoord_index * 2], 1.0f - attrib.texcoords[index.texcoord_index * 2 + 1]}};
        vertices.push_back(v);
        indices.push_back(static_cast<uint16_t>(indices.size()));
    }

    // Create buffer data for gpu.
    VkDeviceSize vBufSize{sizeof(Vertex) * vertices.size()};
    VkDeviceSize iBufSize{sizeof(uint16_t) * indices.size()};
    VkBufferCreateInfo bufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                .size = vBufSize + iBufSize,
                                .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT};

    VmaAllocationCreateInfo vBufferAllocCI{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                                                    VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                           .usage = VMA_MEMORY_USAGE_AUTO};
    VmaAllocationInfo vBufferAllocInfo{};
    chk(vmaCreateBuffer(allocator, &bufferCI, &vBufferAllocCI, &vBuffer, &vBufferAllocation, &vBufferAllocInfo));

    memcpy(vBufferAllocInfo.pMappedData, vertices.data(), vBufSize);
    memcpy(((char*)vBufferAllocInfo.pMappedData) + vBufSize, indices.data(), iBufSize);

    // Shader data buffer setup.
    for (uint32_t i = 0; i < maxFramesInFlight; i++)
    {
        VkBufferCreateInfo uBufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                     .size = sizeof(ShaderData),
                                     .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT};
        VmaAllocationCreateInfo uBufferAllocCI{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                                        VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
                                                        VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                               .usage = VMA_MEMORY_USAGE_AUTO};
        chk(vmaCreateBuffer(allocator, &uBufferCI, &uBufferAllocCI, &shaderDataBuffers[i].buffer,
                            &shaderDataBuffers[i].allocation, &shaderDataBuffers[i].allocationInfo));
        VkBufferDeviceAddressInfo uBufferBdaInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                                 .buffer = shaderDataBuffers[i].buffer};
        shaderDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(device, &uBufferBdaInfo);
    }

    // Synchronization setup.
    VkSemaphoreCreateInfo semaphoreCI{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceCI{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    for (auto i = 0; i < maxFramesInFlight; i++)
    {
        chk(vkCreateFence(device, &fenceCI, nullptr, &fences[i]));
        chk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &imageAcquiredSemaphores[i]));
    }
    renderCompleteSemaphores.resize(swapchainImages.size());
    for (auto& semaphore : renderCompleteSemaphores)
    {
        chk(vkCreateSemaphore(device, &semaphoreCI, nullptr, &semaphore));
    }

    // Command buffers
    VkCommandPoolCreateInfo commandPoolCI{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                          .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                          .queueFamilyIndex = queueFamily};
    chk(vkCreateCommandPool(device, &commandPoolCI, nullptr, &commandPool));

    VkCommandBufferAllocateInfo cbAllocCI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                          .commandPool = commandPool,
                                          .commandBufferCount = maxFramesInFlight};
    chk(vkAllocateCommandBuffers(device, &cbAllocCI, commandBuffers.data()));

    // Texture loading.
    std::vector<VkDescriptorImageInfo> textureDescriptors{};
    for (size_t i = 0; i < textures.size(); i++)
    {
        ktxTexture* ktxTexture{nullptr};
        const std::string filename = "assets/suzanne" + std::to_string(i) + ".ktx";
        if (ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture) !=
            KTX_SUCCESS)
        {
            std::println(stderr, "Failed to load texture {}", filename);
            return 1;
        }

        VkImageCreateInfo texImgCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = ktxTexture_GetVkFormat(ktxTexture),
            .extent{.width = ktxTexture->baseWidth, .height = ktxTexture->baseHeight, .depth = 1},
            .mipLevels = ktxTexture->numLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
        VmaAllocationCreateInfo texImageAllocCI{.usage = VMA_MEMORY_USAGE_AUTO};
        chk(vmaCreateImage(allocator, &texImgCI, &texImageAllocCI, &textures[i].image, &textures[i].allocation,
                           nullptr));

        VkImageViewCreateInfo texViewCI{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                        .image = textures[i].image,
                                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                        .format = texImgCI.format,
                                        .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                          .levelCount = ktxTexture->numLevels,
                                                          .layerCount = 1}};
        chk(vkCreateImageView(device, &texViewCI, nullptr, &textures[i].view));

        // Upload through a host-visible staging buffer.
        VkBuffer imgSrcBuffer{VK_NULL_HANDLE};
        VmaAllocation imgSrcAllocation{VK_NULL_HANDLE};
        VkBufferCreateInfo imgSrcBufferCI{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                          .size = ktxTexture->dataSize,
                                          .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
        VmaAllocationCreateInfo imgSrcAllocCI{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                                       VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                              .usage = VMA_MEMORY_USAGE_AUTO};
        VmaAllocationInfo imgSrcAllocInfo{};
        chk(vmaCreateBuffer(allocator, &imgSrcBufferCI, &imgSrcAllocCI, &imgSrcBuffer, &imgSrcAllocation,
                            &imgSrcAllocInfo));
        memcpy(imgSrcAllocInfo.pMappedData, ktxTexture->pData, ktxTexture->dataSize);

        VkFenceCreateInfo fenceOneTimeCI{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence fenceOneTime{VK_NULL_HANDLE};
        chk(vkCreateFence(device, &fenceOneTimeCI, nullptr, &fenceOneTime));

        VkCommandBuffer cbOneTime{VK_NULL_HANDLE};
        VkCommandBufferAllocateInfo cbOneTimeAI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                .commandPool = commandPool,
                                                .commandBufferCount = 1};
        chk(vkAllocateCommandBuffers(device, &cbOneTimeAI, &cbOneTime));

        VkCommandBufferBeginInfo cbOneTimeBI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                             .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        chk(vkBeginCommandBuffer(cbOneTime, &cbOneTimeBI));

        VkImageMemoryBarrier2 barrierTexImage{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                              .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                                              .srcAccessMask = VK_ACCESS_2_NONE,
                                              .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                              .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                              .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                              .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                              .image = textures[i].image,
                                              .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                .levelCount = ktxTexture->numLevels,
                                                                .layerCount = 1}};
        VkDependencyInfo barrierTexInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                        .imageMemoryBarrierCount = 1,
                                        .pImageMemoryBarriers = &barrierTexImage};
        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);

        std::vector<VkBufferImageCopy> copyRegions{};
        for (uint32_t mip = 0; mip < ktxTexture->numLevels; mip++)
        {
            ktx_size_t mipOffset{0};
            if (ktxTexture_GetImageOffset(ktxTexture, mip, 0, 0, &mipOffset) != KTX_SUCCESS)
            {
                std::println(stderr, "Failed to get offset of mip {} in {}", mip, filename);
                return 1;
            }
            copyRegions.push_back({
                .bufferOffset = mipOffset,
                .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = mip, .layerCount = 1},
                .imageExtent{
                    .width = ktxTexture->baseWidth >> mip, .height = ktxTexture->baseHeight >> mip, .depth = 1},
            });
        }
        vkCmdCopyBufferToImage(cbOneTime, imgSrcBuffer, textures[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

        VkImageMemoryBarrier2 barrierTexRead{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                             .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                             .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                             .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                             .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                                             .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                             .image = textures[i].image,
                                             .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                               .levelCount = ktxTexture->numLevels,
                                                               .layerCount = 1}};
        barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
        chk(vkEndCommandBuffer(cbOneTime));

        VkCommandBufferSubmitInfo cbOneTimeSubmitInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                                      .commandBuffer = cbOneTime};
        VkSubmitInfo2 oneTimeSI{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                                .commandBufferInfoCount = 1,
                                .pCommandBufferInfos = &cbOneTimeSubmitInfo};
        chk(vkQueueSubmit2(queue, 1, &oneTimeSI, fenceOneTime));
        chk(vkWaitForFences(device, 1, &fenceOneTime, VK_TRUE, UINT64_MAX));
        vkDestroyFence(device, fenceOneTime, nullptr);
        vkFreeCommandBuffers(device, commandPool, 1, &cbOneTime);
        vmaDestroyBuffer(allocator, imgSrcBuffer, imgSrcAllocation);

        VkSamplerCreateInfo samplerCI{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                      .magFilter = VK_FILTER_LINEAR,
                                      .minFilter = VK_FILTER_LINEAR,
                                      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                      .anisotropyEnable = VK_TRUE,
                                      .maxAnisotropy = 8.0f,
                                      .maxLod = static_cast<float>(ktxTexture->numLevels)};
        chk(vkCreateSampler(device, &samplerCI, nullptr, &textures[i].sampler));

        ktxTexture_Destroy(ktxTexture);
        textureDescriptors.push_back({.sampler = textures[i].sampler,
                                      .imageView = textures[i].view,
                                      .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL});
    }

    // Descriptor indexing.
    VkDescriptorBindingFlags descVariableFlag{VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &descVariableFlag};
    VkDescriptorSetLayoutBinding descLayoutBindingTex{.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                      .descriptorCount = static_cast<uint32_t>(textures.size()),
                                                      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo descLayoutTexCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                    .pNext = &descBindingFlags,
                                                    .bindingCount = 1,
                                                    .pBindings = &descLayoutBindingTex};
    chk(vkCreateDescriptorSetLayout(device, &descLayoutTexCI, nullptr, &descriptorSetLayoutTex));

    VkDescriptorPoolSize poolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  .descriptorCount = static_cast<uint32_t>(textures.size())};
    VkDescriptorPoolCreateInfo descPoolCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                          .maxSets = 1,
                                          .poolSizeCount = 1,
                                          .pPoolSizes = &poolSize};
    chk(vkCreateDescriptorPool(device, &descPoolCI, nullptr, &descriptorPool));

    uint32_t variableDescCount{static_cast<uint32_t>(textures.size())};
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &variableDescCount};
    VkDescriptorSetAllocateInfo texDescSetAlloc{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                .pNext = &variableDescCountAI,
                                                .descriptorPool = descriptorPool,
                                                .descriptorSetCount = 1,
                                                .pSetLayouts = &descriptorSetLayoutTex};
    chk(vkAllocateDescriptorSets(device, &texDescSetAlloc, &descriptorSetTex));

    VkWriteDescriptorSet writeDescSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                      .dstSet = descriptorSetTex,
                                      .dstBinding = 0,
                                      .descriptorCount = static_cast<uint32_t>(textureDescriptors.size()),
                                      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      .pImageInfo = textureDescriptors.data()};
    vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);

    // Slang compiler setup.
    chk(slang::createGlobalSession(slangGlobalSession.writeRef()));
    auto slangTargets{std::to_array<slang::TargetDesc>(
        {{.format{SLANG_SPIRV}, .profile{slangGlobalSession->findProfile("spirv_1_4")}}})};
    auto slangOptions{std::to_array<slang::CompilerOptionEntry>(
        {{slang::CompilerOptionName::EmitSpirvDirectly, {slang::CompilerOptionValueKind::Int, 1}}})};
    slang::SessionDesc slangSessionDesc{.targets{slangTargets.data()},
                                        .targetCount{SlangInt(slangTargets.size())},
                                        .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                                        .compilerOptionEntries{slangOptions.data()},
                                        .compilerOptionEntryCount{uint32_t(slangOptions.size())}};

    // Shader loading.
    Slang::ComPtr<slang::ISession> slangSession;
    chk(slangGlobalSession->createSession(slangSessionDesc, slangSession.writeRef()));
    Slang::ComPtr<slang::IBlob> slangDiagnostics;
    Slang::ComPtr<slang::IModule> slangModule{
        slangSession->loadModuleFromSource("shader", "shaders/shader.slang", nullptr, slangDiagnostics.writeRef())};
    if (!slangModule)
    {
        std::println(stderr, "Failed to compile shaders/shader.slang:\n{}",
                     slangDiagnostics ? static_cast<const char*>(slangDiagnostics->getBufferPointer()) : "");
        return 1;
    }
    Slang::ComPtr<ISlangBlob> spirv;
    chk(slangModule->getTargetCode(0, spirv.writeRef()));

    VkShaderModuleCreateInfo shaderModuleCI{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                            .codeSize = spirv->getBufferSize(),
                                            .pCode = static_cast<const uint32_t*>(spirv->getBufferPointer())};
    VkShaderModule shaderModule{VK_NULL_HANDLE};
    chk(vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shaderModule));

    // Graphics pipeline.
    VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(VkDeviceAddress)};
    VkPipelineLayoutCreateInfo pipelineLayoutCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount = 1,
                                                .pSetLayouts = &descriptorSetLayoutTex,
                                                .pushConstantRangeCount = 1,
                                                .pPushConstantRanges = &pushConstantRange};
    chk(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    const auto shaderStages = std::to_array<VkPipelineShaderStageCreateInfo>({
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = shaderModule,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = shaderModule,
         .pName = "main"},
    });

    VkVertexInputBindingDescription vertexBinding{
        .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    const auto vertexAttributes = std::to_array<VkVertexInputAttributeDescription>({
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, pos)},
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, normal)},
        {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, uv)},
    });
    VkPipelineVertexInputStateCreateInfo vertexInputState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexBinding,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
        .pVertexAttributeDescriptions = vertexAttributes.data()};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    const auto dynamicStates = std::to_array<VkDynamicState>({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
    VkPipelineDynamicStateCreateInfo dynamicState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                  .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                  .pDynamicStates = dynamicStates.data()};
    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};

    VkPipelineRasterizationStateCreateInfo rasterizationState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .lineWidth = 1.0f};
    VkPipelineMultisampleStateCreateInfo multisampleState{.sType =
                                                              VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                          .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo depthStencilState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL};
    VkPipelineColorBlendAttachmentState blendAttachment{.colorWriteMask =
                                                            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo colorBlendState{.sType =
                                                            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                        .attachmentCount = 1,
                                                        .pAttachments = &blendAttachment};

    VkPipelineRenderingCreateInfo renderingCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                              .colorAttachmentCount = 1,
                                              .pColorAttachmentFormats = &imageFormat,
                                              .depthAttachmentFormat = depthFormat};
    VkGraphicsPipelineCreateInfo pipelineCI{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                            .pNext = &renderingCI,
                                            .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                            .pStages = shaderStages.data(),
                                            .pVertexInputState = &vertexInputState,
                                            .pInputAssemblyState = &inputAssemblyState,
                                            .pViewportState = &viewportState,
                                            .pRasterizationState = &rasterizationState,
                                            .pMultisampleState = &multisampleState,
                                            .pDepthStencilState = &depthStencilState,
                                            .pColorBlendState = &colorBlendState,
                                            .pDynamicState = &dynamicState,
                                            .layout = pipelineLayout};
    chk(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));

    uint64_t lastTime{SDL_GetTicks()};
    bool quit{false};
    while (!quit)
    {
        // Wait on fence
        // Acquire next image
        // Update shader data
        // Record command buffer
        // Submit command buffer
        // Present image
        // Poll events
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
