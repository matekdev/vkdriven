#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include "platform/file_watcher.h"
#include "platform/window.h"
#include "scene/scene.h"
#include "vk/allocator.h"
#include "vk/command_pool.h"
#include "vk/device.h"
#include "vk/frame_resources.h"
#include "vk/graphics_pipeline.h"
#include "vk/image.h"
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
    glm::mat4 viewProjection;
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
    [[nodiscard]] bool recreateSwapchain();
    [[nodiscard]] std::expected<GraphicsPipeline, std::string> buildPipeline() const;
    void reloadShaders();
    void drawFrame();
    void updateFrameData(Frame& frame) const;
    void recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex, const Frame& frame) const;

    Window window_;
    Instance instance_;
    Surface surface_;
    Device device_;
    Allocator allocator_;
    Swapchain swapchain_;
    VkFormat depthFormat_;
    Image depthImage_;
    CommandPool commandPool_;
    FrameResources frames_;
    Scene scene_;
    ShaderCompiler shaderCompiler_;
    std::filesystem::path shaderDirectory_;
    GraphicsPipeline pipeline_;
    FileWatcher shaderWatcher_;

    bool updateSwapchain_{false};
    bool reloadRequested_{false};
};
