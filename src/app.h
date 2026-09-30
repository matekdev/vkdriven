#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include "platform/file_watcher.h"
#include "platform/window.h"
#include "scene/mesh.h"
#include "vk/allocator.h"
#include "vk/bindless_textures.h"
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
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

struct ShaderData
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 model[3];
    glm::vec4 lightPos{0.0f, -10.0f, 10.0f, 0.0f};
    uint32_t selected{1};
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
    static constexpr uint32_t textureCount = 3;

    [[nodiscard]] bool handleEvents(float elapsedTime);
    [[nodiscard]] bool recreateSwapchain();
    [[nodiscard]] std::expected<GraphicsPipeline, std::string> buildPipeline() const;
    void reloadShaders();
    void drawFrame();
    void updateShaderData(Frame& frame);
    void recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex, const Frame& frame) const;

    Window window_;
    Instance instance_;
    Surface surface_;
    Device device_;
    Allocator allocator_;
    Swapchain swapchain_;
    VkFormat depthFormat_;
    Image depthImage_;
    Mesh mesh_;
    CommandPool commandPool_;
    FrameResources frames_;
    std::vector<Texture> textures_;
    BindlessTextures bindlessTextures_;
    ShaderCompiler shaderCompiler_;
    std::filesystem::path shaderDirectory_;
    GraphicsPipeline pipeline_;
    FileWatcher shaderWatcher_;

    ShaderData shaderData_{};
    glm::vec3 cameraPosition_{0.0f, 0.0f, -6.0f};
    std::array<glm::vec3, 3> objectRotations_{};
    bool updateSwapchain_{false};
    bool reloadRequested_{false};
};
