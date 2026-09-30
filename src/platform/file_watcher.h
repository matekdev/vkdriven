#pragma once

#include <chrono>
#include <filesystem>
#include <map>

// Polls a directory (recursively) for added, removed or modified files. Cheap enough to call
// every frame: it only touches the filesystem once per interval.
class FileWatcher
{
  public:
    explicit FileWatcher(std::filesystem::path directory,
                         std::chrono::milliseconds interval = std::chrono::milliseconds{500});

    // True if anything changed since the last time this returned true.
    [[nodiscard]] bool poll();

  private:
    [[nodiscard]] std::map<std::filesystem::path, std::filesystem::file_time_type> takeSnapshot() const;

    std::filesystem::path directory_;
    std::chrono::milliseconds interval_;
    std::chrono::steady_clock::time_point lastCheck_;
    std::map<std::filesystem::path, std::filesystem::file_time_type> snapshot_;
};
