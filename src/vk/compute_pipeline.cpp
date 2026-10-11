#include "vk/compute_pipeline.h"

#include "vk/check.h"
#include "vk/device.h"

ComputePipeline::ComputePipeline(const Device& device, VkPipelineCache pipelineCache, VkShaderModule shaderModule,
                                 const ComputePipelineInfo& info)
{
    VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = info.pushConstantSize};
    const bool hasSetLayout = info.setLayout != VK_NULL_HANDLE;
    const bool hasPushConstants = info.pushConstantSize > 0;
    VkPipelineLayoutCreateInfo layoutCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                        .setLayoutCount = hasSetLayout ? 1u : 0u,
                                        .pSetLayouts = hasSetLayout ? &info.setLayout : nullptr,
                                        .pushConstantRangeCount = hasPushConstants ? 1u : 0u,
                                        .pPushConstantRanges = hasPushConstants ? &pushConstantRange : nullptr};
    VkPipelineLayout layout{VK_NULL_HANDLE};
    chk(vkCreatePipelineLayout(device.handle(), &layoutCI, nullptr, &layout));
    layout_ = DeviceHandle<VkPipelineLayout>{device.handle(), layout};

    VkComputePipelineCreateInfo pipelineCI{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                           .stage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                  .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                  .module = shaderModule,
                                                  .pName = "main"},
                                           .layout = layout_.get()};
    VkPipeline pipeline{VK_NULL_HANDLE};
    chk(vkCreateComputePipelines(device.handle(), pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
    pipeline_ = DeviceHandle<VkPipeline>{device.handle(), pipeline};
}
