#pragma once

#include <glm/glm.hpp>

class Window;

// Fly camera with smoothed movement and look. Yaw 0 and pitch 0 look down -Z, matching glTF's forward direction.
// Hold the right mouse button over the viewport to look around, and use WASD (plus Q/E for down/up) to fly.
// Shift moves faster.
class Camera
{
  public:
    Camera(glm::vec3 position, float yawRadians, float pitchRadians);

    // Call every frame between ImGui's NewFrame and Render, so the camera can glide to a stop when idle.
    void update(const Window& window, bool viewportHovered, glm::vec2 mouseDelta);

    [[nodiscard]] glm::mat4 view() const;
    [[nodiscard]] glm::mat4 projection(float aspect) const;

  private:
    [[nodiscard]] bool updateLooking(const Window& window, bool viewportHovered);
    [[nodiscard]] glm::vec3 forward() const;

    glm::vec3 position_;
    float yaw_;
    float pitch_;
    glm::vec3 velocity_{};
    glm::vec2 pendingLook_{};
    bool looking_{false};
    float verticalFovRadians_{glm::radians(60.0f)};
    float nearPlane_{0.1f};
    float farPlane_{100.0f};
};
