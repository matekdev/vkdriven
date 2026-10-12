#pragma once

#include <volk.h>

#include "vk/handle.h"

#include <cstdint>
#include <vector>

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
    std::vector<VkFormat> colorFormats;
    VkFormat depthFormat{VK_FORMAT_UNDEFINED};
    bool depthWrite{true};
    VkCompareOp depthCompareOp{VK_COMPARE_OP_GREATER_OR_EQUAL};
    VertexInput vertexInput{VertexInput::Mesh};
    // Set with vkCmdSetDepthBias before drawing.
    bool dynamicDepthBias{false};
};

// The mesh pipeline: Vertex input, color attachments + one depth attachment (dynamic rendering, reverse-Z),
// dynamic viewport/scissor, and one push constant block visible to the vertex stage.
// VertexInput::None with a VK_FORMAT_UNDEFINED depth format gives a fullscreen pass pipeline instead.
// No color formats gives a depth-only pipeline.
class GraphicsPipeline
{
  public:
    GraphicsPipeline(const Device& device, VkPipelineCache pipelineCache, VkShaderModule shaderModule,
                     const GraphicsPipelineInfo& info);

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
