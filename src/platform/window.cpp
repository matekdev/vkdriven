#include "platform/window.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "vk/check.h"

Window::Window(const char* title, int width, int height)
{
    chk(SDL_Init(SDL_INIT_VIDEO));
    chk(SDL_Vulkan_LoadLibrary(nullptr));

    window_.reset(SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE));
    chk(window_ != nullptr);
}

Window::~Window()
{
    window_.reset();
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
}

VkExtent2D Window::sizeInPixels() const
{
    int width = 0;
    int height = 0;
    chk(SDL_GetWindowSizeInPixels(window_.get(), &width, &height));
    return {.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)};
}

std::span<const char* const> Window::requiredInstanceExtensions() const
{
    uint32_t count = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
    chk(extensions != nullptr);
    return {extensions, count};
}
