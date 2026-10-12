#pragma once

#include <volk.h>

#include <imgui.h>

#include <cstdint>
#include <optional>
#include <span>

class Benchmark;
class Camera;
class Scene;
class ViewportTarget;
struct CullSettings;
struct DirectionalLight;
struct ShadowSettings;
struct TonemapSettings;
struct GpuTiming;

struct ViewportPanelState
{
    // The size the image should be for next frame, or nothing while the window is hidden.
    std::optional<VkExtent2D> requestedExtent;
    bool hovered{false};
};

// Shows the viewport image, outlined with the cull frustum when it's narrower than the camera's.
ViewportPanelState drawViewportPanel(ImGuiID dockspace, const ViewportTarget& viewport, const Camera& camera,
                                     const CullSettings& cullSettings);
// Shows a progress bar while the benchmark (run with --benchmark) is going.
void drawStatsPanel(const Scene& scene, const ViewportTarget& viewport, std::span<const GpuTiming> gpuTimings,
                    uint32_t visibleDrawCount, float cpuRecordMilliseconds, const Benchmark& benchmark);
void drawCameraPanel(Camera& camera, CullSettings& cullSettings);
void drawLightPanel(DirectionalLight& light);
void drawShadowPanel(ShadowSettings& settings);
void drawTonemapPanel(TonemapSettings& settings);
