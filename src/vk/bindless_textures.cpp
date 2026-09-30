#include "vk/bindless_textures.h"

#include "vk/check.h"
#include "vk/device.h"
#include "vk/texture.h"

#include <cassert>
#include <ranges>
#include <vector>

BindlessTextures::BindlessTextures(const Device& device, uint32_t capacity)
    : device_{device.handle()}, capacity_{capacity}
{
    VkDescriptorBindingFlags bindingFlags{VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &bindingFlags};
    VkDescriptorSetLayoutBinding binding{.binding = 0,
                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                         .descriptorCount = capacity,
                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo layoutCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                             .pNext = &bindingFlagsCI,
                                             .bindingCount = 1,
                                             .pBindings = &binding};
    VkDescriptorSetLayout layout{VK_NULL_HANDLE};
    chk(vkCreateDescriptorSetLayout(device_, &layoutCI, nullptr, &layout));
    layout_ = DeviceHandle<VkDescriptorSetLayout>{device_, layout};

    VkDescriptorPoolSize poolSize{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = capacity};
    VkDescriptorPoolCreateInfo poolCI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                      .maxSets = 1,
                                      .poolSizeCount = 1,
                                      .pPoolSizes = &poolSize};
    VkDescriptorPool pool{VK_NULL_HANDLE};
    chk(vkCreateDescriptorPool(device_, &poolCI, nullptr, &pool));
    pool_ = DeviceHandle<VkDescriptorPool>{device_, pool};

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountAI{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &capacity};
    VkDescriptorSetAllocateInfo setAI{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                      .pNext = &variableCountAI,
                                      .descriptorPool = pool_.get(),
                                      .descriptorSetCount = 1,
                                      .pSetLayouts = layout_.ptr()};
    chk(vkAllocateDescriptorSets(device_, &setAI, &set_));
}

void BindlessTextures::write(std::span<const Texture> textures, uint32_t firstIndex) const
{
    assert(firstIndex + textures.size() <= capacity_);

    const auto imageInfos = textures |
                            std::views::transform([](const Texture& texture) { return texture.descriptorInfo(); }) |
                            std::ranges::to<std::vector>();
    VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                               .dstSet = set_,
                               .dstBinding = 0,
                               .dstArrayElement = firstIndex,
                               .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
                               .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               .pImageInfo = imageInfos.data()};
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
}
