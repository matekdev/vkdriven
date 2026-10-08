#pragma once

#include <volk.h>

#include <imgui.h>

#include "vk/handle.h"
#include "vk/image.h"

class Allocator;

// Offscreen color + depth images the scene renders into, shown in the ImGui "Viewport" window.
// The color image is registered with the ImGui Vulkan backend, so this must be destroyed before it.
class ViewportTarget
{
  public:
    // The scene renders through the sRGB format so its linear output gets encoded. ImGui samples the
    // same bytes through the UNORM format so they reach the UNORM swapchain without being decoded.
    static constexpr VkFormat colorFormat = VK_FORMAT_R8G8B8A8_SRGB;
    static constexpr VkFormat displayFormat = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    ViewportTarget(const Allocator& allocator, VkFormat depthFormat, VkExtent2D extent);
    ~ViewportTarget();

    ViewportTarget(const ViewportTarget&) = delete;
    ViewportTarget& operator=(const ViewportTarget&) = delete;
    ViewportTarget(ViewportTarget&&) = delete;
    ViewportTarget& operator=(ViewportTarget&&) = delete;

    // True if the extent is non-zero and differs from the current size.
    [[nodiscard]] bool needsResize(VkExtent2D extent) const;

    // Recreates both images at the new size. The GPU must not be using the old ones.
    void resize(VkExtent2D extent);

    [[nodiscard]] VkExtent2D extent() const
    {
        return extent_;
    }

    [[nodiscard]] const Image& color() const
    {
        return color_;
    }

    [[nodiscard]] const Image& depth() const
    {
        return depth_;
    }

    [[nodiscard]] const Image& hdr() const
    {
        return hdr_;
    }

    [[nodiscard]] VkDescriptorImageInfo hdrDescriptorInfo() const
    {
        return {.sampler = hdrSampler_.get(),
                .imageView = hdr_.view(),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    }

    [[nodiscard]] ImTextureID texture() const;

  private:
    void create(VkExtent2D extent);
    void destroy();

    const Allocator& allocator_;
    VkFormat depthFormat_;
    VkExtent2D extent_{};
    Image color_;
    Image depth_;
    Image hdr_;
    DeviceHandle<VkSampler> hdrSampler_;
    DeviceHandle<VkImageView> displayView_;
    VkDescriptorSet texture_{VK_NULL_HANDLE};
};
