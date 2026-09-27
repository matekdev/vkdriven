// P0 placeholder: opens a window and checks that the Vulkan loader is reachable.
// The Vulkan setup itself (instance, device, swapchain) is P1 work.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <fmt/format.h>
#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace
{

struct SdlSession
{
    SdlSession() = default;
    SdlSession(const SdlSession&) = delete;
    SdlSession& operator=(const SdlSession&) = delete;
    ~SdlSession() { SDL_Quit(); }
};

struct SdlWindowDeleter
{
    void operator()(SDL_Window* window) const { SDL_DestroyWindow(window); }
};

using SdlWindowPtr = std::unique_ptr<SDL_Window, SdlWindowDeleter>;

struct WindowSettings
{
    const char* title;
    int width;
    int height;
    SDL_WindowFlags flags;
};

constexpr WindowSettings mainWindowSettings{
    .title = "Vast Engine",
    .width = 1600,
    .height = 900,
    .flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE,
};

std::expected<void, std::string> initializeSdl()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return std::unexpected(std::string{SDL_GetError()});
    }
    return {};
}

std::expected<SdlWindowPtr, std::string> createWindow(const WindowSettings& settings)
{
    SdlWindowPtr window{SDL_CreateWindow(settings.title, settings.width, settings.height, settings.flags)};
    if (!window)
    {
        return std::unexpected(std::string{SDL_GetError()});
    }
    return window;
}

std::optional<std::uint32_t> queryVulkanLoaderVersion()
{
    std::uint32_t apiVersion = 0;
    if (vkEnumerateInstanceVersion(&apiVersion) != VK_SUCCESS)
    {
        return std::nullopt;
    }
    return apiVersion;
}

void printVulkanLoaderVersion()
{
    const auto loaderVersion = queryVulkanLoaderVersion();
    if (!loaderVersion)
    {
        fmt::println(stderr, "Vulkan loader version could not be queried");
        return;
    }

    fmt::println("Vulkan loader version {}.{}.{}",
        VK_API_VERSION_MAJOR(*loaderVersion),
        VK_API_VERSION_MINOR(*loaderVersion),
        VK_API_VERSION_PATCH(*loaderVersion));
}

bool processEvents()
{
    SDL_Event event{};
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            return false;
        }
    }
    return true;
}

void runMainLoop()
{
    while (processEvents())
    {
        // Nothing is rendered yet, so don't spin a CPU core at 100%.
        SDL_Delay(16);
        FrameMark;
    }
}

} // namespace

int main(int, char**)
{
    if (const auto sdlInitialized = initializeSdl(); !sdlInitialized)
    {
        fmt::println(stderr, "SDL_Init failed: {}", sdlInitialized.error());
        return 1;
    }
    const SdlSession sdlSession;

    printVulkanLoaderVersion();

    const auto window = createWindow(mainWindowSettings);
    if (!window)
    {
        fmt::println(stderr, "SDL_CreateWindow failed: {}", window.error());
        return 1;
    }

    runMainLoop();
    return 0;
}
