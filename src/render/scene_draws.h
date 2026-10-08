#pragma once

#include <volk.h>

class BindlessTextures;
class Scene;
class ShaderPipeline;

// Binds the pipeline, the bindless textures and the scene's geometry, then draws every draw in the scene.
// Must be called inside vkCmdBeginRendering, after the viewport and scissor are set.
void recordSceneDraws(VkCommandBuffer cb, const ShaderPipeline& pipeline, const BindlessTextures& textures,
                      const Scene& scene, VkDeviceAddress frameData);
