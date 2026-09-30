#include "app.h"

#include <imgui.h>

#include "ui/panels.h"
#include "util/expected.h"
#include "vk/check.h"
#include "vk/sync.h"

#include <algorithm>
#include <array>
#include <limits>
#include <print>
#include <span>
#include <utility>

namespace
{

constexpr const char* scenePath = "assets/Suzanne/Suzanne.gltf";
} // namespace

App::App()
    : window_{"vkdriven", 1280, 720}, instance_{"vkdriven", window_.requiredInstanceExtensions()},
      surface_{instance_, window_}, device_{instance_, surface_}, allocator_{instance_, device_},
      swapchain_{device_, surface_, window_}, commandPool_{device_},
      frames_{device_, allocator_, commandPool_, sizeof(FrameData)},
      scene_{orThrow(Scene::loadGltf(allocator_, commandPool_, scenePath))}, shaderDirectory_{VKDRIVEN_SHADER_DIR},
      pipeline_{orThrow(buildPipeline())}, shaderWatcher_{shaderDirectory_},
      imgui_{window_, instance_, device_, FrameResources::maxFramesInFlight, swapchain_.format()},
      viewport_{allocator_, device_.depthFormat(), swapchain_.extent()}
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

        if (updateSwapchain_)
        {
            if (!swapchain_.recreate())
            {
                window_.waitForEvent();
                continue;
            }
            updateSwapchain_ = false;
        }

        if (viewport_.needsResize(requestedViewportExtent_))
        {
            // Earlier frames may still be rendering into or sampling the old images.
            device_.waitIdle();
            viewport_.resize(requestedViewportExtent_);
        }

        imgui_.beginFrame();
        drawUi();
        camera_.update(window_, viewportHovered_, mouseDelta_);
        imgui_.endFrame();
        drawFrame();
    }
}

bool App::handleEvents()
{
    const WindowEvents events = window_.pollEvents();
    if (events.quitRequested)
        return false;

    if (events.resized)
        updateSwapchain_ = true;
    if (std::ranges::contains(events.keysPressed, SDLK_F5))
        reloadRequested_ = true;
    mouseDelta_ = {events.mouseDeltaX, events.mouseDeltaY};
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
                                        ViewportTarget::colorFormat,
                                        device_.depthFormat()};
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

void App::drawUi()
{
    const ImGuiID dockspace = ImGui::DockSpaceOverViewport();
    const ViewportPanelState viewportPanel = drawViewportPanel(dockspace, viewport_);
    if (viewportPanel.requestedExtent)
        requestedViewportExtent_ = *viewportPanel.requestedExtent;
    viewportHovered_ = viewportPanel.hovered;
    drawStatsPanel(scene_, viewport_);
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
    const VkExtent2D extent = viewport_.extent();
    const float aspect{static_cast<float>(extent.width) / static_cast<float>(extent.height)};

    const FrameData frameData{.view = camera_.view(), .projection = camera_.projection(aspect)};
    frame.shaderData.write(std::span{&frameData, 1});
}

void App::recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex, const Frame& frame) const
{
    chk(vkResetCommandBuffer(cb, 0));
    VkCommandBufferBeginInfo cbBI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                  .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    chk(vkBeginCommandBuffer(cb, &cbBI));

    recordScenePass(cb, frame);
    recordUiPass(cb, imageIndex);

    chk(vkEndCommandBuffer(cb));
}

void App::recordScenePass(VkCommandBuffer cb, const Frame& frame) const
{
    const VkExtent2D extent = viewport_.extent();

    // The previous frame's UI pass may still be sampling the viewport image, so wait for its fragment
    // shaders before overwriting it. The image is cleared anyway, so the old contents are discarded.
    imageBarriers(
        cb,
        std::to_array<VkImageMemoryBarrier2>({
            {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
             .srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
             .srcAccessMask = VK_ACCESS_2_NONE,
             .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
             .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
             .image = viewport_.color().handle(),
             .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)},
            {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
             .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
             .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
             .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
             .dstAccessMask =
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
             .image = viewport_.depth().handle(),
             .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)},
        }));

    VkRenderingAttachmentInfo colorAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .imageView = viewport_.color().view(),
                                                  .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                  .clearValue{.color{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderingAttachmentInfo depthAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .imageView = viewport_.depth().view(),
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

    imageBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                      .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                      .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                      .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                      .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                      .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      .image = viewport_.color().handle(),
                      .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)});
}

void App::recordUiPass(VkCommandBuffer cb, uint32_t imageIndex) const
{
    imageBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                      .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      .srcAccessMask = VK_ACCESS_2_NONE,
                      .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                      .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                      .image = swapchain_.image(imageIndex),
                      .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)});

    VkRenderingAttachmentInfo colorAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .imageView = swapchain_.view(imageIndex),
                                                  .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                  .clearValue{.color{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderingInfo renderingInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                  .renderArea{.extent = swapchain_.extent()},
                                  .layerCount = 1,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &colorAttachmentInfo};
    vkCmdBeginRendering(cb, &renderingInfo);
    imgui_.record(cb);
    vkCmdEndRendering(cb);

    imageBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                      .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                      .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                      .dstAccessMask = VK_ACCESS_2_NONE,
                      .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                      .image = swapchain_.image(imageIndex),
                      .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)});
}
