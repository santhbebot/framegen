#include "framegen/motion/CameraSmoother.hpp"
#include "framegen/FrameGenCore.hpp"
#include <android/log.h>
#include <cmath>
#include <algorithm>
#include <pl/memory.h>

#define LOG_TAG "FrameGenCamera"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace framegen {

// ============================================================================
// CameraSmoother Implementation
// ============================================================================

CameraSmoother::CameraSmoother() {
    LOGI("CameraSmoother constructed");
}

bool CameraSmoother::installHooks() {
    // In a real implementation, these would hook into Minecraft's camera functions
    // using pl::memory::hook with signatures discovered via reverse engineering
    // For now, we simulate the hooks being installed
    
    LOGI("CameraSmoother: Installing hooks (simulated)");
    
    // Example of what the real implementation would do:
    /*
    // Hook camera turn delta processing
    void* turn_delta_target = pl::memory::resolveSignature("Camera", "processTurnDelta");
    if (turn_delta_target) {
        turn_delta_hook_ = pl::memory::hook(turn_delta_target, 
            reinterpret_cast<void*>(&turnDeltaDetour),
            reinterpret_cast<void**>(&original_turn_delta_),
            pl::memory::HookPriority::High);
    }
    
    // Hook FOV getter
    void* fov_target = pl::memory::resolveSignature("Camera", "getFov");
    if (fov_target) {
        fov_hook_ = pl::memory::hook(fov_target,
            reinterpret_cast<void*>(&tryGetFovDetour),
            reinterpret_cast<void**>(&original_get_fov_),
            pl::memory::HookPriority::Normal);
    }
    
    // Hook perspective getter
    void* perspective_target = pl::memory::resolveSignature("Camera", "getPerspective");
    if (perspective_target) {
        perspective_hook_ = pl::memory::hook(perspective_target,
            reinterpret_cast<void*>(&getCameraPerspectiveDetour),
            reinterpret_cast<void**>(&original_get_perspective_),
            pl::memory::HookPriority::Normal);
    }
    */
    
    // Simulate success for now
    turn_delta_hook_ = reinterpret_cast<void*>(0x1);
    fov_hook_ = reinterpret_cast<void*>(0x2);
    perspective_hook_ = reinterpret_cast<void*>(0x3);
    
    enabled_.store(true, std::memory_order_relaxed);
    LOGI("CameraSmoother: Hooks installed successfully");
    return true;
}

void CameraSmoother::uninstallHooks() {
    LOGI("CameraSmoother: Uninstalling hooks");
    
    if (turn_delta_hook_) {
        // pl::memory::unhook(turn_delta_target, turn_delta_hook_);
        turn_delta_hook_ = nullptr;
    }
    if (fov_hook_) {
        // pl::memory::unhook(fov_target, fov_hook_);
        fov_hook_ = nullptr;
    }
    if (perspective_hook_) {
        // pl::memory::unhook(perspective_target, perspective_hook_);
        perspective_hook_ = nullptr;
    }
    
    original_turn_delta_ = nullptr;
    original_get_fov_ = nullptr;
    original_get_perspective_ = nullptr;
    enabled_.store(false, std::memory_order_relaxed);
    
    LOGI("CameraSmoother: Hooks uninstalled");
}

void CameraSmoother::processTurnDelta(float yaw_delta, float pitch_delta, int64_t timestamp_ns) {
    if (!enabled_.load(std::memory_order_relaxed)) {
        // Pass through unchanged
        if (original_turn_delta_) {
            original_turn_delta_(yaw_delta, pitch_delta);
        }
        return;
    }
    
    // Calculate delta time
    float dt = 0.016f;  // Default 60 FPS
    if (last_timestamp_ > 0) {
        dt = static_cast<float>(timestamp_ns - last_timestamp_) / 1e9f;
        dt = std::clamp(dt, 0.001f, 0.1f);  // Clamp to reasonable range
    }
    last_timestamp_ = timestamp_ns;
    
    // Apply smoothing based on mode
    float smoothed_yaw = yaw_delta;
    float smoothed_pitch = pitch_delta;
    
    switch (camera_mode_.load(std::memory_order_relaxed)) {
        case CameraSmoothingMode::EMA:
            applyEMA(smoothed_yaw, smoothed_pitch);
            break;
        case CameraSmoothingMode::Spring:
            applySpring(smoothed_yaw, smoothed_pitch, dt);
            break;
        case CameraSmoothingMode::AdaptiveSmart:
            applyAdaptive(smoothed_yaw, smoothed_pitch, dt);
            break;
        case CameraSmoothingMode::None:
        default:
            break;
    }
    
    // Update motion history for adaptive mode
    updateMotionHistory(yaw_delta, pitch_delta, timestamp_ns);
    
    // Call original with smoothed values
    if (original_turn_delta_) {
        original_turn_delta_(smoothed_yaw, smoothed_pitch);
    }
}

