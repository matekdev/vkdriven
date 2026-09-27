#include "vk_engine.h"
#include "vk_types.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>

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

    while (!shouldQuit)
    {
        while (SDL_PollEvent(&e) != 0)
        {
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

        if (_stopRendering)
        {
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

void VulkanEngine::initVulkan()
{
    auto vkBuilder = vkb::InstanceBuilder{};
    auto instance =
        vkBuilder.set_app_name("VKDriven").request_validation_layers(true).require_api_version(1, 3, 0).build();

    _instance = instance.value().instance;
    _debugMessenger = instance.value().debug_messenger;

    SDL_Vulkan_CreateSurface(_window, _instance, nullptr, &_surface);

    // Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.dynamicRendering = true;
    features.synchronization2 = true;

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    auto selector = vkb::PhysicalDeviceSelector{instance.value()};
    auto physicalDevice = selector.set_minimum_version(1, 3)
                              .set_required_features_13(features)
                              .set_required_features_12(features12)
                              .set_surface(_surface)
                              .select()
                              .value();

    auto deviceBuilder = vkb::DeviceBuilder{physicalDevice};
    auto vkDevice = deviceBuilder.build().value();

    _device = vkDevice.device;
    _gpu = physicalDevice.physical_device;
}

void VulkanEngine::initSwapchain()
{
}

void VulkanEngine::initCommands()
{
}

void VulkanEngine::initSyncStructures()
{
}
