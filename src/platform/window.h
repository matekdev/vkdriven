#pragma once

#include <volk.h>

#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_video.h>

#include <memory>
#include <span>
#include <vector>

// What happened since the last pollEvents() call.
struct WindowEvents
{
    bool quitRequested{false};
    bool resized{false};
    std::vector<SDL_Keycode> keysPressed;
    float mouseDeltaX{0.0f};
    float mouseDeltaY{0.0f};
};

// SDL window we render into. Also handles SDL init/shutdown, so only create one.
class Window
{
  public:
    Window(const char* title, int width, int height);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] SDL_Window* handle() const
    {
        return window_.get();
    }

    [[nodiscard]] VkExtent2D sizeInPixels() const;
    [[nodiscard]] std::span<const char* const> requiredInstanceExtensions() const;

    // Drains SDL's event queue without blocking, forwarding every event to ImGui.
    [[nodiscard]] WindowEvents pollEvents() const;
    // Sleeps until the next event arrives, e.g. while the window is minimized.
    void waitForEvent() const;
    // Hides the cursor and keeps reporting mouse motion even at the edge of the screen.
    void setRelativeMouseMode(bool enabled) const;

  private:
    struct SdlWindowDeleter
    {
        void operator()(SDL_Window* window) const
        {
            SDL_DestroyWindow(window);
        }
    };

    std::unique_ptr<SDL_Window, SdlWindowDeleter> window_;
};
