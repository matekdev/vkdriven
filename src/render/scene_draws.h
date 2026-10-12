#pragma once

#include <volk.h>

class BindlessTextures;
class CullPass;
class Scene;
class ShaderPipeline;

#include <cstdint>

// Binds the pipeline, the bindless textures and the scene's geometry, then draws every draw in the scene.
// Must be called inside vkCmdBeginRendering, after the viewport and scissor are set.
// cascadeIndex selects the light matrix when drawing into a shadow cascade.
void recordSceneDraws(VkCommandBuffer cb, const ShaderPipeline& pipeline, const BindlessTextures& textures,
                      const Scene& scene, VkDeviceAddress frameData, uint32_t cascadeIndex = 0);

// The same, but only draws what the cull pass found visible, with one indirect draw.
void recordCulledSceneDraws(VkCommandBuffer cb, const ShaderPipeline& pipeline, const BindlessTextures& textures,
                            const Scene& scene, VkDeviceAddress frameData, const CullPass& cullPass);
