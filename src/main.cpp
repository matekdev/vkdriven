#include <vk_engine.h>

int main()
{
	auto engine = VulkanEngine{};
	engine.init();
	engine.run();
	engine.cleanup();
}
