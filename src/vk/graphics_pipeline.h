#pragma once

#include <volk.h>

#include "vk/handle.h"

#include <cstdint>
#include <span>

class Device;

// The mesh pipeline: Vertex input, one color + one depth attachment (dynamic rendering, reverse-Z),
// dynamic viewport/scissor, and one push constant block visible to the vertex stage.
class GraphicsPipeline
{
  public:
    GraphicsPipeline(const Device& device, VkShaderModule shaderModule,
                     std::span<const VkDescriptorSetLayout> setLayouts, uint32_t pushConstantSize, VkFormat colorFormat,
                     VkFormat depthFormat);

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
