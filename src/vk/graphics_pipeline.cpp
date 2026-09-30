#include "vk/graphics_pipeline.h"

#include "scene/vertex.h"
#include "vk/check.h"
#include "vk/device.h"

#include <array>
#include <cstdint>

GraphicsPipeline::GraphicsPipeline(const Device& device, VkShaderModule shaderModule,
                                   std::span<const VkDescriptorSetLayout> setLayouts, uint32_t pushConstantSize,
                                   VkFormat colorFormat, VkFormat depthFormat)
{
    VkPushConstantRange pushConstantRange{.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = pushConstantSize};
    VkPipelineLayoutCreateInfo layoutCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                        .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
                                        .pSetLayouts = setLayouts.data(),
                                        .pushConstantRangeCount = 1,
                                        .pPushConstantRanges = &pushConstantRange};
    VkPipelineLayout layout{VK_NULL_HANDLE};
    chk(vkCreatePipelineLayout(device.handle(), &layoutCI, nullptr, &layout));
    layout_ = DeviceHandle<VkPipelineLayout>{device.handle(), layout};

    const auto shaderStages = std::to_array<VkPipelineShaderStageCreateInfo>({
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = shaderModule,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = shaderModule,
         .pName = "main"},
    });

    constexpr VkVertexInputBindingDescription vertexBinding = Vertex::bindingDescription();
    constexpr auto vertexAttributes = Vertex::attributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInputState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertexBinding,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
        .pVertexAttributeDescriptions = vertexAttributes.data()};
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

    const auto dynamicStates = std::to_array<VkDynamicState>({VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR});
    VkPipelineDynamicStateCreateInfo dynamicState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                  .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                  .pDynamicStates = dynamicStates.data()};
    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};

    VkPipelineRasterizationStateCreateInfo rasterizationState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .lineWidth = 1.0f};
    VkPipelineMultisampleStateCreateInfo multisampleState{.sType =
                                                              VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                          .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo depthStencilState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL};
    VkPipelineColorBlendAttachmentState blendAttachment{.colorWriteMask =
                                                            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo colorBlendState{.sType =
                                                            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                        .attachmentCount = 1,
                                                        .pAttachments = &blendAttachment};

    VkPipelineRenderingCreateInfo renderingCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                              .colorAttachmentCount = 1,
                                              .pColorAttachmentFormats = &colorFormat,
                                              .depthAttachmentFormat = depthFormat};
    VkGraphicsPipelineCreateInfo pipelineCI{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                            .pNext = &renderingCI,
                                            .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                            .pStages = shaderStages.data(),
                                            .pVertexInputState = &vertexInputState,
                                            .pInputAssemblyState = &inputAssemblyState,
                                            .pViewportState = &viewportState,
                                            .pRasterizationState = &rasterizationState,
                                            .pMultisampleState = &multisampleState,
                                            .pDepthStencilState = &depthStencilState,
                                            .pColorBlendState = &colorBlendState,
                                            .pDynamicState = &dynamicState,
                                            .layout = layout_.get()};
    VkPipeline pipeline{VK_NULL_HANDLE};
    chk(vkCreateGraphicsPipelines(device.handle(), VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));
    pipeline_ = DeviceHandle<VkPipeline>{device.handle(), pipeline};
}
