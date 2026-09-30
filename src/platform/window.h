#pragma once

#include <volk.h>

#include <SDL3/SDL_video.h>

#include <memory>
#include <span>

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
