#pragma once

#include <volk.h>

#include "vk/handle.h"

#include <cstdint>

class Device;

struct ComputePipelineInfo
{
    VkDescriptorSetLayout setLayout{VK_NULL_HANDLE};
    uint32_t pushConstantSize{0};
};

// A pipeline with a single compute stage. Unlike a graphics pipeline there's no fixed-function state:
// just the shader and the layout (an optional descriptor set plus one push constant block).
class ComputePipeline
{
  public:
    ComputePipeline(const Device& device, VkPipelineCache pipelineCache, VkShaderModule shaderModule,
                    const ComputePipelineInfo& info);

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
