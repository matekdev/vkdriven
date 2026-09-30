#include <volk.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "platform/file_watcher.h"
#include "platform/window.h"
#include "scene/mesh.h"
#include "vk/allocator.h"
#include "vk/bindless_textures.h"
#include "vk/buffer.h"
#include "vk/check.h"
#include "vk/command_pool.h"
#include "vk/device.h"
#include "vk/frame_resources.h"
#include "vk/graphics_pipeline.h"
#include "vk/image.h"
#include "vk/instance.h"
#include "vk/shader_compiler.h"
#include "vk/surface.h"
#include "vk/swapchain.h"
#include "vk/texture.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <print>
#include <string>
#include <vector>

struct ShaderData
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 model[3];
    glm::vec4 lightPos{0.0f, -10.0f, 10.0f, 0.0f};
    uint32_t selected{1};
} shaderData{};

int main(int, char**)
{
    const Window window{"vkdriven", 1280, 720};

    const Instance instance{"vkdriven", window.requiredInstanceExtensions()};
    const Surface surface{instance, window};

    const Device device{instance, surface};

    const Allocator allocator{instance, device};

    Swapchain swapchain{device, surface, window};

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
        .extent{.width = swapchain.extent().width, .height = swapchain.extent().height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    Image depthImage{allocator, depthImageCI, VK_IMAGE_ASPECT_DEPTH_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT};

    // Model loading.
    const auto mesh = Mesh::loadObj(allocator, "assets/suzanne.obj");
    if (!mesh)
    {
        std::println(stderr, "{}", mesh.error());
        return 1;
    }

    // Command buffers and per-frame resources.
    const CommandPool commandPool{device};
    FrameResources frames{device, allocator, commandPool, sizeof(ShaderData)};

    // Texture loading.
    constexpr uint32_t textureCount = 3;
    std::vector<Texture> textures;
    for (uint32_t i = 0; i < textureCount; i++)
    {
        auto texture = Texture::loadKtx(device, allocator, commandPool, std::format("assets/suzanne{}.ktx", i));
        if (!texture)
        {
            std::println(stderr, "{}", texture.error());
            return 1;
        }
        textures.push_back(std::move(*texture));
    }

    // Descriptor indexing.
    const BindlessTextures bindlessTextures{device, textureCount};
    bindlessTextures.write(textures);

    // Shader loading.
    const std::filesystem::path shaderDirectory{VKDRIVEN_SHADER_DIR};
    const std::filesystem::path shaderPath = shaderDirectory / "shader.slang";
    const ShaderCompiler shaderCompiler;
    const auto shaderModule = shaderCompiler.compile(device, shaderPath);
    if (!shaderModule)
    {
        std::println(stderr, "{}", shaderModule.error());
        return 1;
    }

    // Graphics pipeline.
    const VkDescriptorSetLayout textureSetLayout = bindlessTextures.layout();
    GraphicsPipeline pipeline{device, shaderModule->get(), std::span{&textureSetLayout, 1}, swapchain.format(),
                              depthFormat};

    // Shader hot-reload. A failed compile keeps the current pipeline running.
    FileWatcher shaderWatcher{shaderDirectory};
    const auto reloadShaders = [&]
    {
        const auto reloadedModule = shaderCompiler.compile(device, shaderPath);
        if (!reloadedModule)
        {
            std::println(stderr, "{}", reloadedModule.error());
            return;
        }
        GraphicsPipeline reloadedPipeline{device, reloadedModule->get(), std::span{&textureSetLayout, 1},
                                          swapchain.format(), depthFormat};
        device.waitIdle();
        pipeline = std::move(reloadedPipeline);
        std::println("Reloaded {}", shaderPath.string());
    };
    bool reloadRequested{false};

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
                if (event.key.key == SDLK_F5)
                {
                    reloadRequested = true;
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

        if (shaderWatcher.poll() || reloadRequested)
        {
            reloadRequested = false;
            reloadShaders();
        }

        // Recreate swapchain
        if (updateSwapchain)
        {
            if (!swapchain.recreate())
            {
                SDL_WaitEvent(nullptr);
                continue;
            }
            updateSwapchain = false;

            depthImage.reset();
            depthImageCI.extent = {.width = swapchain.extent().width, .height = swapchain.extent().height, .depth = 1};
            depthImage =
                Image{allocator, depthImageCI, VK_IMAGE_ASPECT_DEPTH_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT};
        }

        Frame& frame = frames.current();

        // Wait on fence
        chk(vkWaitForFences(device.handle(), 1, frame.inFlight.ptr(), VK_TRUE, UINT64_MAX));

        // Acquire next image
        const auto [acquireStatus, acquiredIndex] = swapchain.acquireNextImage(frame.imageAcquired.get());
        if (acquireStatus == SwapchainStatus::OutOfDate)
        {
            updateSwapchain = true;
            continue;
        }
        if (acquireStatus == SwapchainStatus::Suboptimal)
        {
            updateSwapchain = true;
        }
        imageIndex = acquiredIndex;
        chk(vkResetFences(device.handle(), 1, frame.inFlight.ptr()));

        // Update shader data
        const VkExtent2D swapchainExtent = swapchain.extent();
        const float aspect{static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height)};
        shaderData.projection = glm::perspective(glm::radians(45.0f), aspect, 32.0f, 0.1f);
        shaderData.view = glm::translate(glm::mat4(1.0f), camPos);
        for (size_t i = 0; i < objectRotations.size(); i++)
        {
            const glm::vec3 instancePos{(static_cast<float>(i) - 1.0f) * 3.0f, 0.0f, 0.0f};
            shaderData.model[i] =
                glm::translate(glm::mat4(1.0f), instancePos) * glm::mat4_cast(glm::quat(objectRotations[i]));
        }
        frame.shaderData.write(std::span{&shaderData, 1});

        // Record command buffer
        VkCommandBuffer cb = frame.commandBuffer;
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
             .image = swapchain.image(imageIndex),
             .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}},
            {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
             .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
             .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
             .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
             .dstAccessMask =
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
             .image = depthImage.handle(),
             .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                               .levelCount = 1,
                               .layerCount = 1}},
        });
        VkDependencyInfo outputDependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                              .imageMemoryBarrierCount = static_cast<uint32_t>(outputBarriers.size()),
                                              .pImageMemoryBarriers = outputBarriers.data()};
        vkCmdPipelineBarrier2(cb, &outputDependencyInfo);

        VkRenderingAttachmentInfo colorAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                      .imageView = swapchain.view(imageIndex),
                                                      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                      .clearValue{.color{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderingAttachmentInfo depthAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                      .imageView = depthImage.view(),
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

        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());
        const VkDescriptorSet textureSet = bindlessTextures.set();
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout(), 0, 1, &textureSet, 0, nullptr);
        VkDeviceSize vOffset{0};
        const VkBuffer meshBuffer = mesh->buffer();
        vkCmdBindVertexBuffers(cb, 0, 1, &meshBuffer, &vOffset);
        vkCmdBindIndexBuffer(cb, meshBuffer, mesh->indexOffset(), Mesh::indexType);
        const VkDeviceAddress shaderDataAddress = frame.shaderData.deviceAddress();
        vkCmdPushConstants(cb, pipeline.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress),
                           &shaderDataAddress);
        vkCmdDrawIndexed(cb, mesh->indexCount(), 3, 0, 0, 0);
        vkCmdEndRendering(cb);

        VkImageMemoryBarrier2 barrierPresent{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_NONE,
            .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = swapchain.image(imageIndex),
            .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
        VkDependencyInfo presentDependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                               .imageMemoryBarrierCount = 1,
                                               .pImageMemoryBarriers = &barrierPresent};
        vkCmdPipelineBarrier2(cb, &presentDependencyInfo);
        chk(vkEndCommandBuffer(cb));

        // Submit command buffer
        VkSemaphoreSubmitInfo waitSemaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                                .semaphore = frame.imageAcquired.get(),
                                                .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkCommandBufferSubmitInfo commandBufferSubmitInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                                          .commandBuffer = cb};
        VkSemaphoreSubmitInfo signalSemaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                                  .semaphore = swapchain.renderComplete(imageIndex),
                                                  .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo2 submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                                 .waitSemaphoreInfoCount = 1,
                                 .pWaitSemaphoreInfos = &waitSemaphoreInfo,
                                 .commandBufferInfoCount = 1,
                                 .pCommandBufferInfos = &commandBufferSubmitInfo,
                                 .signalSemaphoreInfoCount = 1,
                                 .pSignalSemaphoreInfos = &signalSemaphoreInfo};
        chk(vkQueueSubmit2(device.queue(), 1, &submitInfo, frame.inFlight.get()));
        frames.advance();

        // Present image
        if (swapchain.present(device.queue(), imageIndex) != SwapchainStatus::Optimal)
        {
            updateSwapchain = true;
        }
    }

    // Cleaning up
    device.waitIdle();

    return 0;
}
