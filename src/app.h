#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include "platform/file_watcher.h"
#include "platform/window.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "ui/imgui_layer.h"
#include "ui/viewport_target.h"
#include "vk/allocator.h"
#include "vk/command_pool.h"
#include "vk/device.h"
#include "vk/frame_resources.h"
#include "vk/graphics_pipeline.h"
#include "vk/instance.h"
#include "vk/shader_compiler.h"
#include "vk/surface.h"
#include "vk/swapchain.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

// Written once per frame into the frame's shader data buffer.
struct FrameData
{
    glm::mat4 view;
    glm::mat4 projection;
};

// Pushed before every draw. Must match DrawConstants in shaders/scene.slang.
struct DrawConstants
{
    VkDeviceAddress frame;
    VkDeviceAddress transforms;
    uint32_t transformIndex;
};

// Owns the whole renderer. Members are declared in dependency order, so they're created top to
// bottom and destroyed bottom to top; the destructor only has to wait for the GPU first.
// Throws std::runtime_error from the constructor if an asset or shader fails to load.
class App
{
  public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    void run();

  private:
    [[nodiscard]] bool handleEvents();
    [[nodiscard]] std::expected<GraphicsPipeline, std::string> buildPipeline() const;
    void reloadShaders();
    void drawUi();
    void drawFrame();
    void updateFrameData(Frame& frame) const;
    void recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex, const Frame& frame) const;
    void recordScenePass(VkCommandBuffer cb, const Frame& frame) const;
    void recordUiPass(VkCommandBuffer cb, uint32_t imageIndex) const;

    Window window_;
    Instance instance_;
    Surface surface_;
    Device device_;
    Allocator allocator_;
    Swapchain swapchain_;
    CommandPool commandPool_;
    FrameResources frames_;
    Scene scene_;
    ShaderCompiler shaderCompiler_;
    std::filesystem::path shaderDirectory_;
    GraphicsPipeline pipeline_;
    FileWatcher shaderWatcher_;
    ImGuiLayer imgui_;
    ViewportTarget viewport_;

    Camera camera_{glm::vec3{0.0f, 0.0f, 3.0f}, 0.0f, 0.0f};
    glm::vec2 mouseDelta_{};
    bool viewportHovered_{false};

    VkExtent2D requestedViewportExtent_{};
    bool updateSwapchain_{false};
    bool reloadRequested_{false};
};
