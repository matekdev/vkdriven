#include "app.h"

#include <imgui.h>

#include "ui/panels.h"
#include "util/expected.h"
#include "vk/check.h"

#include <algorithm>
#include <limits>
#include <print>
#include <span>

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
      scenePass_{device_, shaderCompiler_, shaderDirectory_ / "scene.slang"}, shaderWatcher_{shaderDirectory_},
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
            scenePass_.reloadShaders();
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

    scenePass_.record(cb, viewport_, scene_, frame.shaderData.deviceAddress());
    imgui_.record(cb, swapchain_, imageIndex);

    chk(vkEndCommandBuffer(cb));
}
