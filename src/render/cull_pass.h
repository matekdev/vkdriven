#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include "vk/buffer.h"
#include "vk/compute_pipeline.h"

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

class Allocator;
class Device;
class PipelineCache;
class Scene;
class ShaderCompiler;

// Below 1, the cull frustum is narrower than the one the camera renders with, so draws visibly get culled
// before they reach the screen edges.
struct CullSettings
{
    float fovScale{1.0f};
    bool showFrustum{true};
};

struct CullConstants
{
    VkDeviceAddress frame;
    VkDeviceAddress draws;
    VkDeviceAddress transforms;
    VkDeviceAddress primitives;
    VkDeviceAddress commands;
    VkDeviceAddress commandCount;
    uint32_t drawCount;
};

// The six planes bounding what viewProjection maps into clip space, in the space it maps from, with normals
// pointing inwards. Order: the two x planes, the two y planes, then the two depth planes.
std::array<glm::vec4, 6> frustumPlanes(const glm::mat4& viewProjection);

// Tests every draw's bounding sphere against the camera frustum on the GPU, and writes a
// VkDrawIndexedIndirectCommand for each visible one plus how many there are, ready for an indirect draw.
// Throws std::runtime_error from the constructor if the shader fails to compile.
class CullPass
{
  public:
    static constexpr uint32_t workgroupSize = 64;

    CullPass(const Device& device, const Allocator& allocator, const ShaderCompiler& shaderCompiler,
             const PipelineCache& pipelineCache, const Scene& scene, std::filesystem::path shaderPath);

    // Keeps the current pipeline if the shader fails to compile.
    void reloadShaders();

    // Also copies the visible count into visibleDrawCount, a host-visible buffer the CPU reads after the fence.
    void record(VkCommandBuffer cb, const Scene& scene, VkDeviceAddress frameData,
                const Buffer& visibleDrawCount) const;

    [[nodiscard]] VkBuffer commandBuffer() const
    {
        return commands_.handle();
    }

    [[nodiscard]] VkBuffer countBuffer() const
    {
        return count_.handle();
    }

  private:
    [[nodiscard]] std::expected<ComputePipeline, std::string> build() const;

    const Device& device_;
    const ShaderCompiler& shaderCompiler_;
    const PipelineCache& pipelineCache_;
    std::filesystem::path shaderPath_;
    ComputePipeline pipeline_;
    Buffer commands_;
    Buffer count_;
};
