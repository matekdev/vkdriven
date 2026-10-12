#pragma once

#include <volk.h>

#include "vk/shader_pipeline.h"

#include <cstdint>
#include <filesystem>

class BindlessTextures;
class Device;
class PipelineCache;
class ShaderCompiler;
class ViewportTarget;

// Must match the TONEMAPPER_* constants in tonemap.slang.
enum class Tonemapper : uint32_t
{
    Exponential,
    Reinhard,
    Aces,
    Agx,
    PbrNeutral
};

// Must match the VIEW_* constants in tonemap.slang.
enum class ViewportView : uint32_t
{
    Lit,
    Normals,
    MotionVectors
};

struct TonemapSettings
{
    float exposure{3.5f};
    Tonemapper tonemapper{Tonemapper::Reinhard};
    ViewportView view{ViewportView::Lit};
    // Motion vectors are fractions of a pixel to a few pixels, so they're scaled up to be visible.
    float motionScale{50.0f};
};

// Bindless indices of the viewport images the tonemap pass reads.
struct ViewportTextureIndices
{
    uint32_t hdr;
    uint32_t normal;
    uint32_t motion;
};

struct TonemapConstants
{
    ViewportTextureIndices textures;
    float exposure;
    Tonemapper tonemapper;
    ViewportView view;
    float motionScale;
};

// Tonemaps the viewport's HDR image into its color image, then leaves the color image ready to be sampled.
// The debug views show the depth prepass's normals or motion vectors instead.
// Throws std::runtime_error from the constructor if the shader fails to compile.
class TonemapPass
{
  public:
    TonemapPass(const Device& device, const ShaderCompiler& shaderCompiler, const PipelineCache& pipelineCache,
                const BindlessTextures& textures, std::filesystem::path shaderPath);

    // Keeps the current pipeline if the shader fails to compile.
    void reloadShaders()
    {
        pipeline_.reload();
    }

    void record(VkCommandBuffer cb, const ViewportTarget& target, const ViewportTextureIndices& textureIndices,
                const TonemapSettings& settings) const;

  private:
    const BindlessTextures& textures_;
    ShaderPipeline pipeline_;
};
