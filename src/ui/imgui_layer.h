#pragma once

#include <volk.h>

#include <cstdint>

class Device;
class Instance;
class Swapchain;
class Window;

// Owns the ImGui context plus its SDL3 and Vulkan backends. The Vulkan backend creates its own
// pipeline and descriptor pool. The pipeline is built for a single color attachment of the given
// format with no depth.
class ImGuiLayer
{
  public:
    ImGuiLayer(const Window& window, const Instance& instance, const Device& device, uint32_t framesInFlight,
               VkFormat colorFormat);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;
    ImGuiLayer(ImGuiLayer&&) = delete;
    ImGuiLayer& operator=(ImGuiLayer&&) = delete;

    // Call once per frame before any ImGui:: widget calls.
    void beginFrame() const;
    // Finalizes this frame's widgets into draw data. Call before recording.
    void endFrame() const;
    // Records a pass that clears the swapchain image, draws the data from endFrame() into it,
    // and leaves it ready to present.
    void record(VkCommandBuffer cb, const Swapchain& swapchain, uint32_t imageIndex) const;
};
