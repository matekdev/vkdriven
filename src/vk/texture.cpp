#include "vk/texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "vk/allocator.h"
#include "vk/buffer.h"
#include "vk/check.h"
#include "vk/command_pool.h"
#include "vk/device.h"
#include "vk/sync.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <format>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace
{

struct StbiImageDeleter
{
    void operator()(stbi_uc* pixels) const
    {
        stbi_image_free(pixels);
    }
};

VkImageSubresourceRange mipRange(uint32_t baseMip, uint32_t mipCount)
{
    return {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = baseMip, .levelCount = mipCount, .layerCount = 1};
}

VkOffset3D mipExtent(uint32_t width, uint32_t height, uint32_t mip)
{
    return {.x = static_cast<int32_t>(std::max(width >> mip, 1u)),
            .y = static_cast<int32_t>(std::max(height >> mip, 1u)),
            .z = 1};
}

DeviceHandle<VkSampler> createSampler(const Device& device, uint32_t mipLevels)
{
    VkSamplerCreateInfo samplerCI{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                  .magFilter = VK_FILTER_LINEAR,
                                  .minFilter = VK_FILTER_LINEAR,
                                  .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                  .anisotropyEnable = VK_TRUE,
                                  .maxAnisotropy = 8.0f,
                                  .maxLod = static_cast<float>(mipLevels)};
    VkSampler sampler{VK_NULL_HANDLE};
    chk(vkCreateSampler(device.handle(), &samplerCI, nullptr, &sampler));
    return {device.handle(), sampler};
}

} // namespace

Texture::Texture(Image image, DeviceHandle<VkSampler> sampler) : image_{std::move(image)}, sampler_{std::move(sampler)}
{
}

std::expected<Texture, std::string> Texture::loadImage(const Device& device, const Allocator& allocator,
                                                       const CommandPool& commandPool,
                                                       const std::filesystem::path& path, VkFormat format)
{
    int width{0};
    int height{0};
    const std::unique_ptr<stbi_uc, StbiImageDeleter> pixels{
        stbi_load(path.string().c_str(), &width, &height, nullptr, STBI_rgb_alpha)};
    if (!pixels)
        return std::unexpected{std::format("Failed to load image {}: {}", path.string(), stbi_failure_reason())};

    const auto pixelBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    return fromPixels(device, allocator, commandPool, static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                      std::as_bytes(std::span{pixels.get(), pixelBytes}), format);
}

std::expected<Texture, std::string> Texture::fromPixels(const Device& device, const Allocator& allocator,
                                                        const CommandPool& commandPool, uint32_t width, uint32_t height,
                                                        std::span<const std::byte> pixels, VkFormat format)
{
    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties(device.physical(), format, &formatProperties);
    constexpr VkFormatFeatureFlags requiredFeatures = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT |
                                                      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    if ((formatProperties.optimalTilingFeatures & requiredFeatures) != requiredFeatures)
        return std::unexpected{std::format("Format {} doesn't support linear blits", static_cast<int>(format))};

    const uint32_t mipLevels = std::bit_width(std::max(width, height));

    VkImageCreateInfo imageCI{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                              .imageType = VK_IMAGE_TYPE_2D,
                              .format = format,
                              .extent{.width = width, .height = height, .depth = 1},
                              .mipLevels = mipLevels,
                              .arrayLayers = 1,
                              .samples = VK_SAMPLE_COUNT_1_BIT,
                              .tiling = VK_IMAGE_TILING_OPTIMAL,
                              .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                       VK_IMAGE_USAGE_SAMPLED_BIT,
                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    Image image{allocator, imageCI, VK_IMAGE_ASPECT_COLOR_BIT};

    Buffer stagingBuffer{allocator, pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT};
    stagingBuffer.write(pixels);

    commandPool.submitImmediate(
        [&](VkCommandBuffer cb)
        {
            pipelineBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                 .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                                 .srcAccessMask = VK_ACCESS_2_NONE,
                                 .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                 .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                 .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                 .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 .image = image.handle(),
                                 .subresourceRange = mipRange(0, mipLevels)});

            const VkBufferImageCopy copyRegion{
                .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .layerCount = 1},
                .imageExtent{.width = width, .height = height, .depth = 1}};
            vkCmdCopyBufferToImage(cb, stagingBuffer.handle(), image.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                   &copyRegion);

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
                                     .image = image.handle(),
                                     .subresourceRange = mipRange(mip - 1, 1)});

                const VkImageBlit blit{
                    .srcSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = mip - 1, .layerCount = 1},
                    .srcOffsets{{}, mipExtent(width, height, mip - 1)},
                    .dstSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = mip, .layerCount = 1},
                    .dstOffsets{{}, mipExtent(width, height, mip)}};
                vkCmdBlitImage(cb, image.handle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.handle(),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
            }

            // Every mip but the last was a blit source; the last was only ever written.
            std::vector<VkImageMemoryBarrier2> toShaderRead{{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                             .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                                             .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                             .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                             .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                                             .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                             .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                                             .image = image.handle(),
                                                             .subresourceRange = mipRange(mipLevels - 1, 1)}};
            if (mipLevels > 1)
            {
                toShaderRead.push_back({.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                        .srcAccessMask = VK_ACCESS_2_NONE,
                                        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                        .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                        .image = image.handle(),
                                        .subresourceRange = mipRange(0, mipLevels - 1)});
            }
            pipelineBarrier(cb, toShaderRead);
        });

    return Texture{std::move(image), createSampler(device, mipLevels)};
}