void CameraSmoother::consumeFrameMotion(float motion, int64_t timestamp_ns) {
    // This would be called per frame to consume accumulated motion
    // Used for frame interpolation motion vectors
    (void)motion;
    (void)timestamp_ns;
}

void CameraSmoother::updateConfig(const FrameGenConfig& config) {
    camera_mode_.store(config.camera_mode, std::memory_order_relaxed);
    smoothing_strength_.store(config.camera_smoothing_strength, std::memory_order_relaxed);
    responsiveness_.store(config.camera_responsiveness, std::memory_order_relaxed);
    
    LOGI("CameraSmoother config updated: mode=%d, strength=%.2f, resp=%.2f",
         static_cast<int>(config.camera_mode),
         config.camera_smoothing_strength,
         config.camera_responsiveness);
}

void CameraSmoother::reset() {
    ema_yaw_ = 0.0f;
    ema_pitch_ = 0.0f;
    ema_initialized_ = false;
    
    spring_yaw_pos_ = 0.0f;
    spring_yaw_vel_ = 0.0f;
    spring_pitch_pos_ = 0.0f;
    spring_pitch_vel_ = 0.0f;
    spring_initialized_ = false;
    
    motion_history_.fill({0.0f, 0.0f, 0});
    motion_history_index_.store(0, std::memory_order_relaxed);
    motion_history_count_.store(0, std::memory_order_relaxed);
    last_timestamp_ = 0;
    
    LOGI("CameraSmoother reset");
}

// ============================================================================
// Smoothing Algorithms
// ============================================================================

void CameraSmoother::applyEMA(float& yaw, float& pitch) {
    float alpha = smoothing_strength_.load(std::memory_order_relaxed);
    
    if (!ema_initialized_) {
        ema_yaw_ = yaw;
        ema_pitch_ = pitch;
        ema_initialized_ = true;
    } else {
        // Exponential Moving Average: new = alpha * current + (1 - alpha) * previous
        ema_yaw_ = alpha * yaw + (1.0f - alpha) * ema_yaw_;
        ema_pitch_ = alpha * pitch + (1.0f - alpha) * ema_pitch_;
    }
    
    // Output the smoothed delta (difference from previous)
    yaw = ema_yaw_;
    pitch = ema_pitch_;
}

void CameraSmoother::applySpring(float& yaw, float& pitch, float dt) {
    float stiffness = smoothing_strength_.load(std::memory_order_relaxed) * 100.0f;  // 0-100
    float damping = responsiveness_.load(std::memory_order_relaxed) * 20.0f;  // 0-20
    
    if (!spring_initialized_) {
        spring_yaw_pos_ = yaw;
        spring_pitch_pos_ = pitch;
        spring_yaw_vel_ = 0.0f;
        spring_pitch_vel_ = 0.0f;
        spring_initialized_ = true;
    }
    
    // Spring-damper system: F = -k * x - d * v
    // Target is the input delta, current position is smoothed output
    float yaw_force = -stiffness * spring_yaw_pos_ - damping * spring_yaw_vel_ + stiffness * yaw;
    float pitch_force = -stiffness * spring_pitch_pos_ - damping * spring_pitch_vel_ + stiffness * pitch;
    
    // Integrate (semi-implicit Euler)
    spring_yaw_vel_ += yaw_force * dt;
    spring_pitch_vel_ += pitch_force * dt;
    spring_yaw_pos_ += spring_yaw_vel_ * dt;
    spring_pitch_pos_ += spring_pitch_vel_ * dt;
    
    yaw = spring_yaw_pos_;
    pitch = spring_pitch_pos_;
}

