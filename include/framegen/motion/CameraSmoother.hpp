#pragma once

#include "framegen/FrameGenCore.hpp"
#include <chrono>
#include <atomic>
#include <array>

namespace framegen {

// ============================================================================
// CameraSmoother Implementation
// ============================================================================

class CameraSmoother : public ICameraSmoother {
public:
    CameraSmoother();
    ~CameraSmoother() override = default;
    
    bool installHooks() override;
    void uninstallHooks() override;
    void processTurnDelta(float yaw_delta, float pitch_delta, int64_t timestamp_ns) override;
    void consumeFrameMotion(float motion, int64_t timestamp_ns) override;
    void updateConfig(const FrameGenConfig& config) override;
    void reset() override;

private:
    // Configuration
    std::atomic<CameraSmoothingMode> camera_mode_{CameraSmoothingMode::AdaptiveSmart};
    std::atomic<float> smoothing_strength_{0.85f};
    std::atomic<float> responsiveness_{0.15f};
    std::atomic<bool> enabled_{true};
    
    // EMA state
    float ema_yaw_{0.0f};
    float ema_pitch_{0.0f};
    bool ema_initialized_{false};
    
    // Spring state
    float spring_yaw_pos_{0.0f};
    float spring_yaw_vel_{0.0f};
    float spring_pitch_pos_{0.0f};
    float spring_pitch_vel_{0.0f};
    bool spring_initialized_{false};
    
    // Adaptive state
    struct MotionSample {
        float yaw;
        float pitch;
        int64_t timestamp;
    };
    static constexpr size_t MOTION_HISTORY_SIZE = 120;  // ~2 seconds at 60Hz
    std::array<MotionSample, MOTION_HISTORY_SIZE> motion_history_{};
    std::atomic<size_t> motion_history_index_{0};
    std::atomic<size_t> motion_history_count_{0};
    
    // Last timestamp for delta time calculation
    int64_t last_timestamp_{0};
    
    // Hook handles (opaque pointers from pl::memory::hook)
    void* turn_delta_hook_{nullptr};
    void* fov_hook_{nullptr};
    void* perspective_hook_{nullptr};
    
    // Original function pointers (for detours)
    using TurnDeltaFn = void(*)(float, float);
    using GetFovFn = float(*)();
    using GetPerspectiveFn = void(*)();
    
    TurnDeltaFn original_turn_delta_{nullptr};
    GetFovFn original_get_fov_{nullptr};
    GetPerspectiveFn original_get_perspective_{nullptr};
    
    // Smoothing algorithms
    void applyEMA(float& yaw, float& pitch);
    void applySpring(float& yaw, float& pitch, float dt);
    void applyAdaptive(float& yaw, float& pitch, float dt);
    void updateMotionHistory(float yaw, float pitch, int64_t timestamp);
    float calculateMotionVariance() const;
    float calculateAdaptiveStrength() const;
    
    // Hook callbacks (static for C compatibility)
    static void turnDeltaDetour(float yaw_delta, float pitch_delta);
    static float tryGetFovDetour();
    static void getCameraPerspectiveDetour();
    static void applyTurnDeltaDetour(void* vec2_ptr);
};

} // namespace framegen