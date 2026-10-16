#pragma once

#include "framegen/FrameGenCore.hpp"
#include <chrono>
#include <atomic>
#include <array>

namespace framegen {

// ============================================================================
// FramePacer Implementation
// ============================================================================

class FramePacer : public IFramePacer {
public:
    FramePacer();
    ~FramePacer() override = default;
    
    void paceForTargetFps(int target_fps, int64_t frame_start_ns) override;
    void recordFrame(int64_t timestamp_ns, int frame_type, bool is_interpolated) override;
    void reset() override;
    void updateConfig(const FrameGenConfig& config) override;
    RuntimeTelemetrySnapshot getMetrics() const override;

private:
    // Configuration
    std::atomic<int> target_fps_{60};
    std::atomic<int> min_fps_{30};
    std::atomic<bool> enable_pacing_{true};
    std::atomic<VSyncMode> vsync_mode_{VSyncMode::Auto};
    
    // Frame timing
    int64_t last_frame_ns_{0};
    int64_t frame_interval_ns_{16666666};  // ~60 FPS default
    int64_t min_frame_interval_ns_{41666666};  // ~24 FPS max
    
    // Metrics
    mutable std::atomic<uint64_t> frame_count_{0};
    mutable std::atomic<uint64_t> interpolated_count_{0};
    mutable std::atomic<uint64_t> dropped_count_{0};
    mutable std::atomic<float> current_fps_{0.0f};
    mutable std::atomic<float> effective_fps_{0.0f};
    mutable std::atomic<float> frame_time_ms_{0.0f};
    mutable std::atomic<float> jitter_ms_{0.0f};
    mutable std::atomic<float> p99_frame_time_ms_{0.0f};
    mutable std::atomic<float> p999_frame_time_ms_{0.0f};
    
    // Frame time history for percentile calculation
    static constexpr size_t HISTORY_SIZE = 1000;
    std::array<float, HISTORY_SIZE> frame_time_history_{};
    std::atomic<size_t> history_index_{0};
    std::atomic<size_t> history_count_{0};
    
    // Jitter calculation
    float prev_frame_time_ms_{0.0f};
    
    // VSync handling
    bool tryUsePresentationTime(int64_t target_present_ns);
    void busyWaitUntil(int64_t target_ns);
    void sleepUntil(int64_t target_ns);
    
    // Percentile calculation
    float calculatePercentile(float percentile) const;
    void updatePercentiles();
};

} // namespace framegen