void CameraSmoother::applyAdaptive(float& yaw, float& pitch, float dt) {
    // Adaptive: adjust smoothing based on motion variance
    float variance = calculateMotionVariance();
    float adaptive_strength = calculateAdaptiveStrength();
    
    // Blend between EMA and Spring based on motion characteristics
    float alpha = adaptive_strength;
    
    if (!ema_initialized_) {
        ema_yaw_ = yaw;
        ema_pitch_ = pitch;
        ema_initialized_ = true;
    }
    
    // Apply EMA with adaptive alpha
    ema_yaw_ = alpha * yaw + (1.0f - alpha) * ema_yaw_;
    ema_pitch_ = alpha * pitch + (1.0f - alpha) * ema_pitch_;
    
    // Add spring correction for high-frequency motion
    if (variance > 0.1f) {
        float correction = responsiveness_.load(std::memory_order_relaxed) * 0.5f;
        ema_yaw_ += (yaw - ema_yaw_) * correction;
        ema_pitch_ += (pitch - ema_pitch_) * correction;
    }
    
    yaw = ema_yaw_;
    pitch = ema_pitch_;
}

void CameraSmoother::updateMotionHistory(float yaw, float pitch, int64_t timestamp) {
    size_t idx = motion_history_index_.fetch_add(1, std::memory_order_relaxed) % MOTION_HISTORY_SIZE;
    motion_history_[idx] = {yaw, pitch, timestamp};
    size_t count = motion_history_count_.load(std::memory_order_relaxed);
    if (count < MOTION_HISTORY_SIZE) {
        motion_history_count_.store(count + 1, std::memory_order_relaxed);
    }
}

float CameraSmoother::calculateMotionVariance() const {
    size_t count = motion_history_count_.load(std::memory_order_relaxed);
    if (count < 10) return 0.0f;  // Not enough samples
    
    float sum_yaw = 0.0f, sum_pitch = 0.0f;
    size_t valid = 0;
    
    for (size_t i = 0; i < count; ++i) {
        const auto& sample = motion_history_[i];
        if (sample.timestamp > 0) {
            sum_yaw += sample.yaw;
            sum_pitch += sample.pitch;
            valid++;
        }
    }
    
    if (valid < 2) return 0.0f;
    
    float mean_yaw = sum_yaw / valid;
    float mean_pitch = sum_pitch / valid;
    
    float var_yaw = 0.0f, var_pitch = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        const auto& sample = motion_history_[i];
        if (sample.timestamp > 0) {
            float dy = sample.yaw - mean_yaw;
            float dp = sample.pitch - mean_pitch;
            var_yaw += dy * dy;
            var_pitch += dp * dp;
        }
    }
    
    return (var_yaw + var_pitch) / valid;
}

float CameraSmoother::calculateAdaptiveStrength() const {
    float variance = calculateMotionVariance();
    float base_strength = smoothing_strength_.load(std::memory_order_relaxed);
    float resp = responsiveness_.load(std::memory_order_relaxed);
    
    // High variance (erratic motion) -> more smoothing
    // Low variance (smooth motion) -> less smoothing, more responsive
    if (variance > 1.0f) {
        return std::min(base_strength + 0.1f, 0.95f);
    } else if (variance > 0.1f) {
        return base_strength;
    } else {
        return std::max(base_strength - resp * 0.5f, 0.5f);
    }
}

// ============================================================================
// Static Hook Callbacks
// ============================================================================

void CameraSmoother::turnDeltaDetour(float yaw_delta, float pitch_delta) {
    // This would be called from the hooked function
    // Need to get instance and call processTurnDelta
    // For now, just forward to original if available
    // In real implementation, use a singleton or thread-local instance
}

float CameraSmoother::tryGetFovDetour() {
    // Hook for FOV getter - could modify FOV for smoothing
    return 70.0f;  // Default FOV
}

void CameraSmoother::getCameraPerspectiveDetour() {
    // Hook for perspective matrix getter
}

void CameraSmoother::applyTurnDeltaDetour(void* vec2_ptr) {
    // Hook for applying turn delta from input
    // vec2_ptr would point to a Vec2 structure
}

} // namespace framegen