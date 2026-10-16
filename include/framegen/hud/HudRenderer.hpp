#pragma once

#include "framegen/FrameGenCore.hpp"
#include <jni.h>
#include <atomic>
#include <mutex>

namespace framegen {

// ============================================================================
// HudRenderer Implementation
// ============================================================================

class HudRenderer : public IHudRenderer {
public:
    HudRenderer();
    ~HudRenderer() override;
    
    bool initialize(void* java_vm, const FrameGenConfig& config) override;
    void shutdown() override;
    void publishHud(const FrameGenConfig& config, const RuntimeTelemetrySnapshot& snapshot) override;
    void updateConfig(const FrameGenConfig& config) override;
    void clearHud() override;

private:
    // Configuration
    std::atomic<HudStyle> hud_style_{HudStyle::Standard};
    std::atomic<HudOptimizerMode> hud_optimizer_{HudOptimizerMode::HideDefaultFPS};
    std::atomic<bool> hud_protection_{true};
    std::atomic<bool> show_fps_graph_{true};
    std::atomic<bool> show_frame_times_{true};
    std::atomic<bool> show_memory_usage_{false};
    
    // Java VM and JNI
    JavaVM* java_vm_{nullptr};
    jclass hud_class_{nullptr};
    jmethodID publish_method_{nullptr};
    jmethodID clear_method_{nullptr};
    jmethodID optimize_method_{nullptr};
    
    // Telemetry history for graphs
    static constexpr size_t GRAPH_HISTORY_SIZE = 120;
    std::array<float, GRAPH_HISTORY_SIZE> fps_history_{};
    std::array<float, GRAPH_HISTORY_SIZE> frame_time_history_{};
    std::array<float, GRAPH_HISTORY_SIZE> jitter_history_{};
    std::atomic<size_t> history_index_{0};
    
    // Internal methods
    void callJavaPublishHud(const RuntimeTelemetrySnapshot& snapshot);
    void callJavaClearHud();
    void callJavaOptimizeHud(bool enable);
    void updateHistory(const RuntimeTelemetrySnapshot& snapshot);
    std::string formatTelemetryJson(const RuntimeTelemetrySnapshot& snapshot) const;
};

} // namespace framegen