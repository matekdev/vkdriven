#pragma once

#include <volk.h>

#include <SDL3/SDL_error.h>
#include <slang.h>
#include <vulkan/vk_enum_string_helper.h>

#include <cstdint>
#include <cstdlib>
#include <print>
#include <source_location>

inline void chk(VkResult result, std::source_location location = std::source_location::current())
{
    if (result >= VK_SUCCESS)
        return;

    std::println(stderr, "Vulkan error {} at {}:{}", string_VkResult(result), location.file_name(), location.line());
    std::abort();
}

inline void chk(bool result, std::source_location location = std::source_location::current())
{
    if (result)
        return;

    std::println(stderr, "SDL error \"{}\" at {}:{}", SDL_GetError(), location.file_name(), location.line());
    std::abort();
}

inline void chk(SlangResult result, std::source_location location = std::source_location::current())
{
    if (SLANG_SUCCEEDED(result))
        return;

    std::println(stderr, "Slang error {:#x} at {}:{}", static_cast<uint32_t>(result), location.file_name(),
                 location.line());
    std::abort();
}
