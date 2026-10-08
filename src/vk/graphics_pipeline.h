#pragma once

#include <volk.h>

#include "vk/handle.h"

#include <cstdint>

class Device;

enum class VertexInput
{
    Mesh,
    None
};

struct GraphicsPipelineInfo
{
    VkDescriptorSetLayout setLayout{VK_NULL_HANDLE};
    uint32_t pushConstantSize{0};
    VkFormat colorFormat{VK_FORMAT_UNDEFINED};
    VkFormat depthFormat{VK_FORMAT_UNDEFINED};
    VertexInput vertexInput{VertexInput::Mesh};
};

// The mesh pipeline: Vertex input, one color + one depth attachment (dynamic rendering, reverse-Z),
// dynamic viewport/scissor, and one push constant block visible to the vertex stage.
// VertexInput::None with a VK_FORMAT_UNDEFINED depth format gives a fullscreen pass pipeline instead.
// A VK_FORMAT_UNDEFINED color format gives a depth-only pipeline.
class GraphicsPipeline
{
  public:
    GraphicsPipeline(const Device& device, VkShaderModule shaderModule, const GraphicsPipelineInfo& info);

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
