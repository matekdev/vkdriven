#include "render/image_based_lighting.h"

#include "util/expected.h"
#include "vk/allocator.h"
#include "vk/check.h"
#include "vk/command_pool.h"
#include "vk/compute_pipeline.h"
#include "vk/device.h"
#include "vk/mip_chain.h"
#include "vk/pipeline_cache.h"
#include "vk/shader_compiler.h"
#include "vk/sync.h"
#include "vk/texture.h"

#include <array>
#include <bit>

namespace
{

constexpr uint32_t cubeFaceCount = 6;
constexpr uint32_t workgroupSize = 8;

VkImageSubresourceRange cubeMipRange(uint32_t baseMip, uint32_t mipCount)
{
    return {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = baseMip,
            .levelCount = mipCount,
            .layerCount = cubeFaceCount};
}

Image createCube(const Allocator& allocator, uint32_t size, uint32_t mipLevels, VkImageUsageFlags usage)
{
    VkImageCreateInfo imageCI{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                              .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                              .imageType = VK_IMAGE_TYPE_2D,
                              .format = ImageBasedLighting::format,
                              .extent{.width = size, .height = size, .depth = 1},
                              .mipLevels = mipLevels,
                              .arrayLayers = cubeFaceCount,
                              .samples = VK_SAMPLE_COUNT_1_BIT,
                              .tiling = VK_IMAGE_TILING_OPTIMAL,
                              .usage = usage,
                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    return Image{allocator, imageCI, VK_IMAGE_ASPECT_COLOR_BIT};
}

// Compute writes a cube one mip at a time through a 2D array view (one layer per face);
// sampling by direction needs a cube view.
DeviceHandle<VkImageView> createView(const Device& device, const Image& cube, VkImageViewType viewType,
                                     uint32_t mipCount)
{
    VkImageViewCreateInfo viewCI{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                 .image = cube.handle(),
                                 .viewType = viewType,
                                 .format = cube.format(),
                                 .subresourceRange = cubeMipRange(0, mipCount)};
    VkImageView view{VK_NULL_HANDLE};
    chk(vkCreateImageView(device.handle(), &viewCI, nullptr, &view));
    return {device.handle(), view};
}

DeviceHandle<VkSampler> createCubeSampler(const Device& device)
{
    VkSamplerCreateInfo samplerCI{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                  .magFilter = VK_FILTER_LINEAR,
                                  .minFilter = VK_FILTER_LINEAR,
                                  .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                  .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                  .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                  .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                  .maxLod = VK_LOD_CLAMP_NONE};
    VkSampler sampler{VK_NULL_HANDLE};
    chk(vkCreateSampler(device.handle(), &samplerCI, nullptr, &sampler));
    return {device.handle(), sampler};
}

// Binding 0: the texture to read. Binding 1: the storage image to write. Pushed straight into the
// command buffer with vkCmdPushDescriptorSetKHR, so there's no pool or set to allocate.
DeviceHandle<VkDescriptorSetLayout> createReadWriteLayout(const Device& device)
{
    const auto bindings = std::to_array<VkDescriptorSetLayoutBinding>({
        {.binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
        {.binding = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT},
    });
    VkDescriptorSetLayoutCreateInfo layoutCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                             .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                                             .bindingCount = static_cast<uint32_t>(bindings.size()),
                                             .pBindings = bindings.data()};
    VkDescriptorSetLayout layout{VK_NULL_HANDLE};
    chk(vkCreateDescriptorSetLayout(device.handle(), &layoutCI, nullptr, &layout));
    return {device.handle(), layout};
}

// Runs a read-one-texture, write-one-cube shader with one thread per texel of every face.
void dispatchToCube(VkCommandBuffer cb, const ComputePipeline& pipeline, const VkDescriptorImageInfo& input,
                    VkImageView output, uint32_t outputSize)
{
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.handle());

    const VkDescriptorImageInfo outputInfo{.imageView = output, .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
    const auto writes = std::to_array<VkWriteDescriptorSet>({
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstBinding = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         .pImageInfo = &input},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .dstBinding = 1,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .pImageInfo = &outputInfo},
    });
    vkCmdPushDescriptorSetKHR(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.layout(), 0,
                              static_cast<uint32_t>(writes.size()), writes.data());

