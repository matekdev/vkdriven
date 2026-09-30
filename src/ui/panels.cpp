#include "ui/panels.h"

#include <IconsFontAwesome6.h>

#include "scene/scene.h"
#include "ui/viewport_target.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

std::optional<VkExtent2D> drawViewportPanel(ImGuiID dockspace, const ViewportTarget& viewport)
{
    std::optional<VkExtent2D> requestedExtent;

    ImGui::SetNextWindowDockID(dockspace, ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    if (ImGui::Begin(ICON_FA_CUBE " Viewport###Viewport"))
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        requestedExtent = VkExtent2D{.width = static_cast<uint32_t>(std::max(available.x, 0.0f)),
                                     .height = static_cast<uint32_t>(std::max(available.y, 0.0f))};
        const VkExtent2D extent = viewport.extent();
        ImGui::Image(viewport.texture(), ImVec2{static_cast<float>(extent.width), static_cast<float>(extent.height)});
    }
    ImGui::End();
    ImGui::PopStyleVar();

    return requestedExtent;
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
