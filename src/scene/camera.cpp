#include "scene/camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include "platform/window.h"

#include <algorithm>
#include <cmath>

namespace
{

constexpr glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
// Stops just short of straight up/down, where lookAt's up vector becomes parallel to forward.
constexpr float maxPitchRadians = glm::radians(89.0f);

constexpr float lookSensitivity = 0.003f;
constexpr float moveSpeed = 2.0f;
constexpr float fastMoveSpeed = 8.0f;
// Higher is snappier. Movement eases in and out; look only gets a slight smoothing.
constexpr float moveSharpness = 10.0f;
constexpr float lookSharpness = 40.0f;

// Fraction of the remaining distance to cover this frame. Exponential, so it behaves the same at any frame rate.
float smoothingFactor(float sharpness, float deltaSeconds)
{
    return 1.0f - std::exp(-sharpness * deltaSeconds);
}

float keyAxis(ImGuiKey negative, ImGuiKey positive)
{
    return (ImGui::IsKeyDown(positive) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(negative) ? 1.0f : 0.0f);
}

} // namespace

Camera::Camera(glm::vec3 position, float yawRadians, float pitchRadians)
    : position_{position}, yaw_{yawRadians}, pitch_{pitchRadians}
{
}

void Camera::update(const Window& window, bool viewportHovered, glm::vec2 mouseDelta)
{
    const ImGuiIO& io = ImGui::GetIO();
    const bool looking = updateLooking(window, viewportHovered);

    if (looking)
        pendingLook_ += glm::vec2{mouseDelta.x, -mouseDelta.y} * lookSensitivity;
    const glm::vec2 look = pendingLook_ * smoothingFactor(lookSharpness, io.DeltaTime);
    pendingLook_ -= look;
    yaw_ += look.x;
    pitch_ = std::clamp(pitch_ + look.y, -maxPitchRadians, maxPitchRadians);

    glm::vec3 targetVelocity{};
    const glm::vec3 moveDirection{keyAxis(ImGuiKey_A, ImGuiKey_D), keyAxis(ImGuiKey_Q, ImGuiKey_E),
                                  keyAxis(ImGuiKey_S, ImGuiKey_W)};
    if (looking && glm::length(moveDirection) > 0.0f)
    {
        const glm::vec3 direction = glm::normalize(moveDirection);
        const glm::vec3 right = glm::normalize(glm::cross(forward(), worldUp));
        const float speed = io.KeyShift ? fastMoveSpeed : moveSpeed;
        targetVelocity = (right * direction.x + worldUp * direction.y + forward() * direction.z) * speed;
    }
    velocity_ = glm::mix(velocity_, targetVelocity, smoothingFactor(moveSharpness, io.DeltaTime));
    position_ += velocity_ * io.DeltaTime;
}

bool Camera::updateLooking(const Window& window, bool viewportHovered)
{
    const bool looking = looking_ ? ImGui::IsMouseDown(ImGuiMouseButton_Right)
                                  : viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    if (looking == looking_)
        return looking;

    looking_ = looking;
    window.setRelativeMouseMode(looking);
    // Otherwise ImGui's SDL backend shows the cursor again every frame.
    ImGuiIO& io = ImGui::GetIO();
    if (looking)
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    else
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    return looking;
}

glm::mat4 Camera::view() const
{
    return glm::lookAt(position_, position_ + forward(), worldUp);
}

glm::mat4 Camera::projection(float aspect) const
{
    // Reverse-Z: near and far are swapped so depth 1 is the near plane and 0 is the far plane.
    glm::mat4 projection = glm::perspective(verticalFovRadians_, aspect, farPlane_, nearPlane_);
    // glTF is +Y up but Vulkan's clip space has +Y pointing down the screen.
    projection[1][1] *= -1.0f;
    return projection;
}

glm::vec3 Camera::worldPosition() const
{
    return position_;
}

glm::vec3 Camera::forward() const
{
    return {std::cos(pitch_) * std::sin(yaw_), std::sin(pitch_), -std::cos(pitch_) * std::cos(yaw_)};
}
