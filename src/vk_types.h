#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include <fmt/format.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <cstdlib>
#include <source_location>

inline void vkCheck(const VkResult result, const std::source_location location = std::source_location::current())
{
    if (result >= VK_SUCCESS)
    {
        return;
    }

    fmt::println(stderr, "Vulkan error {} at {}:{}", vk::to_string(static_cast<vk::Result>(result)), location.file_name(), location.line());
    std::abort();
}
