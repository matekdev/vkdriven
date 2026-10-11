#pragma once

#include <volk.h>

#include <cstdint>

// Records blits that fill mips 1 and up of every layer, each from the mip above it, then leaves every mip in
// READ_ONLY_OPTIMAL for fragment and compute shaders. Expects every mip in TRANSFER_DST_OPTIMAL, with mip 0
// already written by a transfer (or made visible to the transfer stage by a barrier).
void recordMipChain(VkCommandBuffer cb, VkImage image, VkExtent2D extent, uint32_t mipLevels, uint32_t layerCount = 1);
