#include "profiling/benchmark.h"

#include "vk/gpu_timer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <numeric>
#include <ostream>
#include <print>

namespace
{

// Sponza views: down the atrium, up close to a wall (most of the scene is out of view), and down the atrium from
// the upper gallery at the far end.
const auto benchmarkViews = std::to_array<BenchmarkView>({
    {.name = "Atrium", .position = {-8.0f, 2.0f, 0.0f}, .yawRadians = glm::radians(90.0f), .pitchRadians = 0.0f},
    {.name = "Wall",
     .position = {-0.5f, 2.0f, 1.5f},
     .yawRadians = 0.0f,
     .pitchRadians = glm::radians(-8.0f)},
    {.name = "Gallery",
     .position = {7.0f, 6.0f, 0.0f},
     .yawRadians = glm::radians(-90.0f),
     .pitchRadians = glm::radians(-25.0f)},
});

// The first line a shell command prints, or an empty string if it couldn't run.
std::string firstOutputLine(const char* command)
{
    const std::unique_ptr<FILE, decltype(&_pclose)> pipe{_popen(command, "r"), &_pclose};
    if (!pipe)
        return {};

    std::array<char, 256> buffer{};
    if (!std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()))
        return {};

    std::string line{buffer.data()};
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();
    return line;
}

// The short commit hash, marked dirty when there are uncommitted changes, since those runs aren't of the commit.
std::string gitCommit()
{
    std::string commit = firstOutputLine("git rev-parse --short HEAD 2>nul");
    if (commit.empty())
        return "unknown";
    if (!firstOutputLine("git status --porcelain 2>nul").empty())
        commit += "-dirty";
    return commit;
}

#ifdef NDEBUG
constexpr const char* buildType = "release";
#else
constexpr const char* buildType = "debug";
#endif

} // namespace

void Benchmark::start()
{
    viewIndex_ = 0;
    frameInView_ = 0;
    series_.clear();
    rows_.clear();
}

const BenchmarkView& Benchmark::currentView() const
{
    return benchmarkViews[*viewIndex_];
}

float Benchmark::progress() const
{
    if (!running())
        return 0.0f;

    const float framesPerView = static_cast<float>(warmupFrames + sampleFrames);
    const float framesDone = static_cast<float>(*viewIndex_) * framesPerView + static_cast<float>(frameInView_);
    return framesDone / (framesPerView * static_cast<float>(benchmarkViews.size()));
}

void Benchmark::addFrame(std::span<const GpuTiming> gpuTimings, float cpuRecordMilliseconds)
{
    if (!running())
        return;

    frameInView_++;
    if (frameInView_ > warmupFrames)
    {
        record("CPU record", cpuRecordMilliseconds);
        for (const GpuTiming& timing : gpuTimings)
            record(std::format("GPU {}", timing.name), timing.milliseconds);
    }
    if (frameInView_ < warmupFrames + sampleFrames)
        return;

    summarizeView();
    series_.clear();
    frameInView_ = 0;
    ++*viewIndex_;
    if (*viewIndex_ < benchmarkViews.size())
        return;

    viewIndex_.reset();
    writeResults();
}

void Benchmark::record(const std::string& metric, float milliseconds)
{
    const auto existing = std::ranges::find(series_, metric, &Series::metric);
    if (existing != series_.end())
    {
        existing->milliseconds.push_back(milliseconds);
        return;
    }
    series_.push_back({.metric = metric, .milliseconds = {milliseconds}});
}

void Benchmark::summarizeView()
{
    for (Series& series : series_)
    {
        std::vector<float>& values = series.milliseconds;
        std::ranges::sort(values);
        const size_t count = values.size();
        rows_.push_back({.view = currentView().name,
                         .metric = series.metric,
                         .samples = count,
                         .min = values.front(),
                         .median = values[count / 2],
                         .mean = std::accumulate(values.begin(), values.end(), 0.0f) / static_cast<float>(count),
                         .p95 = values[std::min(count - 1, count * 95 / 100)]});
    }
}

void Benchmark::writeResults()
{
    const std::string commit = gitCommit();
    const auto now = std::chrono::zoned_time{std::chrono::current_zone(),
                                             std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

    std::filesystem::create_directories("benchmarks");
    const std::filesystem::path resultPath =
        std::filesystem::path{"benchmarks"} / std::format("{:%Y-%m-%d_%H%M%S}_{}.csv", now, commit);

    std::ofstream file{resultPath};
    std::println(file, "commit,build,viewport,view,metric,samples,min_ms,median_ms,mean_ms,p95_ms");
    for (const Row& row : rows_)
        std::println(file, "{},{},{}x{},{},{},{},{:.4f},{:.4f},{:.4f},{:.4f}", commit, buildType,
                     renderExtent.width, renderExtent.height, row.view, row.metric, row.samples, row.min,
                     row.median, row.mean, row.p95);

    std::println("Benchmark ({}, {}, {}x{}), median / p95 in ms:", commit, buildType, renderExtent.width,
                 renderExtent.height);
    for (const Row& row : rows_)
        std::println("  {:<10} {:<14} {:7.3f} / {:7.3f}", row.view, row.metric, row.median, row.p95);
    std::println("Saved {}", resultPath.string());
}
