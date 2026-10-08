#include "render/scene_draws.h"

#include "render/scene_pass.h"
#include "scene/scene.h"
#include "vk/bindless_textures.h"
#include "vk/shader_pipeline.h"

#include <cstddef>
#include <cstdint>
#include <span>

void recordSceneDraws(VkCommandBuffer cb, const ShaderPipeline& pipeline, const BindlessTextures& textures,
                      const Scene& scene, VkDeviceAddress frameData)
{
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());
    const VkDescriptorSet textureSet = textures.set();
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout(), 0, 1, &textureSet, 0, nullptr);
    const VkDeviceSize vertexOffset{0};
    const VkBuffer vertexBuffer = scene.vertexBuffer();
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer, &vertexOffset);
    vkCmdBindIndexBuffer(cb, scene.indexBuffer(), 0, Scene::indexType);

    constexpr VkShaderStageFlags pushStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    const DrawConstants constants{.frame = frameData,
                                  .draws = scene.drawBufferAddress(),
                                  .transforms = scene.transformBufferAddress(),
                                  .materials = scene.materialBufferAddress()};
    vkCmdPushConstants(cb, pipeline.layout(), pushStages, 0, sizeof(DrawConstants), &constants);

    const std::span<const Draw> draws = scene.draws();
    for (uint32_t drawIndex = 0; drawIndex < draws.size(); drawIndex++)
    {
        const Primitive& primitive = scene.primitives()[draws[drawIndex].primitiveIndex];
        vkCmdPushConstants(cb, pipeline.layout(), pushStages, offsetof(DrawConstants, drawIndex), sizeof(drawIndex),
                           &drawIndex);
        vkCmdDrawIndexed(cb, primitive.indexCount, 1, primitive.firstIndex, primitive.vertexOffset, 0);
    }
}
