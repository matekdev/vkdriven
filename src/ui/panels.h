#pragma once

#include <volk.h>

#include <imgui.h>

#include <optional>

class Scene;
class ViewportTarget;
struct DirectionalLight;

struct ViewportPanelState
{
    // The size the image should be for next frame, or nothing while the window is hidden.
    std::optional<VkExtent2D> requestedExtent;
    bool hovered{false};
};

// Shows the viewport image.
ViewportPanelState drawViewportPanel(ImGuiID dockspace, const ViewportTarget& viewport);
void drawStatsPanel(const Scene& scene, const ViewportTarget& viewport);
void drawLightPanel(DirectionalLight& light);
void drawTonemapPanel(float& exposure);
