#include "render/shadow_pass.h"

#include "render/scene_draws.h"
#include "render/scene_pass.h"
#include "vk/bindless_textures.h"
#include "vk/check.h"
#include "vk/commands.h"
#include "vk/device.h"
#include "vk/sync.h"

#include <utility>

namespace
{

Image createShadowMap(const Allocator& allocator)
{
    VkImageCreateInfo imageCI{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = ShadowPass::format,
        .extent{.width = ShadowPass::size, .height = ShadowPass::size, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    return Image{allocator, imageCI, VK_IMAGE_ASPECT_DEPTH_BIT, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT};
}

// Nearest, so each lookup returns one stored depth. Outside the map the border depth is 0, which is the far
// plane in reverse-Z, so anything the light's box doesn't cover counts as lit.
DeviceHandle<VkSampler> createShadowSampler(const Device& device)
{
    VkSamplerCreateInfo samplerCI{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                  .magFilter = VK_FILTER_NEAREST,
                                  .minFilter = VK_FILTER_NEAREST,
                                  .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                  .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                  .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                  .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                  .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK};
    VkSampler sampler{VK_NULL_HANDLE};
    chk(vkCreateSampler(device.handle(), &samplerCI, nullptr, &sampler));
    return {device.handle(), sampler};
}

} // namespace

ShadowPass::ShadowPass(const Device& device, const Allocator& allocator, const ShaderCompiler& shaderCompiler,
                       const BindlessTextures& textures, std::filesystem::path shaderPath)
    : textures_{textures}, shadowMap_{createShadowMap(allocator)}, sampler_{createShadowSampler(device)},
      pipeline_{device,
                shaderCompiler,
                std::move(shaderPath),
                {.setLayout = textures.layout(), .pushConstantSize = sizeof(DrawConstants), .depthFormat = format}}
{
}

void ShadowPass::record(VkCommandBuffer cb, const Scene& scene, VkDeviceAddress frameData) const
{
    constexpr VkExtent2D extent{.width = size, .height = size};

    // The previous frame's scene pass may still be sampling the shadow map, so wait for its fragment
    // shaders before overwriting it. The depth is cleared anyway, so the old contents are discarded.
    pipelineBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                         .srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                         .srcAccessMask = VK_ACCESS_2_NONE,
                         .dstStageMask =
                             VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                         .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                          VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                         .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                         .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                         .image = shadowMap_.handle(),
                         .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)});

    VkRenderingAttachmentInfo depthAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .imageView = shadowMap_.view(),
                                                  .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                  .clearValue{.depthStencil{.depth = 0.0f, .stencil = 0}}};
    VkRenderingInfo renderingInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                  .renderArea{.extent = extent},
                                  .layerCount = 1,
                                  .pDepthAttachment = &depthAttachmentInfo};
    vkCmdBeginRendering(cb, &renderingInfo);

    setViewportAndScissor(cb, extent);

    recordSceneDraws(cb, pipeline_, textures_, scene, frameData);
    vkCmdEndRendering(cb);

    // The scene pass reads the shadow map in its fragment shader.
    pipelineBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                         .srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                         .srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                         .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                         .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                         .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                         .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         .image = shadowMap_.handle(),
                         .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT)});
}
