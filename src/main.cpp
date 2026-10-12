#include <SDL3/SDL_main.h>

#include "app.h"

#include <algorithm>
#include <exception>
#include <print>
#include <span>
#include <string_view>

int main(int argc, char** argv)
{
    const std::span arguments{argv, static_cast<size_t>(argc)};
    const bool benchmark = std::ranges::any_of(
        arguments.subspan(1), [](const char* argument) { return std::string_view{argument} == "--benchmark"; });

    try
    {
        App app;
        app.run(benchmark ? RunMode::Benchmark : RunMode::Interactive);
    }
    catch (const std::exception& e)
    {
        std::println(stderr, "{}", e.what());
        return 1;
    }
    return 0;
}
