#pragma once

#include <volk.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

struct GpuTiming;

struct BenchmarkView
{
    const char* name{nullptr};
    glm::vec3 position{};
    float yawRadians{0.0f};
    float pitchRadians{0.0f};
};

// Puts the camera at a few fixed views and records raw per-frame timings at each, then writes min / median /
// mean / p95 per pass to a CSV in benchmarks/, so runs from different commits can be compared.
class Benchmark
{
  public:
    // Long enough for the GPU timer's two-frame delay and clock ramp-up after each camera jump.
    static constexpr uint32_t warmupFrames = 120;
    static constexpr uint32_t sampleFrames = 500;
    static constexpr VkExtent2D renderExtent{.width = 1920, .height = 1080};

    void start();
    void addFrame(std::span<const GpuTiming> gpuTimings, float cpuRecordMilliseconds);

    [[nodiscard]] bool running() const
    {
        return viewIndex_.has_value();
    }

    // The view the camera should be at this frame. Only valid while running.
    [[nodiscard]] const BenchmarkView& currentView() const;
    [[nodiscard]] float progress() const;

  private:
    struct Series
    {
        std::string metric;
        std::vector<float> milliseconds;
    };

    struct Row
    {
        std::string view;
        std::string metric;
        size_t samples{0};
        float min{0.0f};
        float median{0.0f};
        float mean{0.0f};
        float p95{0.0f};
    };

    void record(const std::string& metric, float milliseconds);
    void summarizeView();
    void writeResults();

    std::optional<size_t> viewIndex_;
    uint32_t frameInView_{0};
    std::vector<Series> series_;
    std::vector<Row> rows_;
};
