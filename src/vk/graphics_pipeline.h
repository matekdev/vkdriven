#pragma once

#include <volk.h>

#include "vk/handle.h"

#include <span>

class Device;

// The mesh pipeline: Vertex input, one color + one depth attachment (dynamic rendering, reverse-Z),
// dynamic viewport/scissor, and a single push constant holding the shader data buffer's address.
class GraphicsPipeline
{
  public:
    GraphicsPipeline(const Device& device, VkShaderModule shaderModule,
                     std::span<const VkDescriptorSetLayout> setLayouts, VkFormat colorFormat, VkFormat depthFormat);

    [[nodiscard]] VkPipeline handle() const
    {
        return pipeline_.get();
    }

    [[nodiscard]] VkPipelineLayout layout() const
    {
        return layout_.get();
    }

  private:
    DeviceHandle<VkPipelineLayout> layout_;
    DeviceHandle<VkPipeline> pipeline_;
};
