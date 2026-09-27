#include <vk_engine.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vk_types.h>

#include <chrono>
#include <thread>

VulkanEngine& VulkanEngine::get()
{
	static VulkanEngine engine;
	return engine;
}

void VulkanEngine::init()
{
	SDL_Init(SDL_INIT_VIDEO);
	const auto windowFlags = SDL_WINDOW_VULKAN;

	_window = SDL_CreateWindow("VKDriven", _windowExtent.width, _windowExtent.height, windowFlags);
	_isInitialized = true;
}

void VulkanEngine::run()
{
    SDL_Event e;
    auto shouldQuit = false;

    while (!shouldQuit) {
        while (SDL_PollEvent(&e) != 0) {
            switch (e.type)
            {
            case SDL_EVENT_QUIT:
                shouldQuit = true;
                break;
            case SDL_EVENT_WINDOW_MINIMIZED:
                _stopRendering = true;
                break;
            case SDL_EVENT_WINDOW_RESTORED:
                _stopRendering = false;
                break;
            default:
                break;
            }
        }

        if (_stopRendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}

void VulkanEngine::draw()
{

}

void VulkanEngine::cleanup()
{
	if (_isInitialized)
		SDL_DestroyWindow(_window);
}
