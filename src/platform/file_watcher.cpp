#include "platform/file_watcher.h"

#include <system_error>
#include <utility>

FileWatcher::FileWatcher(std::filesystem::path directory, std::chrono::milliseconds interval)
    : directory_{std::move(directory)}, interval_{interval}, lastCheck_{std::chrono::steady_clock::now()},
      snapshot_{takeSnapshot()}
{
}

bool FileWatcher::poll()
{
    const auto now = std::chrono::steady_clock::now();
    if (now - lastCheck_ < interval_)
        return false;
    lastCheck_ = now;

    std::map<std::filesystem::path, std::filesystem::file_time_type> current = takeSnapshot();
    if (current == snapshot_)
        return false;

    snapshot_ = std::move(current);
    return true;
}

std::map<std::filesystem::path, std::filesystem::file_time_type> FileWatcher::takeSnapshot() const
{
    std::map<std::filesystem::path, std::filesystem::file_time_type> snapshot;
    std::error_code error;
    for (auto it = std::filesystem::recursive_directory_iterator{directory_, error};
         !error && it != std::filesystem::recursive_directory_iterator{}; it.increment(error))
    {
        std::error_code entryError;
        if (!it->is_regular_file(entryError))
            continue;

        const auto writeTime = it->last_write_time(entryError);
        if (!entryError)
            snapshot.emplace(it->path(), writeTime);
    }
    return snapshot;
}