    const uint32_t groupCount = (outputSize + workgroupSize - 1) / workgroupSize;
    vkCmdDispatch(cb, groupCount, groupCount, cubeFaceCount);
}

} // namespace

ImageBasedLighting::ImageBasedLighting(const Device& device, const Allocator& allocator,
                                       const CommandPool& commandPool, const ShaderCompiler& shaderCompiler,
                                       const PipelineCache& pipelineCache, const std::filesystem::path& equirectPath,
                                       const std::filesystem::path& shaderDirectory)
    : environment_{createCube(allocator, environmentSize, std::bit_width(environmentSize),
                              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)},
      environmentCubeView_{createView(device, environment_, VK_IMAGE_VIEW_TYPE_CUBE, environment_.mipLevels())},
      irradiance_{createCube(allocator, irradianceSize, 1, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)},
      irradianceCubeView_{createView(device, irradiance_, VK_IMAGE_VIEW_TYPE_CUBE, 1)},
      sampler_{createCubeSampler(device)}
{
    const Texture equirect = orThrow(Texture::loadHdr(device, allocator, commandPool, equirectPath));
    const DeviceHandle<VkImageView> environmentMip0View =
        createView(device, environment_, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1);

    const DeviceHandle<VkDescriptorSetLayout> setLayout = createReadWriteLayout(device);
    const DeviceHandle<VkShaderModule> equirectToCubeShader =
        orThrow(shaderCompiler.compile(device, shaderDirectory / "equirect_to_cube.slang"));
    const ComputePipeline equirectToCube{
        device, pipelineCache.handle(), equirectToCubeShader.get(), {.setLayout = setLayout.get()}};
    const DeviceHandle<VkShaderModule> irradianceShader =
        orThrow(shaderCompiler.compile(device, shaderDirectory / "irradiance.slang"));
    const ComputePipeline convolveIrradiance{
        device, pipelineCache.handle(), irradianceShader.get(), {.setLayout = setLayout.get()}};

    commandPool.submitImmediate(
        [&](VkCommandBuffer cb)
        {
            // Nothing has used either cube yet. Storage image writes need GENERAL; the environment's
            // lower mips are filled by blits later, so they start as blit destinations.
            pipelineBarrier(cb, std::to_array<VkImageMemoryBarrier2>({
                                    {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                     .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                                     .srcAccessMask = VK_ACCESS_2_NONE,
                                     .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                     .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                     .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                     .image = environment_.handle(),
                                     .subresourceRange = cubeMipRange(0, 1)},
                                    {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                     .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                                     .srcAccessMask = VK_ACCESS_2_NONE,
                                     .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                     .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                     .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                     .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     .image = environment_.handle(),
                                     .subresourceRange = cubeMipRange(1, environment_.mipLevels() - 1)},
                                    {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                     .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                                     .srcAccessMask = VK_ACCESS_2_NONE,
                                     .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                     .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                     .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                     .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                                     .image = irradiance_.handle(),
                                     .subresourceRange = cubeMipRange(0, 1)},
                                }));

            dispatchToCube(cb, equirectToCube, equirect.descriptorInfo(), environmentMip0View.get(), environmentSize);

            // Mip 0 is finished; the blits that build the other mips read it.
            pipelineBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                 .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                 .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                 .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                 .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                                 .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                 .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 .image = environment_.handle(),
                                 .subresourceRange = cubeMipRange(0, 1)});
            recordMipChain(cb, environment_.handle(), {.width = environmentSize, .height = environmentSize},
                           environment_.mipLevels(), cubeFaceCount);

            // recordMipChain left the whole environment readable by compute, so the convolution can sample it.
            dispatchToCube(cb, convolveIrradiance, environmentDescriptorInfo(), irradiance_.view(), irradianceSize);

            pipelineBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                 .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                 .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                 .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                 .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                 .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                                 .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
                                 .image = irradiance_.handle(),
                                 .subresourceRange = cubeMipRange(0, 1)});
        });
}
