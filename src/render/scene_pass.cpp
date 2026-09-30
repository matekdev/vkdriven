#include "render/scene_pass.h"

#include "scene/scene.h"
#include "ui/viewport_target.h"
#include "util/expected.h"
#include "vk/device.h"
#include "vk/shader_compiler.h"
#include "vk/sync.h"

#include <array>
#include <print>
#include <span>
#include <utility>

ScenePass::ScenePass(const Device& device, const ShaderCompiler& shaderCompiler, std::filesystem::path shaderPath)
    : device_{device}, shaderCompiler_{shaderCompiler}, shaderPath_{std::move(shaderPath)},
      pipeline_{orThrow(buildPipeline())}
{
}

void ScenePass::reloadShaders()
{
    auto reloadedPipeline = buildPipeline();
    if (!reloadedPipeline)
    {
        std::println(stderr, "{}", reloadedPipeline.error());
        return;
    }
    device_.waitIdle();
    pipeline_ = std::move(*reloadedPipeline);
    std::println("Reloaded {}", shaderPath_.string());
}

std::expected<GraphicsPipeline, std::string> ScenePass::buildPipeline() const
{
    return shaderCompiler_.compile(device_, shaderPath_)
        .transform(
            [&](const DeviceHandle<VkShaderModule>& shaderModule)
            {
                return GraphicsPipeline{device_,
                                        shaderModule.get(),
                                        std::span<const VkDescriptorSetLayout>{},
                                        sizeof(DrawConstants),
                                        ViewportTarget::colorFormat,
                                        device_.depthFormat()};
            });
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
             .image = target.color().handle(),
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
             .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)},
        }));

    VkRenderingAttachmentInfo colorAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .imageView = target.color().view(),
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
                                  .pColorAttachments = &colorAttachmentInfo,
                                  .pDepthAttachment = &depthAttachmentInfo};
    vkCmdBeginRendering(cb, &renderingInfo);

    VkViewport viewport{.width = static_cast<float>(extent.width),
                        .height = static_cast<float>(extent.height),
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f};
    vkCmdSetViewport(cb, 0, 1, &viewport);
    VkRect2D scissor{.extent = extent};
    vkCmdSetScissor(cb, 0, 1, &scissor);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.handle());
    const VkDeviceSize vertexOffset{0};
    const VkBuffer vertexBuffer = scene.vertexBuffer();
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer, &vertexOffset);
    vkCmdBindIndexBuffer(cb, scene.indexBuffer(), 0, Scene::indexType);

    for (const Draw& draw : scene.draws())
    {
        const Primitive& primitive = scene.primitives()[draw.primitiveIndex];
        const DrawConstants constants{
            .frame = frameData, .transforms = scene.transformsAddress(), .transformIndex = draw.transformIndex};
        vkCmdPushConstants(cb, pipeline_.layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DrawConstants), &constants);
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
                         .image = target.color().handle(),
                         .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)});
}
