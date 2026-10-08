#pragma once

#include <volk.h>

inline void setViewportAndScissor(VkCommandBuffer cb, VkExtent2D extent)
{
    VkViewport viewport{.width = static_cast<float>(extent.width),
                        .height = static_cast<float>(extent.height),
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f};
    vkCmdSetViewport(cb, 0, 1, &viewport);
    VkRect2D scissor{.extent = extent};
    vkCmdSetScissor(cb, 0, 1, &scissor);
}
