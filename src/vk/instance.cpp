#include "vk/instance.h"

#include "vk/check.h"

#include <algorithm>
#include <cstdint>
#include <print>
#include <string_view>
#include <vector>

namespace
{

#ifdef NDEBUG
constexpr bool validationEnabled = false;
#else
constexpr bool validationEnabled = true;
#endif

constexpr const char* validationLayerName = "VK_LAYER_KHRONOS_validation";

bool isLayerAvailable(std::string_view layerName)
{
    uint32_t layerCount = 0;
    chk(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
    std::vector<VkLayerProperties> layers(layerCount);
    chk(vkEnumerateInstanceLayerProperties(&layerCount, layers.data()));
    return std::ranges::any_of(layers, [&](const VkLayerProperties& layer) { return layerName == layer.layerName; });
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void*)
{
    const char* label = severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT     ? "error"
                        : severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ? "warning"
                                                                                      : "info";
    std::println(stderr, "[vulkan {}] {}", label, callbackData->pMessage);
    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT debugMessengerCI()
{
    return {.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debugCallback};
}

} // namespace

Instance::Instance(const char* applicationName, std::span<const char* const> extensions)
{
    chk(volkInitialize());

    const bool useValidation = validationEnabled && isLayerAvailable(validationLayerName);
    if (validationEnabled && !useValidation)
        std::println(stderr, "{} not found, running without validation (is the Vulkan SDK installed?)",
                     validationLayerName);

    std::vector<const char*> enabledExtensions{extensions.begin(), extensions.end()};
    std::vector<const char*> enabledLayers;
    if (useValidation)
    {
        enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        enabledLayers.push_back(validationLayerName);
    }

    // Chained into the instance create info so vkCreateInstance/vkDestroyInstance are validated too.
    const VkDebugUtilsMessengerCreateInfoEXT messengerCI = debugMessengerCI();

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = applicationName,
        .apiVersion = VK_API_VERSION_1_3,
    };
    VkInstanceCreateInfo instanceCI{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                    .pNext = useValidation ? &messengerCI : nullptr,
                                    .pApplicationInfo = &appInfo,
                                    .enabledLayerCount = static_cast<uint32_t>(enabledLayers.size()),
                                    .ppEnabledLayerNames = enabledLayers.data(),
                                    .enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size()),
                                    .ppEnabledExtensionNames = enabledExtensions.data()};
    chk(vkCreateInstance(&instanceCI, nullptr, &instance_));
    volkLoadInstance(instance_);

    if (useValidation)
        chk(vkCreateDebugUtilsMessengerEXT(instance_, &messengerCI, nullptr, &debugMessenger_));
}

Instance::~Instance()
{
    if (debugMessenger_ != VK_NULL_HANDLE)
        vkDestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
    vkDestroyInstance(instance_, nullptr);
}
