#include "app.h"

#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>

#include "vk/check.h"

#include <cstddef>
#include <format>
#include <limits>
#include <print>
#include <span>
#include <stdexcept>
#include <utility>

namespace
{

constexpr const char* scenePath = "assets/Suzanne/Suzanne.gltf";

template <typename T> T orThrow(std::expected<T, std::string> result)
{
    if (!result)
        throw std::runtime_error{result.error()};
    return std::move(*result);
}

VkFormat findDepthFormat(VkPhysicalDevice physical)
{
    for (const VkFormat format : {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT})
    {
        VkFormatProperties2 formatProperties{.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
        vkGetPhysicalDeviceFormatProperties2(physical, format, &formatProperties);
        if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return format;
    }
    throw std::runtime_error{"No supported depth/stencil format found"};
}

Image createDepthImage(const Allocator& allocator, VkFormat format, VkExtent2D extent)
{
    VkImageCreateInfo depthImageCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent{.width = extent.width, .height = extent.height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    return Image{allocator, depthImageCI, VK_IMAGE_ASPECT_DEPTH_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT};
}

} // namespace

App::App()
    : window_{"vkdriven", 1280, 720}, instance_{"vkdriven", window_.requiredInstanceExtensions()},
      surface_{instance_, window_}, device_{instance_, surface_}, allocator_{instance_, device_},
      swapchain_{device_, surface_, window_}, depthFormat_{findDepthFormat(device_.physical())},
      depthImage_{createDepthImage(allocator_, depthFormat_, swapchain_.extent())}, commandPool_{device_},
      frames_{device_, allocator_, commandPool_, sizeof(FrameData)},
      scene_{orThrow(Scene::loadGltf(allocator_, commandPool_, scenePath))}, shaderDirectory_{VKDRIVEN_SHADER_DIR},
      pipeline_{orThrow(buildPipeline())}, shaderWatcher_{shaderDirectory_}
{
    std::println("Loaded {}: {} meshes, {} primitives, {} vertices, {} indices, {} draws, {} transforms", scenePath,
                 scene_.meshes().size(), scene_.primitives().size(), scene_.vertexCount(), scene_.indexCount(),
                 scene_.draws().size(), scene_.transformCount());
}

App::~App()
{
    device_.waitIdle();
}

void App::run()
{
    while (true)
    {
        if (!handleEvents())
            return;

        if (shaderWatcher_.poll() || reloadRequested_)
        {
            reloadRequested_ = false;
            reloadShaders();
        }

        if (updateSwapchain_ && !recreateSwapchain())
        {
            SDL_WaitEvent(nullptr);
            continue;
        }

        drawFrame();
    }
}

bool App::handleEvents()
{
    for (SDL_Event event; SDL_PollEvent(&event);)
    {
        if (event.type == SDL_EVENT_QUIT)
            return false;

        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F5)
        {
            reloadRequested_ = true;
        }
        if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
        {
            updateSwapchain_ = true;
        }
    }
    return true;
}

bool App::recreateSwapchain()
{
    if (!swapchain_.recreate())
        return false;

    updateSwapchain_ = false;
    depthImage_.reset();
    depthImage_ = createDepthImage(allocator_, depthFormat_, swapchain_.extent());
    return true;
}

std::expected<GraphicsPipeline, std::string> App::buildPipeline() const
{
    return shaderCompiler_.compile(device_, shaderDirectory_ / "scene.slang")
        .transform(
            [&](const DeviceHandle<VkShaderModule>& shaderModule)
            {
                return GraphicsPipeline{device_,
                                        shaderModule.get(),
                                        std::span<const VkDescriptorSetLayout>{},
                                        sizeof(DrawConstants),
                                        swapchain_.format(),
                                        depthFormat_};
            });
}

void App::reloadShaders()
{
    auto reloadedPipeline = buildPipeline();
    if (!reloadedPipeline)
    {
        std::println(stderr, "{}", reloadedPipeline.error());
        return;
    }
    device_.waitIdle();
    pipeline_ = std::move(*reloadedPipeline);
    std::println("Reloaded shaders from {}", shaderDirectory_.string());
}

void App::drawFrame()
{
    Frame& frame = frames_.current();

    // Wait on fence
    chk(vkWaitForFences(device_.handle(), 1, frame.inFlight.ptr(), VK_TRUE, std::numeric_limits<uint64_t>::max()));

    // Acquire next image
    const auto [acquireStatus, imageIndex] = swapchain_.acquireNextImage(frame.imageAcquired.get());
    if (acquireStatus == SwapchainStatus::OutOfDate)
    {
        updateSwapchain_ = true;
        return;
    }
    if (acquireStatus == SwapchainStatus::Suboptimal)
    {
        updateSwapchain_ = true;
    }
    chk(vkResetFences(device_.handle(), 1, frame.inFlight.ptr()));

    updateFrameData(frame);
    recordCommandBuffer(frame.commandBuffer, imageIndex, frame);

    // Submit command buffer
    VkSemaphoreSubmitInfo waitSemaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                            .semaphore = frame.imageAcquired.get(),
                                            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkCommandBufferSubmitInfo commandBufferSubmitInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                                      .commandBuffer = frame.commandBuffer};
    VkSemaphoreSubmitInfo signalSemaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                              .semaphore = swapchain_.renderComplete(imageIndex),
                                              .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo2 submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                             .waitSemaphoreInfoCount = 1,
                             .pWaitSemaphoreInfos = &waitSemaphoreInfo,
                             .commandBufferInfoCount = 1,
                             .pCommandBufferInfos = &commandBufferSubmitInfo,
                             .signalSemaphoreInfoCount = 1,
                             .pSignalSemaphoreInfos = &signalSemaphoreInfo};
    chk(vkQueueSubmit2(device_.queue(), 1, &submitInfo, frame.inFlight.get()));
    frames_.advance();

    // Present image
    if (swapchain_.present(device_.queue(), imageIndex) != SwapchainStatus::Optimal)
    {
        updateSwapchain_ = true;
    }
}

