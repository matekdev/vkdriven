#include "render/cull_pass.h"

#include "scene/scene.h"
#include "util/expected.h"
#include "vk/device.h"
#include "vk/pipeline_cache.h"
#include "vk/shader_compiler.h"
#include "vk/sync.h"

#include <print>
#include <utility>

static_assert(sizeof(VkDrawIndexedIndirectCommand) == 20, "Must match DrawCommand in shaders/cull.slang");

// A clip-space point is inside when -w <= x <= w, -w <= y <= w and 0 <= z <= w. Each of those six
// inequalities is a plane: with rows r of viewProjection, x <= w becomes dot(r3 - r0, p) >= 0 (Gribb-Hartmann).
std::array<glm::vec4, 6> frustumPlanes(const glm::mat4& viewProjection)
{
    const glm::mat4 rows = glm::transpose(viewProjection);
    std::array<glm::vec4, 6> planes{rows[3] + rows[0], rows[3] - rows[0], rows[3] + rows[1],
                                    rows[3] - rows[1], rows[2],           rows[3] - rows[2]};
    for (glm::vec4& plane : planes)
        plane /= glm::length(glm::vec3{plane});
    return planes;
}

CullPass::CullPass(const Device& device, const Allocator& allocator, const ShaderCompiler& shaderCompiler,
                   const PipelineCache& pipelineCache, const Scene& scene, std::filesystem::path shaderPath)
    : device_{device}, shaderCompiler_{shaderCompiler}, pipelineCache_{pipelineCache},
      shaderPath_{std::move(shaderPath)}, pipeline_{orThrow(build())},
      commands_{allocator, scene.draws().size() * sizeof(VkDrawIndexedIndirectCommand),
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT},
      count_{allocator, sizeof(uint32_t),
             VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT}
{
}

void CullPass::reloadShaders()
{
    auto reloadedPipeline = build();
    if (!reloadedPipeline)
    {
        std::println(stderr, "{}", reloadedPipeline.error());
        return;
    }
    device_.waitIdle();
    pipeline_ = std::move(*reloadedPipeline);
    std::println("Reloaded {}", shaderPath_.string());
}

std::expected<ComputePipeline, std::string> CullPass::build() const
{
    return shaderCompiler_.compile(device_, shaderPath_)
        .transform(
            [&](const DeviceHandle<VkShaderModule>& shaderModule)
            {
                return ComputePipeline{device_, pipelineCache_.handle(), shaderModule.get(),
                                       {.pushConstantSize = sizeof(CullConstants)}};
            });
}

void CullPass::record(VkCommandBuffer cb, const Scene& scene, VkDeviceAddress frameData,
                      const Buffer& visibleDrawCount) const
{
    // Both buffers are reused every frame. The previous frame's cull shader wrote them, and its draws and count
    // copy read them, so all of that has to finish before the count is reset and the commands are rewritten.
    pipelineBarrier(cb, VkMemoryBarrier2{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                         .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                                                         VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
                                                         VK_PIPELINE_STAGE_2_COPY_BIT,
                                         .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                         .dstStageMask =
                                             VK_PIPELINE_STAGE_2_CLEAR_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                         .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT |
                                                          VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT});
    vkCmdFillBuffer(cb, count_.handle(), 0, sizeof(uint32_t), 0);

    // The cull shader's atomic add reads and writes the count, so the zero has to land first.
    pipelineBarrier(cb, VkMemoryBarrier2{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                         .srcStageMask = VK_PIPELINE_STAGE_2_CLEAR_BIT,
                                         .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                         .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                         .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                                          VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT});

    const auto drawCount = static_cast<uint32_t>(scene.draws().size());
    const CullConstants constants{.frame = frameData,
                                  .draws = scene.drawBufferAddress(),
                                  .transforms = scene.transformBufferAddress(),
                                  .primitives = scene.primitiveBufferAddress(),
                                  .commands = commands_.deviceAddress(),
                                  .commandCount = count_.deviceAddress(),
                                  .drawCount = drawCount};
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_.handle());
    vkCmdPushConstants(cb, pipeline_.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CullConstants), &constants);
    vkCmdDispatch(cb, (drawCount + workgroupSize - 1) / workgroupSize, 1, 1);

    // The scene pass's indirect draw reads the commands and the count as draw parameters, which happens in
    // the draw-indirect stage at the very start of the graphics pipeline, not in a shader. The count is also
    // copied out for the CPU.
    pipelineBarrier(cb, VkMemoryBarrier2{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                         .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                         .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                         .dstStageMask =
                                             VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_COPY_BIT,
                                         .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |
                                                          VK_ACCESS_2_TRANSFER_READ_BIT});

    const VkBufferCopy countCopy{.size = sizeof(uint32_t)};
    vkCmdCopyBuffer(cb, count_.handle(), visibleDrawCount.handle(), 1, &countCopy);

    // Waiting on the frame's fence doesn't by itself make the copy visible to the host.
    pipelineBarrier(cb, VkMemoryBarrier2{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                         .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                                         .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                         .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                                         .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT});
}
