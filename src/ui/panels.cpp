#include "ui/panels.h"

#include <IconsFontAwesome6.h>

#include "render/shadow_pass.h"
#include "render/tonemap_pass.h"
#include "scene/light.h"
#include "scene/scene.h"
#include "ui/viewport_target.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

ViewportPanelState drawViewportPanel(ImGuiID dockspace, const ViewportTarget& viewport)
{
    ViewportPanelState state;

    ImGui::SetNextWindowDockID(dockspace, ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    if (ImGui::Begin(ICON_FA_CUBE " Viewport###Viewport"))
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        state.requestedExtent = VkExtent2D{.width = static_cast<uint32_t>(std::max(available.x, 0.0f)),
                                           .height = static_cast<uint32_t>(std::max(available.y, 0.0f))};
        const VkExtent2D extent = viewport.extent();
        ImGui::Image(viewport.texture(), ImVec2{static_cast<float>(extent.width), static_cast<float>(extent.height)});
        state.hovered = ImGui::IsItemHovered();
    }
    ImGui::End();
    ImGui::PopStyleVar();

    return state;
}

void drawStatsPanel(const Scene& scene, const ViewportTarget& viewport)
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Begin(ICON_FA_CHART_SIMPLE " Stats###Stats");
    ImGui::Text("%.2f ms (%.0f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("Viewport: %u x %u", viewport.extent().width, viewport.extent().height);
    ImGui::Separator();
    ImGui::Text("Draws: %zu", scene.draws().size());
    ImGui::Text("Primitives: %zu", scene.primitives().size());
    ImGui::Text("Vertices: %zu", static_cast<size_t>(scene.vertexCount()));
    ImGui::Text("Indices: %zu", static_cast<size_t>(scene.indexCount()));
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
