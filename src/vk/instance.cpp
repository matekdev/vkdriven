#include "vk/instance.h"

#include "vk/check.h"

#include <cstdint>

Instance::Instance(const char* applicationName, std::span<const char* const> extensions)
{
    chk(volkInitialize());

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = applicationName,
        .apiVersion = VK_API_VERSION_1_3,
    };
    VkInstanceCreateInfo instanceCI{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                    .pApplicationInfo = &appInfo,
                                    .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
                                    .ppEnabledExtensionNames = extensions.data()};
    chk(vkCreateInstance(&instanceCI, nullptr, &instance_));
    volkLoadInstance(instance_);
}

Instance::~Instance()
{
    vkDestroyInstance(instance_, nullptr);
}
