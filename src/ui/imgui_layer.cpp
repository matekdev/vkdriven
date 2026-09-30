#include "ui/imgui_layer.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <IconsFontAwesome6.h>

#include "platform/window.h"
#include "ui/imgui_style.h"
#include "vk/check.h"
#include "vk/device.h"
#include "vk/instance.h"

#include <cstdint>
#include <expected>
#include <format>
#include <stdexcept>
#include <string>

namespace
{

constexpr uint32_t userTextureCount = 1;
constexpr uint32_t descriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE + userTextureCount;

constexpr const char* textFontPath = "assets/fonts/JetBrainsMono-Regular.ttf";
constexpr const char* iconFontPath = "assets/fonts/fa-solid-900.ttf";
constexpr float textFontSize = 16.0f;
// Font Awesome glyphs are drawn larger than text at the same size, so scale them down to line up.
constexpr float iconFontSize = textFontSize * 2.0f / 3.0f;
constexpr ImWchar iconRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};

std::expected<void, std::string> loadFonts(ImFontAtlas& fonts)
{
    if (!fonts.AddFontFromFileTTF(textFontPath, textFontSize))
        return std::unexpected{std::format("Failed to load font {}", textFontPath)};

    ImFontConfig iconConfig;
    iconConfig.MergeMode = true;
    iconConfig.PixelSnapH = true;
    iconConfig.GlyphMinAdvanceX = iconFontSize;
    if (!fonts.AddFontFromFileTTF(iconFontPath, iconFontSize, &iconConfig, iconRanges))
        return std::unexpected{std::format("Failed to load font {}", iconFontPath)};
    return {};
}

} // namespace

ImGuiLayer::ImGuiLayer(const Window& window, const Instance& instance, const Device& device,
                       uint32_t framesInFlight, VkFormat colorFormat)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    applyClassicSteamStyle(ImGui::GetStyle());

    if (const auto fontsLoaded = loadFonts(*io.Fonts); !fontsLoaded)
    {
        ImGui::DestroyContext();
        throw std::runtime_error{fontsLoaded.error()};
    }

    chk(ImGui_ImplSDL3_InitForVulkan(window.handle()));

    ImGui_ImplVulkan_InitInfo initInfo{
        .ApiVersion = VK_API_VERSION_1_3,
        .Instance = instance.handle(),
        .PhysicalDevice = device.physical(),
        .Device = device.handle(),
        .QueueFamily = device.queueFamily(),
        .Queue = device.queue(),
        .DescriptorPoolSize = descriptorPoolSize,
        .MinImageCount = framesInFlight,
        .ImageCount = framesInFlight,
        .PipelineInfoMain{.PipelineRenderingCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                        .colorAttachmentCount = 1,
                                                        .pColorAttachmentFormats = &colorFormat}},
        .UseDynamicRendering = true,
        .CheckVkResultFn = [](VkResult result) { chk(result); },
    };
    chk(ImGui_ImplVulkan_Init(&initInfo));
}

ImGuiLayer::~ImGuiLayer()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::beginFrame() const
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame() const
{
    ImGui::Render();
}

void ImGuiLayer::record(VkCommandBuffer cb) const
{
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);
}
