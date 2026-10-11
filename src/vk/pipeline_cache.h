#pragma once

#include <volk.h>

#include "vk/handle.h"

#include <filesystem>

class Device;

// A VkPipelineCache that's loaded from a file on startup and written back on destruction, so pipelines
// built in an earlier run don't have to be compiled from SPIR-V to GPU code again. A file written by a
// different GPU or driver version is ignored.
class PipelineCache
{
  public:
    PipelineCache(const Device& device, std::filesystem::path path);
    ~PipelineCache();

    PipelineCache(const PipelineCache&) = delete;
    PipelineCache& operator=(const PipelineCache&) = delete;
    PipelineCache(PipelineCache&&) = delete;
    PipelineCache& operator=(PipelineCache&&) = delete;

    [[nodiscard]] VkPipelineCache handle() const
    {
        return cache_.get();
    }

  private:
    void save() const;

    VkDevice device_{VK_NULL_HANDLE};
    std::filesystem::path path_;
    DeviceHandle<VkPipelineCache> cache_;
};
