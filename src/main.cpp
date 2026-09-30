#include <SDL3/SDL_main.h>

#include "app.h"

#include <exception>
#include <print>

int main(int, char**)
{
    try
    {
        App app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::println(stderr, "{}", e.what());
        return 1;
    }
    return 0;
}