void App::updateFrameData(Frame& frame) const
{
    const VkExtent2D extent = swapchain_.extent();
    const float aspect{static_cast<float>(extent.width) / static_cast<float>(extent.height)};

    // Reverse-Z: near and far are swapped so depth 1 is the near plane and 0 is the far plane.
    glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 100.0f, 0.1f);
    // glTF is +Y up but Vulkan's clip space has +Y pointing down the screen.
    projection[1][1] *= -1.0f;
    const glm::mat4 view =
        glm::lookAt(glm::vec3{0.0f, 0.0f, 3.0f}, glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});

    const FrameData frameData{.viewProjection = projection * view};
    frame.shaderData.write(std::span{&frameData, 1});
}

void App::recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex, const Frame& frame) const
{
    const VkExtent2D extent = swapchain_.extent();

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
         .image = swapchain_.image(imageIndex),
         .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}},
        {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
         .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
         .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
         .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
         .dstAccessMask =
             VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
         .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
         .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
         .image = depthImage_.handle(),
         .subresourceRange{
             .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, .levelCount = 1, .layerCount = 1}},
    });
    VkDependencyInfo outputDependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                          .imageMemoryBarrierCount = static_cast<uint32_t>(outputBarriers.size()),
                                          .pImageMemoryBarriers = outputBarriers.data()};
    vkCmdPipelineBarrier2(cb, &outputDependencyInfo);

    VkRenderingAttachmentInfo colorAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .imageView = swapchain_.view(imageIndex),
                                                  .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                  .clearValue{.color{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderingAttachmentInfo depthAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .imageView = depthImage_.view(),
                                                  .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                  .clearValue{.depthStencil{.depth = 0.0f, .stencil = 0}}};
    VkRenderingInfo renderingInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                  .renderArea{.extent = extent},
                                  .layerCount = 1,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &colorAttachmentInfo,
                                  .pDepthAttachment = &depthAttachmentInfo};
    vkCmdBeginRendering(cb, &renderingInfo);

    VkViewport viewport{.width = static_cast<float>(extent.width),
                        .height = static_cast<float>(extent.height),
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f};
    vkCmdSetViewport(cb, 0, 1, &viewport);
    VkRect2D scissor{.extent = extent};
    vkCmdSetScissor(cb, 0, 1, &scissor);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.handle());
    const VkDeviceSize vertexOffset{0};
    const VkBuffer vertexBuffer = scene_.vertexBuffer();
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer, &vertexOffset);
    vkCmdBindIndexBuffer(cb, scene_.indexBuffer(), 0, Scene::indexType);

    for (const Draw& draw : scene_.draws())
    {
        const Primitive& primitive = scene_.primitives()[draw.primitiveIndex];
        const DrawConstants constants{.frame = frame.shaderData.deviceAddress(),
                                      .transforms = scene_.transformsAddress(),
                                      .transformIndex = draw.transformIndex};
        vkCmdPushConstants(cb, pipeline_.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DrawConstants), &constants);
        vkCmdDrawIndexed(cb, primitive.indexCount, 1, primitive.firstIndex, primitive.vertexOffset, 0);
    }
    vkCmdEndRendering(cb);

    VkImageMemoryBarrier2 barrierPresent{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = swapchain_.image(imageIndex),
        .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
    VkDependencyInfo presentDependencyInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                           .imageMemoryBarrierCount = 1,
                                           .pImageMemoryBarriers = &barrierPresent};
    vkCmdPipelineBarrier2(cb, &presentDependencyInfo);
    chk(vkEndCommandBuffer(cb));
}
