#include "render/tonemap_pass.h"

#include "ui/viewport_target.h"
#include "vk/bindless_textures.h"
#include "vk/commands.h"
#include "vk/sync.h"

#include <utility>

TonemapPass::TonemapPass(const Device& device, const ShaderCompiler& shaderCompiler, const BindlessTextures& textures,
                         std::filesystem::path shaderPath)
    : textures_{textures}, pipeline_{device,
                                     shaderCompiler,
                                     std::move(shaderPath),
                                     {.setLayout = textures.layout(),
                                      .pushConstantSize = sizeof(TonemapConstants),
                                      .colorFormat = ViewportTarget::colorFormat,
                                      .vertexInput = VertexInput::None}}
{
}

void TonemapPass::record(VkCommandBuffer cb, const ViewportTarget& target, uint32_t hdrTextureIndex,
                         float exposure) const
{
    const VkExtent2D extent = target.extent();

    // The previous frame's UI pass may still be sampling the color image, so wait for its fragment
    // shaders before overwriting it. Every pixel gets written, so the old contents are discarded.
    pipelineBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                         .srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                         .srcAccessMask = VK_ACCESS_2_NONE,
                         .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                         .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                         .image = target.color().handle(),
                         .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)});

    VkRenderingAttachmentInfo colorAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .imageView = target.color().view(),
                                                  .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                  .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                  .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
    VkRenderingInfo renderingInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                  .renderArea{.extent = extent},
                                  .layerCount = 1,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &colorAttachmentInfo};
    vkCmdBeginRendering(cb, &renderingInfo);

    setViewportAndScissor(cb, extent);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.handle());
    const VkDescriptorSet textureSet = textures_.set();
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.layout(), 0, 1, &textureSet, 0, nullptr);

    constexpr VkShaderStageFlags pushStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    const TonemapConstants constants{.hdrTextureIndex = hdrTextureIndex, .exposure = exposure};
    vkCmdPushConstants(cb, pipeline_.layout(), pushStages, 0, sizeof(TonemapConstants), &constants);
    vkCmdDraw(cb, 3, 1, 0, 0);
    vkCmdEndRendering(cb);

    pipelineBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                         .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                         .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                         .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                         .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         .image = target.color().handle(),
                         .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)});
}
