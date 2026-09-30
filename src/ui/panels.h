#pragma once

#include <volk.h>

#include <imgui.h>

#include <optional>

class Scene;
class ViewportTarget;

// Shows the viewport image. Returns the size the image should be for next frame, or nothing while
// the window is hidden.
std::optional<VkExtent2D> drawViewportPanel(ImGuiID dockspace, const ViewportTarget& viewport);
void drawStatsPanel(const Scene& scene, const ViewportTarget& viewport);
