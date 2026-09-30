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
#include "vk/swapchain.h"
#include "vk/sync.h"

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

void ImGuiLayer::record(VkCommandBuffer cb, const Swapchain& swapchain, uint32_t imageIndex) const
{
    pipelineBarrier(cb,
                    {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                     .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     .srcAccessMask = VK_ACCESS_2_NONE,
                     .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                     .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                     .newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                     .image = swapchain.image(imageIndex),
                     .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)});

    VkRenderingAttachmentInfo colorAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .imageView = swapchain.view(imageIndex),
                                                  .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                  .clearValue{.color{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderingInfo renderingInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                  .renderArea{.extent = swapchain.extent()},
                                  .layerCount = 1,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &colorAttachmentInfo};
    vkCmdBeginRendering(cb, &renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);
    vkCmdEndRendering(cb);

    pipelineBarrier(cb, {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                         .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                         .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                         .dstAccessMask = VK_ACCESS_2_NONE,
                         .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
                         .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         .image = swapchain.image(imageIndex),
                         .subresourceRange = subresourceRange(VK_IMAGE_ASPECT_COLOR_BIT)});
}
