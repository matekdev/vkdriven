#include "app.h"

#include <imgui.h>

#include "render/shadow_cascades.h"
#include "ui/panels.h"
#include "util/expected.h"
#include "vk/check.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <print>
#include <ranges>
#include <span>
#include <vector>

namespace
{

constexpr const char* scenePath = "assets/Sponza/Sponza.gltf";
constexpr const char* pipelineCachePath = "pipeline_cache.bin";
constexpr const char* environmentPath = "assets/environment/kloofendal_48d_partly_cloudy_puresky_2k.hdr";

} // namespace

App::App(RunMode mode)
    : mode_{mode}, window_{"vkdriven", 1920, 1080}, instance_{"vkdriven", window_.requiredInstanceExtensions()},
      surface_{instance_, window_}, device_{instance_, surface_}, allocator_{instance_, device_},
      swapchain_{device_, surface_, window_, mode != RunMode::Benchmark}, commandPool_{device_},
      frames_{device_, allocator_, commandPool_, sizeof(FrameData)},
      scene_{orThrow(Scene::loadGltf(device_, allocator_, commandPool_, scenePath))},
      environment_{orThrow(Texture::loadHdr(device_, allocator_, commandPool_, environmentPath))},
      bindlessTextures_{device_, static_cast<uint32_t>(scene_.textures().size()) + 3},
      pipelineCache_{device_, pipelineCachePath}, shaderDirectory_{VKDRIVEN_SHADER_DIR},
      cullPass_{device_, allocator_, shaderCompiler_, pipelineCache_, scene_, shaderDirectory_ / "cull.slang"},
      shadowPass_{device_, allocator_, shaderCompiler_, pipelineCache_, bindlessTextures_,
                  shaderDirectory_ / "shadow.slang"},
      scenePass_{device_, shaderCompiler_, pipelineCache_, bindlessTextures_, shaderDirectory_ / "scene.slang",
                 shaderDirectory_ / "skybox.slang"},
      tonemapPass_{device_, shaderCompiler_, pipelineCache_, bindlessTextures_, shaderDirectory_ / "tonemap.slang"},
      shaderWatcher_{shaderDirectory_},
      imgui_{window_, instance_, device_, pipelineCache_.handle(), FrameResources::maxFramesInFlight,
             swapchain_.format()},
      viewport_{allocator_, device_.depthFormat(), swapchain_.extent()}
{
    bindlessTextures_.write(scene_.textures());

    const VkDescriptorImageInfo hdrInfo = viewport_.hdrDescriptorInfo();
    bindlessTextures_.write(std::span{&hdrInfo, 1}, hdrTextureIndex());

    const VkDescriptorImageInfo shadowMapInfo = shadowPass_.descriptorInfo();
    bindlessTextures_.write(std::span{&shadowMapInfo, 1}, shadowMapTextureIndex());

    const VkDescriptorImageInfo environmentInfo = environment_.descriptorInfo();
    bindlessTextures_.write(std::span{&environmentInfo, 1}, environmentTextureIndex());

    std::println("Loaded {}: {} meshes, {} primitives, {} vertices, {} indices, {} draws, {} transforms, {} textures",
                 scenePath, scene_.meshes().size(), scene_.primitives().size(), scene_.vertexCount(),
                 scene_.indexCount(), scene_.draws().size(), scene_.transforms().size(), scene_.textures().size());
}

App::~App()
{
    device_.waitIdle();
}

