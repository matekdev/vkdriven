#include "render/scene_pass.h"

#include "scene/scene.h"
#include "ui/viewport_target.h"
#include "vk/bindless_textures.h"
#include "vk/commands.h"
#include "vk/device.h"
#include "vk/sync.h"

#include <array>
#include <cstddef>
#include <span>
#include <utility>

ScenePass::ScenePass(const Device& device, const ShaderCompiler& shaderCompiler, const BindlessTextures& textures,
                     std::filesystem::path shaderPath)
    : textures_{textures}, pipeline_{device,
                                     shaderCompiler,
                                     std::move(shaderPath),
                                     {.setLayout = textures.layout(),
                                      .pushConstantSize = sizeof(DrawConstants),
                                      .colorFormat = ViewportTarget::hdrFormat,
                                      .depthFormat = device.depthFormat()}}
{
}

void ScenePass::record(VkCommandBuffer cb, const ViewportTarget& target, const Scene& scene,
                       VkDeviceAddress frameData) const
{
    const VkExtent2D extent = target.extent();

    // The previous frame's UI pass may still be sampling the viewport image, so wait for its fragment
    // shaders before overwriting it. The image is cleared anyway, so the old contents are discarded.
    pipelineBarrier(
        cb,
        std::to_array<VkImageMemoryBarrier2>({
            {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
             .srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
             .srcAccessMask = VK_ACCESS_2_NONE,
             .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
             .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
             .image = target.hdr().handle(),
             .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)},
            {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
             .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
             .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
             .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
             .dstAccessMask =
                 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
             .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
             .image = target.depth().handle(),
             .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)},
        }));

    VkRenderingAttachmentInfo hdrAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                .imageView = target.hdr().view(),
                                                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                .clearValue{.color{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderingAttachmentInfo depthAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .imageView = target.depth().view(),
                                                  .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                  .clearValue{.depthStencil{.depth = 0.0f, .stencil = 0}}};
    VkRenderingInfo renderingInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                  .renderArea{.extent = extent},
                                  .layerCount = 1,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &hdrAttachmentInfo,
                                  .pDepthAttachment = &depthAttachmentInfo};
    vkCmdBeginRendering(cb, &renderingInfo);

    setViewportAndScissor(cb, extent);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.handle());
    const VkDescriptorSet textureSet = textures_.set();
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.layout(), 0, 1, &textureSet, 0, nullptr);
    const VkDeviceSize vertexOffset{0};
    const VkBuffer vertexBuffer = scene.vertexBuffer();
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer, &vertexOffset);
    vkCmdBindIndexBuffer(cb, scene.indexBuffer(), 0, Scene::indexType);

    constexpr VkShaderStageFlags pushStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    const DrawConstants constants{.frame = frameData,
                                  .draws = scene.drawBufferAddress(),
                                  .transforms = scene.transformBufferAddress(),
                                  .materials = scene.materialBufferAddress()};
    vkCmdPushConstants(cb, pipeline_.layout(), pushStages, 0, sizeof(DrawConstants), &constants);

    const std::span<const Draw> draws = scene.draws();
    for (uint32_t drawIndex = 0; drawIndex < draws.size(); drawIndex++)
    {
        const Primitive& primitive = scene.primitives()[draws[drawIndex].primitiveIndex];
        vkCmdPushConstants(cb, pipeline_.layout(), pushStages, offsetof(DrawConstants, drawIndex), sizeof(drawIndex),
                           &drawIndex);
        vkCmdDrawIndexed(cb, primitive.indexCount, 1, primitive.firstIndex, primitive.vertexOffset, 0);
    }
    vkCmdEndRendering(cb);

    pipelineBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                         .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                         .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                         .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                         .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         .image = target.hdr().handle(),
                         .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)});
}
