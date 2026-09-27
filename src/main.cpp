#include "vk_engine.h"

int main()
{
    auto& engine = VulkanEngine::get();
    engine.init();
    engine.run();
    engine.cleanup();
}
