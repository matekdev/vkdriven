#pragma once

#include "vk_types.h"

struct FrameData
{
    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine
{
  public:
    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debugMessenger;
    VkPhysicalDevice _gpu;
    VkDevice _device;
    VkSurfaceKHR _surface;

    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;

    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    VkExtent2D _swapchainExtent;

    bool _isInitialized = false;
    int _frameNumber = 0;
    bool _stopRendering = false;
    VkExtent2D _windowExtent{1700, 900};
    struct SDL_Window* _window = nullptr;

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    FrameData _frames[FRAME_OVERLAP];
    FrameData& getCurrentFrame()
    {
        return _frames[_frameNumber % FRAME_OVERLAP];
    }

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

    void createSwapchain(uint32_t width, uint32_t height);
    void destroySwapchain();
};