void App::run()
{
    // Gives the viewport panel a few frames to settle on its size before the benchmark records it.
    constexpr uint32_t benchmarkStartFrame = 10;
    uint32_t frameCount = 0;

    while (true)
    {
        if (mode_ == RunMode::Benchmark)
        {
            if (frameCount == benchmarkStartFrame)
                benchmark_.start();
            else if (frameCount > benchmarkStartFrame && !benchmark_.running())
                return;
            frameCount++;
        }

        if (!handleEvents())
            return;

        if (shaderWatcher_.poll() || reloadRequested_)
        {
            reloadRequested_ = false;
            cullPass_.reloadShaders();
            shadowPass_.reloadShaders();
            scenePass_.reloadShaders();
            tonemapPass_.reloadShaders();
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

        // The benchmark always renders at the same size, so results don't depend on the window or panel layout.
        const VkExtent2D viewportExtent = benchmark_.running() ? Benchmark::renderExtent : requestedViewportExtent_;
        if (viewport_.needsResize(viewportExtent))
        {
            // Earlier frames may still be rendering into or sampling the old images.
            device_.waitIdle();
            viewport_.resize(viewportExtent);

            const VkDescriptorImageInfo hdrInfo = viewport_.hdrDescriptorInfo();
            bindlessTextures_.write(std::span{&hdrInfo, 1}, hdrTextureIndex());
        }

        imgui_.beginFrame();
        drawUi();
        if (benchmark_.running())
        {
            const BenchmarkView& view = benchmark_.currentView();
            camera_.setPose(view.position, view.yawRadians, view.pitchRadians);
        }
        else
        {
            camera_.update(window_, viewportHovered_, mouseDelta_);
        }
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
    const ViewportPanelState viewportPanel = drawViewportPanel(dockspace, viewport_, camera_, cullSettings_);
    if (viewportPanel.requestedExtent)
        requestedViewportExtent_ = *viewportPanel.requestedExtent;
    viewportHovered_ = viewportPanel.hovered;
    drawStatsPanel(scene_, viewport_, gpuTimings_, visibleDrawCount_, cpuRecordMilliseconds_, benchmark_);
    drawCameraPanel(camera_, cullSettings_);
    drawLightPanel(light_);
    drawShadowPanel(shadowSettings_);
    drawTonemapPanel(tonemapSettings_);
}

void App::drawFrame()
{
    Frame& frame = frames_.current();

    // Wait on fence
    chk(vkWaitForFences(device_.handle(), 1, frame.inFlight.ptr(), VK_TRUE, std::numeric_limits<uint64_t>::max()));

    // The fence guarantees this frame's last timestamps are written; read them before recording resets the queries.
    // A running average keeps the numbers readable in the stats panel.
    const std::vector<GpuTiming> latestTimings = frame.gpuTimer.read();
    if (latestTimings.size() != gpuTimings_.size())
        gpuTimings_ = latestTimings;
    for (auto [smoothed, latest] : std::views::zip(gpuTimings_, latestTimings))
        smoothed.milliseconds = std::lerp(smoothed.milliseconds, latest.milliseconds, 0.05f);

    std::array<uint32_t, 1> visibleDrawCount{};
    frame.visibleDrawCount.read(visibleDrawCount);
    visibleDrawCount_ = visibleDrawCount[0];

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

    const auto recordStart = std::chrono::steady_clock::now();
    updateFrameData(frame);
    recordCommandBuffer(frame.commandBuffer, imageIndex, frame);
    const float cpuRecordMilliseconds =
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - recordStart).count();
    cpuRecordMilliseconds_ = std::lerp(cpuRecordMilliseconds_, cpuRecordMilliseconds, 0.05f);
    benchmark_.addFrame(latestTimings, cpuRecordMilliseconds);

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

    const ShadowCascades cascades =
        computeShadowCascades(camera_, aspect, light_.directionToLight(), shadowSettings_);

    const glm::mat4 view = camera_.view();
    const glm::mat4 projection = camera_.projection(aspect);
    const glm::mat4 cullProjection =
        camera_.projection(aspect, camera_.nearPlane(), camera_.farPlane(), cullSettings_.fovScale);
    const FrameData frameData{.view = view,
                              .projection = projection,
                              .directionToLight = glm::vec4{light_.directionToLight(), 0.0f},
                              .cameraPosition = glm::vec4{camera_.worldPosition(), 0.0f},
                              .lightRadiance = glm::vec4{light_.radiance(), 0.0f},
                              .lightViewProjections = cascades.viewProjections,
                              .cascadeSplitDepths = cascades.splitDepths,
                              .cascadeTexelSizes = cascades.texelSizes,
                              .shadowMapTextureIndex = shadowMapTextureIndex(),
                              .normalOffset = shadowSettings_.normalOffset,
                              .showCascades = shadowSettings_.showCascades ? 1u : 0u,
                              .environmentTextureIndex = environmentTextureIndex(),
                              .frustumPlanes = frustumPlanes(cullProjection * view)};
    frame.shaderData.write(std::span{&frameData, 1});
}

void App::recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex, Frame& frame) const
{
    chk(vkResetCommandBuffer(cb, 0));
    VkCommandBufferBeginInfo cbBI{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                  .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    chk(vkBeginCommandBuffer(cb, &cbBI));

    GpuTimer& timer = frame.gpuTimer;
    timer.reset(cb);
    const uint32_t frameScope = timer.begin(cb, "Frame");

    const uint32_t cullScope = timer.begin(cb, "Cull");
    cullPass_.record(cb, scene_, frame.shaderData.deviceAddress(), frame.visibleDrawCount);
    timer.end(cb, cullScope);

    const uint32_t shadowScope = timer.begin(cb, "Shadows");
    shadowPass_.record(cb, scene_, frame.shaderData.deviceAddress(), shadowSettings_);
    timer.end(cb, shadowScope);

    const uint32_t sceneScope = timer.begin(cb, "Scene");
    scenePass_.record(cb, viewport_, scene_, frame.shaderData.deviceAddress(), cullPass_);
    timer.end(cb, sceneScope);

    const uint32_t tonemapScope = timer.begin(cb, "Tonemap");
    tonemapPass_.record(cb, viewport_, hdrTextureIndex(), tonemapSettings_);
    timer.end(cb, tonemapScope);

    const uint32_t imguiScope = timer.begin(cb, "ImGui");
    imgui_.record(cb, swapchain_, imageIndex);
    timer.end(cb, imguiScope);

    timer.end(cb, frameScope);

    chk(vkEndCommandBuffer(cb));
}
