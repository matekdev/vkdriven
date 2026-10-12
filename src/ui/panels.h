#pragma once

#include <volk.h>

#include <imgui.h>

#include <optional>
#include <span>

class Benchmark;
class Camera;
class Scene;
class ViewportTarget;
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

// Shows the viewport image.
ViewportPanelState drawViewportPanel(ImGuiID dockspace, const ViewportTarget& viewport);
// Shows a progress bar while the benchmark (run with --benchmark) is going.
void drawStatsPanel(const Scene& scene, const ViewportTarget& viewport, const Camera& camera,
                    std::span<const GpuTiming> gpuTimings, float cpuRecordMilliseconds, const Benchmark& benchmark);
void drawLightPanel(DirectionalLight& light);
void drawShadowPanel(ShadowSettings& settings);
void drawTonemapPanel(TonemapSettings& settings);
