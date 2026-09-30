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
#include <ktx.h>
#include <ktxvulkan.h>
#include <slang-com-ptr.h>
#include <slang.h>
#include <tiny_obj_loader.h>

#include "platform/window.h"
#include "vk/check.h"
#include "vk/device.h"
#include "vk/handle.h"
#include "vk/instance.h"
#include "vk/surface.h"
#include "vk/sync.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <string>
#include <vector>

VmaAllocator allocator{VK_NULL_HANDLE};
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

int main(int, char**)
{
    const Window window{"vkdriven", 1280, 720};

    const Instance instance{"vkdriven", window.requiredInstanceExtensions()};
    const Surface surface{instance, window};

    const Device device{instance, surface};

    // Setup VMA (Vulkan Memory Allocator).
    VmaVulkanFunctions vkFunctions{.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                                   .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
                                   .vkCreateImage = vkCreateImage};
    VmaAllocatorCreateInfo allocatorCI{.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
                                       .physicalDevice = device.physical(),
                                       .device = device.handle(),
                                       .pVulkanFunctions = &vkFunctions,
                                       .instance = instance.handle()};
    chk(vmaCreateAllocator(&allocatorCI, &allocator));

    // Query surface capabilities.
    VkSurfaceCapabilitiesKHR surfaceCaps{};
    chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical(), surface.handle(), &surfaceCaps));

    // Swapchain setup.
    VkExtent2D swapchainExtent{surfaceCaps.currentExtent};
    if (surfaceCaps.currentExtent.width == 0xFFFFFFFF)
    {
        swapchainExtent = window.sizeInPixels();
    }

    const VkFormat imageFormat{VK_FORMAT_B8G8R8A8_SRGB};
    VkSwapchainCreateInfoKHR swapchainCI{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                         .surface = surface.handle(),
                                         .minImageCount = surfaceCaps.minImageCount,
                                         .imageFormat = imageFormat,
                                         .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                                         .imageExtent{.width = swapchainExtent.width, .height = swapchainExtent.height},
                                         .imageArrayLayers = 1,
                                         .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                         .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                                         .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                         .presentMode = VK_PRESENT_MODE_FIFO_KHR};
    chk(vkCreateSwapchainKHR(device.handle(), &swapchainCI, nullptr, &swapchain));

    uint32_t imageCount{0};
    chk(vkGetSwapchainImagesKHR(device.handle(), swapchain, &imageCount, nullptr));
    swapchainImages.resize(imageCount);
    chk(vkGetSwapchainImagesKHR(device.handle(), swapchain, &imageCount, swapchainImages.data()));
    swapchainImageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkImageViewCreateInfo viewCI{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = imageFormat,
            .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
        chk(vkCreateImageView(device.handle(), &viewCI, nullptr, &swapchainImageViews[i]));
    }

    // Depth attachment setup.
    std::vector<VkFormat> depthFormatList{VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    VkFormat depthFormat{VK_FORMAT_UNDEFINED};
    for (VkFormat& format : depthFormatList)
    {
        VkFormatProperties2 formatProperties{.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
        vkGetPhysicalDeviceFormatProperties2(device.physical(), format, &formatProperties);
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
    chk(vkCreateImageView(device.handle(), &depthViewCI, nullptr, &depthImageView));

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
        shaderDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(device.handle(), &uBufferBdaInfo);
    }

    // Synchronization setup.
    std::array<DeviceHandle<VkFence>, maxFramesInFlight> fences;
    std::array<DeviceHandle<VkSemaphore>, maxFramesInFlight> imageAcquiredSemaphores;
    for (uint32_t i = 0; i < maxFramesInFlight; i++)
    {
        fences[i] = createFence(device.handle(), VK_FENCE_CREATE_SIGNALED_BIT);
        imageAcquiredSemaphores[i] = createSemaphore(device.handle());
    }
    std::vector<DeviceHandle<VkSemaphore>> renderCompleteSemaphores;
    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        renderCompleteSemaphores.push_back(createSemaphore(device.handle()));
    }

    // Command buffers
    VkCommandPoolCreateInfo commandPoolCI{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                          .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                          .queueFamilyIndex = device.queueFamily()};
    chk(vkCreateCommandPool(device.handle(), &commandPoolCI, nullptr, &commandPool));

    VkCommandBufferAllocateInfo cbAllocCI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                          .commandPool = commandPool,
                                          .commandBufferCount = maxFramesInFlight};
    chk(vkAllocateCommandBuffers(device.handle(), &cbAllocCI, commandBuffers.data()));

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
        chk(vkCreateImageView(device.handle(), &texViewCI, nullptr, &textures[i].view));

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

        const auto fenceOneTime = createFence(device.handle());

        VkCommandBuffer cbOneTime{VK_NULL_HANDLE};
        VkCommandBufferAllocateInfo cbOneTimeAI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                .commandPool = commandPool,
                                                .commandBufferCount = 1};
        chk(vkAllocateCommandBuffers(device.handle(), &cbOneTimeAI, &cbOneTime));

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
        chk(vkQueueSubmit2(device.queue(), 1, &oneTimeSI, fenceOneTime.get()));
        chk(vkWaitForFences(device.handle(), 1, fenceOneTime.ptr(), VK_TRUE, UINT64_MAX));
        vkFreeCommandBuffers(device.handle(), commandPool, 1, &cbOneTime);
        vmaDestroyBuffer(allocator, imgSrcBuffer, imgSrcAllocation);

        VkSamplerCreateInfo samplerCI{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                      .magFilter = VK_FILTER_LINEAR,
                                      .minFilter = VK_FILTER_LINEAR,
                                      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                      .anisotropyEnable = VK_TRUE,
                                      .maxAnisotropy = 8.0f,
                                      .maxLod = static_cast<float>(ktxTexture->numLevels)};
        chk(vkCreateSampler(device.handle(), &samplerCI, nullptr, &textures[i].sampler));

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
    chk(vkCreateDescriptorSetLayout(device.handle(), &descLayoutTexCI, nullptr, &descriptorSetLayoutTex));

    VkDescriptorPoolSize poolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  .descriptorCount = static_cast<uint32_t>(textures.size())};
    VkDescriptorPoolCreateInfo descPoolCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                          .maxSets = 1,
                                          .poolSizeCount = 1,
                                          .pPoolSizes = &poolSize};
    chk(vkCreateDescriptorPool(device.handle(), &descPoolCI, nullptr, &descriptorPool));

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
    chk(vkAllocateDescriptorSets(device.handle(), &texDescSetAlloc, &descriptorSetTex));

    VkWriteDescriptorSet writeDescSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                      .dstSet = descriptorSetTex,
                                      .dstBinding = 0,
                                      .descriptorCount = static_cast<uint32_t>(textureDescriptors.size()),
                                      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      .pImageInfo = textureDescriptors.data()};
    vkUpdateDescriptorSets(device.handle(), 1, &writeDescSet, 0, nullptr);

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
    chk(vkCreateShaderModule(device.handle(), &shaderModuleCI, nullptr, &shaderModule));

    // Graphics pipeline.
    VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(VkDeviceAddress)};
    VkPipelineLayoutCreateInfo pipelineLayoutCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount = 1,
                                                .pSetLayouts = &descriptorSetLayoutTex,
                                                .pushConstantRangeCount = 1,
                                                .pPushConstantRanges = &pushConstantRange};
    chk(vkCreatePipelineLayout(device.handle(), &pipelineLayoutCI, nullptr, &pipelineLayout));

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
    chk(vkCreateGraphicsPipelines(device.handle(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));

    uint32_t frameIndex{0};
    uint32_t imageIndex{0};
    bool updateSwapchain{false};
    glm::vec3 camPos{0.0f, 0.0f, -6.0f};
    std::array<glm::vec3, 3> objectRotations{};

    uint64_t lastTime{SDL_GetTicks()};
    bool quit{false};
    while (!quit)
    {
        // Poll events
        const float elapsedTime{static_cast<float>(SDL_GetTicks() - lastTime) / 1000.0f};
        lastTime = SDL_GetTicks();
        for (SDL_Event event; SDL_PollEvent(&event);)
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                quit = true;
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION && (event.motion.state & SDL_BUTTON_LMASK))
            {
                objectRotations[shaderData.selected].x -= event.motion.yrel * elapsedTime;
                objectRotations[shaderData.selected].y += event.motion.xrel * elapsedTime;
            }
            if (event.type == SDL_EVENT_MOUSE_WHEEL)
            {
                camPos.z += event.wheel.y * elapsedTime * 10.0f;
            }
            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                if (event.key.key == SDLK_PLUS || event.key.key == SDLK_KP_PLUS)
                {
                    shaderData.selected = (shaderData.selected + 1) % 3;
                }
                if (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS)
                {
                    shaderData.selected = (shaderData.selected + 2) % 3;
                }
            }
            if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                updateSwapchain = true;
            }
        }
        if (quit)
        {
            break;
        }

        // Recreate swapchain
        if (updateSwapchain)
        {
            chk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physical(), surface.handle(), &surfaceCaps));
            swapchainExtent = surfaceCaps.currentExtent;
            if (surfaceCaps.currentExtent.width == 0xFFFFFFFF)
            {
                swapchainExtent = window.sizeInPixels();
            }
            if (swapchainExtent.width == 0 || swapchainExtent.height == 0)
            {
                SDL_WaitEvent(nullptr);
                continue;
            }
            updateSwapchain = false;
            device.waitIdle();

            swapchainCI.oldSwapchain = swapchain;
            swapchainCI.imageExtent = swapchainExtent;
            chk(vkCreateSwapchainKHR(device.handle(), &swapchainCI, nullptr, &swapchain));
            vkDestroySwapchainKHR(device.handle(), swapchainCI.oldSwapchain, nullptr);

            for (auto view : swapchainImageViews)
            {
                vkDestroyImageView(device.handle(), view, nullptr);
            }
            chk(vkGetSwapchainImagesKHR(device.handle(), swapchain, &imageCount, nullptr));
            swapchainImages.resize(imageCount);
            chk(vkGetSwapchainImagesKHR(device.handle(), swapchain, &imageCount, swapchainImages.data()));
            swapchainImageViews.resize(imageCount);
            for (uint32_t i = 0; i < imageCount; ++i)
            {
                VkImageViewCreateInfo viewCI{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = swapchainImages[i],
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = imageFormat,
                    .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
                chk(vkCreateImageView(device.handle(), &viewCI, nullptr, &swapchainImageViews[i]));
            }

            renderCompleteSemaphores.clear();
            for (uint32_t i = 0; i < imageCount; i++)
            {
                renderCompleteSemaphores.push_back(createSemaphore(device.handle()));
            }

            vkDestroyImageView(device.handle(), depthImageView, nullptr);
            vmaDestroyImage(allocator, depthImage, depthImageAllocation);
            depthImageCI.extent = {.width = swapchainExtent.width, .height = swapchainExtent.height, .depth = 1};
            chk(vmaCreateImage(allocator, &depthImageCI, &allocCI, &depthImage, &depthImageAllocation, nullptr));
            depthViewCI.image = depthImage;
            chk(vkCreateImageView(device.handle(), &depthViewCI, nullptr, &depthImageView));
        }

        // Wait on fence
        chk(vkWaitForFences(device.handle(), 1, fences[frameIndex].ptr(), VK_TRUE, UINT64_MAX));

        // Acquire next image
        const VkResult acquireResult =
            vkAcquireNextImageKHR(device.handle(), swapchain, UINT64_MAX, imageAcquiredSemaphores[frameIndex].get(),
                                  VK_NULL_HANDLE, &imageIndex);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            updateSwapchain = true;
            continue;
        }
        chk(acquireResult);
        if (acquireResult == VK_SUBOPTIMAL_KHR)
        {
            updateSwapchain = true;
        }
        chk(vkResetFences(device.handle(), 1, fences[frameIndex].ptr()));

        // Update shader data
        const float aspect{static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height)};
        shaderData.projection = glm::perspective(glm::radians(45.0f), aspect, 32.0f, 0.1f);
        shaderData.view = glm::translate(glm::mat4(1.0f), camPos);
        for (size_t i = 0; i < objectRotations.size(); i++)
        {
            const glm::vec3 instancePos{(static_cast<float>(i) - 1.0f) * 3.0f, 0.0f, 0.0f};
            shaderData.model[i] =
                glm::translate(glm::mat4(1.0f), instancePos) * glm::mat4_cast(glm::quat(objectRotations[i]));
        }
        memcpy(shaderDataBuffers[frameIndex].allocationInfo.pMappedData, &shaderData, sizeof(ShaderData));

        // Record command buffer
        VkCommandBuffer cb = commandBuffers[frameIndex];
        chk(vkResetCommandBuffer(cb, 0));
        VkCommandBufferBeginInfo cbBI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
        chk(vkBeginCommandBuffer(cb, &cbBI));

        const auto outputBarriers = std::to_array<VkImageMemoryBarrier2>({
            {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
             .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
             .srcAccessMask = VK_ACCESS_2_NONE,
             .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
             .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
             .image = swapchainImages[imageIndex],
             .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}},
            {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
             .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
             .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
             .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
             .dstAccessMask =
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
             .image = depthImage,
             .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                               .levelCount = 1,
                               .layerCount = 1}},
        });
        VkDependencyInfo outputDependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                              .imageMemoryBarrierCount = static_cast<uint32_t>(outputBarriers.size()),
                                              .pImageMemoryBarriers = outputBarriers.data()};
        vkCmdPipelineBarrier2(cb, &outputDependencyInfo);

        VkRenderingAttachmentInfo colorAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                      .imageView = swapchainImageViews[imageIndex],
                                                      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                      .clearValue{.color{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderingAttachmentInfo depthAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                      .imageView = depthImageView,
                                                      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                      .clearValue{.depthStencil{.depth = 0.0f, .stencil = 0}}};
        VkRenderingInfo renderingInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                      .renderArea{.extent = swapchainExtent},
                                      .layerCount = 1,
                                      .colorAttachmentCount = 1,
                                      .pColorAttachments = &colorAttachmentInfo,
                                      .pDepthAttachment = &depthAttachmentInfo};
        vkCmdBeginRendering(cb, &renderingInfo);

        VkViewport viewport{.width = static_cast<float>(swapchainExtent.width),
                            .height = static_cast<float>(swapchainExtent.height),
                            .minDepth = 0.0f,
                            .maxDepth = 1.0f};
        vkCmdSetViewport(cb, 0, 1, &viewport);
        VkRect2D scissor{.extent = swapchainExtent};
        vkCmdSetScissor(cb, 0, 1, &scissor);

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSetTex, 0,
                                nullptr);
        VkDeviceSize vOffset{0};
        vkCmdBindVertexBuffers(cb, 0, 1, &vBuffer, &vOffset);
        vkCmdBindIndexBuffer(cb, vBuffer, vBufSize, VK_INDEX_TYPE_UINT16);
        vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress),
                           &shaderDataBuffers[frameIndex].deviceAddress);
        vkCmdDrawIndexed(cb, static_cast<uint32_t>(indexCount), 3, 0, 0, 0);
        vkCmdEndRendering(cb);

        VkImageMemoryBarrier2 barrierPresent{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_NONE,
            .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = swapchainImages[imageIndex],
            .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
        VkDependencyInfo presentDependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                               .imageMemoryBarrierCount = 1,
                                               .pImageMemoryBarriers = &barrierPresent};
        vkCmdPipelineBarrier2(cb, &presentDependencyInfo);
        chk(vkEndCommandBuffer(cb));

        // Submit command buffer
        VkSemaphoreSubmitInfo waitSemaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                                .semaphore = imageAcquiredSemaphores[frameIndex].get(),
                                                .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkCommandBufferSubmitInfo commandBufferSubmitInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                                          .commandBuffer = cb};
        VkSemaphoreSubmitInfo signalSemaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                                  .semaphore = renderCompleteSemaphores[imageIndex].get(),
                                                  .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo2 submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                                 .waitSemaphoreInfoCount = 1,
                                 .pWaitSemaphoreInfos = &waitSemaphoreInfo,
                                 .commandBufferInfoCount = 1,
                                 .pCommandBufferInfos = &commandBufferSubmitInfo,
                                 .signalSemaphoreInfoCount = 1,
                                 .pSignalSemaphoreInfos = &signalSemaphoreInfo};
        chk(vkQueueSubmit2(device.queue(), 1, &submitInfo, fences[frameIndex].get()));
        frameIndex = (frameIndex + 1) % maxFramesInFlight;

        // Present image
        VkPresentInfoKHR presentInfo{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                     .waitSemaphoreCount = 1,
                                     .pWaitSemaphores = renderCompleteSemaphores[imageIndex].ptr(),
                                     .swapchainCount = 1,
                                     .pSwapchains = &swapchain,
                                     .pImageIndices = &imageIndex};
        const VkResult presentResult = vkQueuePresentKHR(device.queue(), &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
        {
            updateSwapchain = true;
        }
        else
        {
            chk(presentResult);
        }
    }

    // Cleaning up
    device.waitIdle();
    for (uint32_t i = 0; i < maxFramesInFlight; i++)
    {
        vmaDestroyBuffer(allocator, shaderDataBuffers[i].buffer, shaderDataBuffers[i].allocation);
    }
    vkDestroyImageView(device.handle(), depthImageView, nullptr);
    vmaDestroyImage(allocator, depthImage, depthImageAllocation);
    for (auto view : swapchainImageViews)
    {
        vkDestroyImageView(device.handle(), view, nullptr);
    }
    vmaDestroyBuffer(allocator, vBuffer, vBufferAllocation);
    for (auto& texture : textures)
    {
        vkDestroyImageView(device.handle(), texture.view, nullptr);
        vkDestroySampler(device.handle(), texture.sampler, nullptr);
        vmaDestroyImage(allocator, texture.image, texture.allocation);
    }
    vkDestroyDescriptorSetLayout(device.handle(), descriptorSetLayoutTex, nullptr);
    vkDestroyDescriptorPool(device.handle(), descriptorPool, nullptr);
    vkDestroyPipeline(device.handle(), pipeline, nullptr);
    vkDestroyPipelineLayout(device.handle(), pipelineLayout, nullptr);
    vkDestroyShaderModule(device.handle(), shaderModule, nullptr);
    vkDestroyCommandPool(device.handle(), commandPool, nullptr);
    vkDestroySwapchainKHR(device.handle(), swapchain, nullptr);
    vmaDestroyAllocator(allocator);

    return 0;
}
