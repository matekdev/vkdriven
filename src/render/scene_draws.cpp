#include "render/scene_draws.h"

#include "render/cull_pass.h"
#include "render/scene_pass.h"
#include "scene/scene.h"
#include "vk/bindless_textures.h"
#include "vk/shader_pipeline.h"

#include <cstdint>
#include <span>

namespace
{

void bindScene(VkCommandBuffer cb, const ShaderPipeline& pipeline, const BindlessTextures& textures,
               const Scene& scene, VkDeviceAddress frameData, uint32_t cascadeIndex)
{
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());
    const VkDescriptorSet textureSet = textures.set();
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout(), 0, 1, &textureSet, 0, nullptr);
    const VkDeviceSize vertexOffset{0};
    const VkBuffer vertexBuffer = scene.vertexBuffer();
    vkCmdBindVertexBuffers(cb, 0, 1, &vertexBuffer, &vertexOffset);
    vkCmdBindIndexBuffer(cb, scene.indexBuffer(), 0, Scene::indexType);

    const DrawConstants constants{.frame = frameData,
                                  .draws = scene.drawBufferAddress(),
                                  .transforms = scene.transformBufferAddress(),
                                  .materials = scene.materialBufferAddress(),
                                  .cascadeIndex = cascadeIndex};
    vkCmdPushConstants(cb, pipeline.layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(DrawConstants), &constants);
}

} // namespace

// firstInstance carries the draw's index to the shaders, the same way the cull pass's indirect commands do.
void recordSceneDraws(VkCommandBuffer cb, const ShaderPipeline& pipeline, const BindlessTextures& textures,
                      const Scene& scene, VkDeviceAddress frameData, uint32_t cascadeIndex)
{
    bindScene(cb, pipeline, textures, scene, frameData, cascadeIndex);

    const std::span<const Draw> draws = scene.draws();
    for (uint32_t drawIndex = 0; drawIndex < draws.size(); drawIndex++)
    {
        const Primitive& primitive = scene.primitives()[draws[drawIndex].primitiveIndex];
        vkCmdDrawIndexed(cb, primitive.indexCount, 1, primitive.firstIndex, primitive.vertexOffset, drawIndex);
    }
}

// The GPU reads how many commands to run from the count buffer, capped at one per draw in the scene.
void recordCulledSceneDraws(VkCommandBuffer cb, const ShaderPipeline& pipeline, const BindlessTextures& textures,
                            const Scene& scene, VkDeviceAddress frameData, const CullPass& cullPass)
{
    bindScene(cb, pipeline, textures, scene, frameData, 0);
    vkCmdDrawIndexedIndirectCount(cb, cullPass.commandBuffer(), 0, cullPass.countBuffer(), 0,
                                  static_cast<uint32_t>(scene.draws().size()),
                                  sizeof(VkDrawIndexedIndirectCommand));
}
