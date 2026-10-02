#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include "platform/file_watcher.h"
#include "platform/window.h"
#include "render/scene_pass.h"
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
#include "vk/shader_compiler.h"
#include "vk/surface.h"
#include "vk/swapchain.h"

#include <cstdint>
#include <filesystem>

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
    void drawUi();
    void drawFrame();
    void updateFrameData(Frame& frame) const;
    void recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex, const Frame& frame) const;

    Window window_;
    Instance instance_;
    Surface surface_;
    Device device_;
    Allocator allocator_;
    Swapchain swapchain_;
    CommandPool commandPool_;
    FrameResources frames_;
    Scene scene_;
    BindlessTextures bindlessTextures_;
    ShaderCompiler shaderCompiler_;
    std::filesystem::path shaderDirectory_;
    ScenePass scenePass_;
    FileWatcher shaderWatcher_;
    ImGuiLayer imgui_;
    ViewportTarget viewport_;

    Camera camera_{glm::vec3{0.0f, 0.0f, 3.0f}, 0.0f, 0.0f};
    DirectionalLight light_;
    glm::vec2 mouseDelta_{};
    bool viewportHovered_{false};

    VkExtent2D requestedViewportExtent_{};
    bool updateSwapchain_{false};
    bool reloadRequested_{false};
};
