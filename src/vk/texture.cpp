#include "vk/texture.h"

#include <ktx.h>
#include <ktxvulkan.h>

#include "vk/allocator.h"
#include "vk/buffer.h"
#include "vk/check.h"
#include "vk/command_pool.h"
#include "vk/device.h"

#include <cstdint>
#include <format>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace
{

struct KtxTextureDeleter
{
    void operator()(ktxTexture* texture) const
    {
        ktxTexture_Destroy(texture);
    }
};

} // namespace

Texture::Texture(Image image, DeviceHandle<VkSampler> sampler) : image_{std::move(image)}, sampler_{std::move(sampler)}
{
}

std::expected<Texture, std::string> Texture::loadKtx(const Device& device, const Allocator& allocator,
                                                     const CommandPool& commandPool, const std::filesystem::path& path)
{
    ktxTexture* rawKtx{nullptr};
    if (ktxTexture_CreateFromNamedFile(path.string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &rawKtx) !=
        KTX_SUCCESS)
        return std::unexpected{std::format("Failed to load texture {}", path.string())};
    const std::unique_ptr<ktxTexture, KtxTextureDeleter> ktx{rawKtx};

    std::vector<VkBufferImageCopy> copyRegions{};
    for (uint32_t mip = 0; mip < ktx->numLevels; mip++)
    {
        ktx_size_t mipOffset{0};
        if (ktxTexture_GetImageOffset(ktx.get(), mip, 0, 0, &mipOffset) != KTX_SUCCESS)
            return std::unexpected{std::format("Failed to get offset of mip {} in {}", mip, path.string())};

        copyRegions.push_back({
            .bufferOffset = mipOffset,
            .imageSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = mip, .layerCount = 1},
            .imageExtent{.width = ktx->baseWidth >> mip, .height = ktx->baseHeight >> mip, .depth = 1},
        });
    }

    VkImageCreateInfo imageCI{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                              .imageType = VK_IMAGE_TYPE_2D,
                              .format = ktxTexture_GetVkFormat(ktx.get()),
                              .extent{.width = ktx->baseWidth, .height = ktx->baseHeight, .depth = 1},
                              .mipLevels = ktx->numLevels,
                              .arrayLayers = 1,
                              .samples = VK_SAMPLE_COUNT_1_BIT,
                              .tiling = VK_IMAGE_TILING_OPTIMAL,
                              .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    Image image{allocator, imageCI, VK_IMAGE_ASPECT_COLOR_BIT};

    Buffer stagingBuffer{allocator, ktx->dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT};
    stagingBuffer.write(std::span{ktx->pData, ktx->dataSize});

    commandPool.submitImmediate(
        [&](VkCommandBuffer cb)
        {
            VkImageMemoryBarrier2 barrierTexImage{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                  .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                                                  .srcAccessMask = VK_ACCESS_2_NONE,
                                                  .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                                  .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                  .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                  .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                  .image = image.handle(),
                                                  .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                    .levelCount = ktx->numLevels,
                                                                    .layerCount = 1}};
            VkDependencyInfo barrierTexInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                            .imageMemoryBarrierCount = 1,
                                            .pImageMemoryBarriers = &barrierTexImage};
            vkCmdPipelineBarrier2(cb, &barrierTexInfo);

            vkCmdCopyBufferToImage(cb, stagingBuffer.handle(), image.handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                   static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

            VkImageMemoryBarrier2 barrierTexRead{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                 .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                                 .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                 .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                 .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                                                 .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                 .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                                 .image = image.handle(),
                                                 .subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                   .levelCount = ktx->numLevels,
                                                                   .layerCount = 1}};
            barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
            vkCmdPipelineBarrier2(cb, &barrierTexInfo);
        });

    VkSamplerCreateInfo samplerCI{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                  .magFilter = VK_FILTER_LINEAR,
                                  .minFilter = VK_FILTER_LINEAR,
                                  .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                  .anisotropyEnable = VK_TRUE,
                                  .maxAnisotropy = 8.0f,
                                  .maxLod = static_cast<float>(ktx->numLevels)};
    VkSampler sampler{VK_NULL_HANDLE};
    chk(vkCreateSampler(device.handle(), &samplerCI, nullptr, &sampler));

    return Texture{std::move(image), DeviceHandle<VkSampler>{device.handle(), sampler}};
}
