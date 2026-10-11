#include "vk/pipeline_cache.h"

#include "vk/check.h"
#include "vk/device.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <print>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

std::vector<std::byte> readFile(const std::filesystem::path& path)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
        return {};

    std::vector<std::byte> bytes(size);
    std::ifstream file{path, std::ios::binary};
    if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
        return {};
    return bytes;
}

bool matchesDevice(std::span<const std::byte> data, VkPhysicalDevice physical)
{
    VkPipelineCacheHeaderVersionOne header{};
    if (data.size() < sizeof(header))
        return false;
    std::memcpy(&header, data.data(), sizeof(header));

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical, &properties);
    return header.headerSize >= sizeof(header) && header.headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE &&
           header.vendorID == properties.vendorID && header.deviceID == properties.deviceID &&
           std::ranges::equal(header.pipelineCacheUUID, properties.pipelineCacheUUID);
}

} // namespace

PipelineCache::PipelineCache(const Device& device, std::filesystem::path path)
    : device_{device.handle()}, path_{std::move(path)}
{
    std::vector<std::byte> initialData = readFile(path_);
    if (!initialData.empty() && !matchesDevice(initialData, device.physical()))
    {
        std::println("Ignoring {}: written by a different GPU or driver", path_.string());
        initialData.clear();
    }

    VkPipelineCacheCreateInfo cacheCI{.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
                                      .initialDataSize = initialData.size(),
                                      .pInitialData = initialData.data()};
    VkPipelineCache cache{VK_NULL_HANDLE};
    chk(vkCreatePipelineCache(device_, &cacheCI, nullptr, &cache));
    cache_ = DeviceHandle<VkPipelineCache>{device_, cache};
    std::println("Pipeline cache: loaded {} bytes from {}", initialData.size(), path_.string());
}

PipelineCache::~PipelineCache()
{
    save();
}

void PipelineCache::save() const
{
    size_t size = 0;
    chk(vkGetPipelineCacheData(device_, cache_.get(), &size, nullptr));
    std::vector<std::byte> data(size);
    chk(vkGetPipelineCacheData(device_, cache_.get(), &size, data.data()));

    std::ofstream file{path_, std::ios::binary | std::ios::trunc};
    if (!file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(size)))
        std::println(stderr, "Failed to write pipeline cache {}", path_.string());
}
