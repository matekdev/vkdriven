#include "vk/shader_compiler.h"

#include "vk/check.h"
#include "vk/device.h"

#include <array>
#include <cstdint>
#include <format>

namespace
{

std::string diagnosticsText(slang::IBlob* diagnostics)
{
    if (!diagnostics)
        return {};
    return static_cast<const char*>(diagnostics->getBufferPointer());
}

} // namespace

ShaderCompiler::ShaderCompiler()
{
    chk(slang::createGlobalSession(globalSession_.writeRef()));
}

std::expected<DeviceHandle<VkShaderModule>, std::string> ShaderCompiler::compile(
    const Device& device, const std::filesystem::path& path) const
{
    const auto targets{
        std::to_array<slang::TargetDesc>({{.format{SLANG_SPIRV}, .profile{globalSession_->findProfile("spirv_1_4")}}})};
    auto options{std::to_array<slang::CompilerOptionEntry>(
        {{slang::CompilerOptionName::EmitSpirvDirectly, {slang::CompilerOptionValueKind::Int, 1}}})};
    slang::SessionDesc sessionDesc{.targets{targets.data()},
                                   .targetCount{SlangInt(targets.size())},
                                   .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                                   .compilerOptionEntries{options.data()},
                                   .compilerOptionEntryCount{uint32_t(options.size())}};

    Slang::ComPtr<slang::ISession> session;
    chk(globalSession_->createSession(sessionDesc, session.writeRef()));

    const std::string pathString = path.string();
    const std::string moduleName = path.stem().string();
    Slang::ComPtr<slang::IBlob> diagnostics;
    Slang::ComPtr<slang::IModule> module{
        session->loadModuleFromSource(moduleName.c_str(), pathString.c_str(), nullptr, diagnostics.writeRef())};
    if (!module)
        return std::unexpected{std::format("Failed to compile {}:\n{}", pathString, diagnosticsText(diagnostics))};

    Slang::ComPtr<slang::IBlob> spirv;
    if (SLANG_FAILED(module->getTargetCode(0, spirv.writeRef(), diagnostics.writeRef())))
        return std::unexpected{
            std::format("Failed to generate SPIR-V for {}:\n{}", pathString, diagnosticsText(diagnostics))};

    VkShaderModuleCreateInfo shaderModuleCI{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                            .codeSize = spirv->getBufferSize(),
                                            .pCode = static_cast<const uint32_t*>(spirv->getBufferPointer())};
    VkShaderModule shaderModule{VK_NULL_HANDLE};
    chk(vkCreateShaderModule(device.handle(), &shaderModuleCI, nullptr, &shaderModule));
    return DeviceHandle<VkShaderModule>{device.handle(), shaderModule};
}
