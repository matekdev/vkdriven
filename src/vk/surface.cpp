#include "vk/surface.h"

#include <SDL3/SDL_vulkan.h>

#include "platform/window.h"
#include "vk/check.h"
#include "vk/instance.h"

Surface::Surface(const Instance& instance, const Window& window) : instance_{instance.handle()}
{
    chk(SDL_Vulkan_CreateSurface(window.handle(), instance_, nullptr, &surface_));
}

Surface::~Surface()
{
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
}
