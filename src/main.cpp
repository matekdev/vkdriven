// P0 placeholder: opens a window and checks that the Vulkan loader is reachable.
// The Vulkan setup itself (instance, device, swapchain) is P1 work.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <fmt/format.h>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>

int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fmt::println(stderr, "SDL_Init failed: {}", SDL_GetError());
        return 1;
    }

    uint32_t apiVersion = 0;
    vkEnumerateInstanceVersion(&apiVersion);
    fmt::println("Vulkan loader version {}.{}.{}",
        VK_API_VERSION_MAJOR(apiVersion),
        VK_API_VERSION_MINOR(apiVersion),
        VK_API_VERSION_PATCH(apiVersion));

    SDL_Window* window = SDL_CreateWindow("Vast Engine", 1600, 900, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window)
    {
        fmt::println(stderr, "SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
        }

        // Nothing is rendered yet, so don't spin a CPU core at 100%.
        SDL_Delay(16);
        FrameMark;
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
