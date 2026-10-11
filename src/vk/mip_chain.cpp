#include "vk/mip_chain.h"

#include "vk/sync.h"

#include <algorithm>
#include <vector>

namespace
{

VkImageSubresourceRange mipRange(uint32_t baseMip, uint32_t mipCount, uint32_t layerCount)
{
    return {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = baseMip,
            .levelCount = mipCount,
            .layerCount = layerCount};
}

VkOffset3D mipExtent(VkExtent2D extent, uint32_t mip)
{
    return {.x = static_cast<int32_t>(std::max(extent.width >> mip, 1u)),
            .y = static_cast<int32_t>(std::max(extent.height >> mip, 1u)),
            .z = 1};
}

} // namespace

void recordMipChain(VkCommandBuffer cb, VkImage image, VkExtent2D extent, uint32_t mipLevels, uint32_t layerCount)
{
    for (uint32_t mip = 1; mip < mipLevels; mip++)
    {
        // The previous mip was just written (by the copy or the last blit); it becomes the blit source.
        pipelineBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                             .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                             .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                             .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                             .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                             .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             .image = image,
                             .subresourceRange = mipRange(mip - 1, 1, layerCount)});

        const VkImageBlit blit{.srcSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                               .mipLevel = mip - 1,
                                               .layerCount = layerCount},
                               .srcOffsets{{}, mipExtent(extent, mip - 1)},
                               .dstSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                               .mipLevel = mip,
                                               .layerCount = layerCount},
                               .dstOffsets{{}, mipExtent(extent, mip)}};
        vkCmdBlitImage(cb, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                       &blit, VK_FILTER_LINEAR);
    }

    constexpr VkPipelineStageFlags2 shaderReadStages =
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    // Every mip but the last was a blit source; the last was only ever written.
    std::vector<VkImageMemoryBarrier2> toShaderRead{{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                     .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                                     .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                     .dstStageMask = shaderReadStages,
                                                     .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                                     .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                     .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                                     .image = image,
                                                     .subresourceRange = mipRange(mipLevels - 1, 1, layerCount)}};
    if (mipLevels > 1)
    {
        toShaderRead.push_back({.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                .srcAccessMask = VK_ACCESS_2_NONE,
                                .dstStageMask = shaderReadStages,
                                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                .image = image,
                                .subresourceRange = mipRange(0, mipLevels - 1, layerCount)});
    }
    pipelineBarrier(cb, toShaderRead);
}
