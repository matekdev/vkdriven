#include "ui/panels.h"

#include <IconsFontAwesome6.h>

#include "profiling/benchmark.h"
#include "render/cull_pass.h"
#include "render/shadow_pass.h"
#include "render/tonemap_pass.h"
#include "scene/camera.h"
#include "scene/light.h"
#include "scene/scene.h"
#include "ui/viewport_target.h"
#include "vk/gpu_timer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>

namespace
{

// The cull frustum shares the camera's position and direction, so from the camera its side planes are a centered
// rectangle on screen. The fraction of the screen it covers is the ratio of the two frustums' tan(fov / 2), and
// it's the same horizontally because both use the same aspect ratio.
void drawCullFrustumOverlay(ImVec2 imageMin, ImVec2 imageMax, const Camera& camera, float fovScale)
{
    const float halfFov = camera.verticalFovRadians() / 2.0f;
    const float coverage = std::tan(halfFov * fovScale) / std::tan(halfFov);
    const ImVec2 center{(imageMin.x + imageMax.x) / 2.0f, (imageMin.y + imageMax.y) / 2.0f};
    const ImVec2 halfSize{(imageMax.x - imageMin.x) / 2.0f * coverage, (imageMax.y - imageMin.y) / 2.0f * coverage};
    ImGui::GetWindowDrawList()->AddRect({center.x - halfSize.x, center.y - halfSize.y},
                                        {center.x + halfSize.x, center.y + halfSize.y}, IM_COL32(255, 200, 0, 255),
                                        0.0f, ImDrawFlags_None, 2.0f);
}

} // namespace

ViewportPanelState drawViewportPanel(ImGuiID dockspace, const ViewportTarget& viewport, const Camera& camera,
                                     const CullSettings& cullSettings)
{
    ViewportPanelState state;

    ImGui::SetNextWindowDockID(dockspace, ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    if (ImGui::Begin(ICON_FA_CUBE " Viewport###Viewport"))
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        state.requestedExtent = VkExtent2D{.width = static_cast<uint32_t>(std::max(available.x, 0.0f)),
                                           .height = static_cast<uint32_t>(std::max(available.y, 0.0f))};
        // Normally the image matches the panel. When it's rendered at a fixed size instead (the benchmark), it's
        // scaled to fit and keeps its aspect ratio.
        const VkExtent2D extent = viewport.extent();
        const ImVec2 imageSize{static_cast<float>(extent.width), static_cast<float>(extent.height)};
        const float scale = std::min({available.x / imageSize.x, available.y / imageSize.y, 1.0f});
        ImGui::Image(viewport.texture(), ImVec2{imageSize.x * scale, imageSize.y * scale});
        state.hovered = ImGui::IsItemHovered();
        if (cullSettings.showFrustum && cullSettings.fovScale < 1.0f)
            drawCullFrustumOverlay(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), camera, cullSettings.fovScale);
    }
    ImGui::End();
    ImGui::PopStyleVar();

    return state;
}

void drawStatsPanel(const Scene& scene, const ViewportTarget& viewport, std::span<const GpuTiming> gpuTimings,
                    uint32_t visibleDrawCount, float cpuRecordMilliseconds, const Benchmark& benchmark)
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Begin(ICON_FA_CHART_SIMPLE " Stats###Stats");
    ImGui::Text("%.2f ms (%.0f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("Viewport: %u x %u", viewport.extent().width, viewport.extent().height);
    if (benchmark.running())
        ImGui::ProgressBar(benchmark.progress(), ImVec2{-1.0f, 0.0f}, benchmark.currentView().name);

    ImGui::Separator();
    ImGui::Text("CPU record %5.2f ms", cpuRecordMilliseconds);
    ImGui::Text("GPU time");
    for (const GpuTiming& timing : gpuTimings)
        ImGui::Text("  %-8s %5.2f ms", timing.name, timing.milliseconds);
    ImGui::Separator();
    ImGui::Text("Draws: %u visible / %zu", visibleDrawCount, scene.draws().size());
    ImGui::Text("Primitives: %zu", scene.primitives().size());
    ImGui::Text("Vertices: %zu", static_cast<size_t>(scene.vertexCount()));
    ImGui::Text("Indices: %zu", static_cast<size_t>(scene.indexCount()));
    ImGui::End();
}

void drawCameraPanel(Camera& camera, CullSettings& cullSettings)
{
    ImGui::Begin(ICON_FA_VIDEO " Camera###Camera");
    float verticalFovRadians = camera.verticalFovRadians();
    if (ImGui::SliderAngle("Vertical FOV", &verticalFovRadians, 20.0f, 120.0f))
        camera.setVerticalFovRadians(verticalFovRadians);
    ImGui::Separator();
    ImGui::SliderFloat("Cull FOV scale", &cullSettings.fovScale, 0.1f, 1.0f, "%.2f");
    ImGui::Checkbox("Show cull frustum", &cullSettings.showFrustum);
    ImGui::SetItemTooltip("Culls with a narrower frustum than the camera renders with, so culled draws vanish "
                          "on screen instead of just past its edges.");
    ImGui::End();
}

void drawLightPanel(DirectionalLight& light)
{
    ImGui::Begin(ICON_FA_SUN " Light###Light");
    ImGui::SliderAngle("Azimuth", &light.azimuthRadians, -180.0f, 180.0f);
    ImGui::SliderAngle("Elevation", &light.elevationRadians, -90.0f, 90.0f);
    ImGui::ColorEdit3("Color", &light.color.x);
    ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 10.0f);
    ImGui::End();
}

void drawShadowPanel(ShadowSettings& settings)
{
    ImGui::Begin(ICON_FA_MOON " Shadows###Shadows");
    ImGui::SliderFloat("Constant bias", &settings.constantBias, 0.0f, 10000.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Slope bias", &settings.slopeBias, 0.0f, 10.0f, "%.2f");
    ImGui::SliderFloat("Normal offset", &settings.normalOffset, 0.0f, 5.0f, "%.2f texels");
    ImGui::Separator();
    ImGui::SliderFloat("Distance", &settings.distance, 5.0f, 100.0f, "%.0f m");
    ImGui::SliderFloat("Split lambda", &settings.splitLambda, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Show cascades", &settings.showCascades);
    ImGui::End();
}

void drawTonemapPanel(TonemapSettings& settings)
{
    constexpr auto tonemapperNames =
        std::to_array<const char*>({"Exponential", "Reinhard", "ACES", "AgX", "Khronos PBR Neutral"});

    ImGui::Begin(ICON_FA_CAMERA " Tonemapping###Tonemapping");
    int tonemapper = static_cast<int>(settings.tonemapper);
    if (ImGui::Combo("Tonemapper", &tonemapper, tonemapperNames.data(), static_cast<int>(tonemapperNames.size())))
        settings.tonemapper = static_cast<Tonemapper>(tonemapper);
    ImGui::SliderFloat("Exposure", &settings.exposure, 0.01f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::End();
}
