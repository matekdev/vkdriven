#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include "platform/file_watcher.h"
#include "platform/window.h"
#include "profiling/benchmark.h"
#include "render/cull_pass.h"
#include "render/scene_pass.h"
#include "render/shadow_pass.h"
#include "render/tonemap_pass.h"
#include "scene/camera.h"
#include "scene/light.h"
#include "scene/scene.h"
#include "ui/imgui_layer.h"
#include "ui/viewport_target.h"
#include "vk/allocator.h"
#include "vk/bindless_textures.h"
#include "vk/command_pool.h"
#include "vk/device.h"
#include "vk/frame_resources.h"
#include "vk/instance.h"
#include "vk/pipeline_cache.h"
#include "vk/shader_compiler.h"
#include "vk/surface.h"
#include "vk/swapchain.h"
#include "vk/texture.h"

#include <cstdint>
#include <filesystem>
#include <vector>

enum class RunMode
{
    Interactive,
    // Runs the benchmark once the viewport has settled, then quits. Vsync is off, so the GPU never idles
    // waiting on the display and its clocks stay up.
    Benchmark,
};

// Owns the whole renderer. Members are declared in dependency order, so they're created top to
// bottom and destroyed bottom to top; the destructor only has to wait for the GPU first.
// Throws std::runtime_error from the constructor if an asset or shader fails to load.
class App
{
  public:
    explicit App(RunMode mode);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    void run();

  private:
    [[nodiscard]] bool handleEvents();
    void drawUi();
    void drawFrame();
    void updateFrameData(Frame& frame) const;
    void recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex, Frame& frame) const;

    [[nodiscard]] uint32_t hdrTextureIndex() const
    {
        return static_cast<uint32_t>(scene_.textures().size());
    }

    [[nodiscard]] uint32_t shadowMapTextureIndex() const
    {
        return hdrTextureIndex() + 1;
    }

    [[nodiscard]] uint32_t environmentTextureIndex() const
    {
        return shadowMapTextureIndex() + 1;
    }

    RunMode mode_;
    Window window_;
    Instance instance_;
    Surface surface_;
    Device device_;
    Allocator allocator_;
    Swapchain swapchain_;
    CommandPool commandPool_;
    FrameResources frames_;
    Scene scene_;
    Texture environment_;
    BindlessTextures bindlessTextures_;
    ShaderCompiler shaderCompiler_;
    PipelineCache pipelineCache_;
    std::filesystem::path shaderDirectory_;
    CullPass cullPass_;
    ShadowPass shadowPass_;
    ScenePass scenePass_;
    TonemapPass tonemapPass_;
    FileWatcher shaderWatcher_;
    ImGuiLayer imgui_;
    ViewportTarget viewport_;

    Camera camera_{glm::vec3{-8.0f, 2.0f, 0.0f}, glm::radians(90.0f), 0.0f};
    DirectionalLight light_;
    CullSettings cullSettings_;
    ShadowSettings shadowSettings_;
    TonemapSettings tonemapSettings_;
    std::vector<GpuTiming> gpuTimings_;
    uint32_t visibleDrawCount_{0};
    float cpuRecordMilliseconds_{0.0f};
    Benchmark benchmark_;
    glm::vec2 mouseDelta_{};
    bool viewportHovered_{false};

    VkExtent2D requestedViewportExtent_{};
    bool updateSwapchain_{false};
    bool reloadRequested_{false};
};
