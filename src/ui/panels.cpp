#include "ui/panels.h"

#include <IconsFontAwesome6.h>

#include "scene/light.h"
#include "scene/scene.h"
#include "ui/viewport_target.h"

#include <algorithm>
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

void drawTonemapPanel(float& exposure)
{
    ImGui::Begin(ICON_FA_CAMERA " Tonemapping###Tonemapping");
    ImGui::SliderFloat("Exposure", &exposure, 0.01f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::End();
}
