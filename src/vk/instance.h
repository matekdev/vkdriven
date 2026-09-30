#pragma once

#include <volk.h>

#include <span>

class Instance
{
  public:
    Instance(const char* applicationName, std::span<const char* const> extensions);
    ~Instance();

    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&) = delete;
    Instance& operator=(Instance&&) = delete;

    [[nodiscard]] VkInstance handle() const
    {
        return instance_;
    }

  private:
    VkInstance instance_{VK_NULL_HANDLE};
};
