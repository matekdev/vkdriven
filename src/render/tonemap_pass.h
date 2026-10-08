#pragma once

#include <volk.h>

#include "vk/shader_pipeline.h"

#include <cstdint>
#include <filesystem>

class BindlessTextures;
class Device;
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

struct TonemapSettings
{
    float exposure{3.5f};
    Tonemapper tonemapper{Tonemapper::Reinhard};
};

struct TonemapConstants
{
    uint32_t hdrTextureIndex;
    float exposure;
    Tonemapper tonemapper;
};

// Tonemaps the viewport's HDR image into its color image, then leaves the color image ready to be sampled.
// The HDR image is read through the bindless set at hdrTextureIndex.
// Throws std::runtime_error from the constructor if the shader fails to compile.
class TonemapPass
{
  public:
    TonemapPass(const Device& device, const ShaderCompiler& shaderCompiler, const BindlessTextures& textures,
                std::filesystem::path shaderPath);

    // Keeps the current pipeline if the shader fails to compile.
    void reloadShaders()
    {
        pipeline_.reload();
    }

    void record(VkCommandBuffer cb, const ViewportTarget& target, uint32_t hdrTextureIndex,
                const TonemapSettings& settings) const;

  private:
    const BindlessTextures& textures_;
    ShaderPipeline pipeline_;
};
