#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <print>

int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::println(stderr, "SDL_Init failed: {}", SDL_GetError());
        return 1;
    }

    constexpr auto windowWidth = 1280;
    constexpr auto windowHeight = 720;
    constexpr auto windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    auto* const window = SDL_CreateWindow("vkdriven", windowWidth, windowHeight, windowFlags);
    if (!window)
    {
        std::println(stderr, "SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    auto running = true;
    while (running)
    {
        auto event = SDL_Event{};
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
        }

        SDL_Delay(16);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
