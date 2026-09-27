#pragma once

#include "vk_types.h"

class VulkanEngine
{
public:
    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debugMessenger;
    VkPhysicalDevice _gpu;
    VkDevice _device;
    VkSurfaceKHR _surface;

    bool _isInitialized = false;
    int _frameNumber = 0;
    bool _stopRendering = false;
    VkExtent2D _windowExtent{ 1700, 900 };
    struct SDL_Window* _window = nullptr;

    static VulkanEngine& get();

    void init();
    void run();
    void draw();
    void cleanup();

private:
    VulkanEngine() = default;
    VulkanEngine(const VulkanEngine&) = delete;
    VulkanEngine& operator=(const VulkanEngine&) = delete;

    void initVulkan();
    void initSwapchain();
    void initCommands();
    void initSyncStructures();
};
