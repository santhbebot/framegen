#include "framegen/pacing/FramePacer.hpp"
#include <android/log.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define LOG_TAG "FrameGenPacer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace framegen {

// ============================================================================
// FramePacer Implementation
// ============================================================================

FramePacer::FramePacer() {
    // Initialize frame interval for 60 FPS
    frame_interval_ns_ = 1000000000LL / 60;  // ~16.67ms
    min_frame_interval_ns_ = 1000000000LL / 240;  // ~4.17ms (240 FPS cap for safety)
    last_frame_ns_ = 0;
    
    LOGI("FramePacer initialized: target_fps=%d, interval_ns=%ld", 
         target_fps_.load(), frame_interval_ns_);
}

void FramePacer::paceForTargetFps(int target_fps, int64_t frame_start_ns) {
    if (!enable_pacing_.load() || target_fps <= 0) {
        return;
    }
    
    // Clamp target FPS
    int clamped_fps = std::clamp(target_fps, min_fps_.load(), 5000);
    target_fps_.store(clamped_fps);
    
    // Calculate frame interval
    int64_t new_interval = 1000000000LL / clamped_fps;
    frame_interval_ns_ = new_interval;
    
    // Calculate target present time
    int64_t target_present_ns;
    if (last_frame_ns_ == 0) {
        // First frame
        target_present_ns = frame_start_ns + frame_interval_ns_;
    } else {
        // Next frame should be at last_frame + interval
        target_present_ns = last_frame_ns_ + frame_interval_ns_;
        
        // If we're already past the target, adjust to next interval
        if (target_present_ns < frame_start_ns) {
            int64_t elapsed = frame_start_ns - last_frame_ns_;
            int64_t frames_behind = (elapsed + frame_interval_ns_ - 1) / frame_interval_ns_;
            target_present_ns = last_frame_ns_ + frames_behind * frame_interval_ns_;
        }
    }
    
    // Try to use EGL presentation time extension for precise timing
    if (vsync_mode_.load() != VSyncMode::Disabled) {
        if (tryUsePresentationTime(target_present_ns)) {
            last_frame_ns_ = target_present_ns;
            return;
        }
    }
    
    // Fallback: sleep/wait until target time
    int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    if (target_present_ns > now) {
        int64_t sleep_ns = target_present_ns - now;
        
        // For very short waits, use busy wait for precision
        if (sleep_ns < 2000000) {  // < 2ms
            busyWaitUntil(target_present_ns);
        } else {
            sleepUntil(target_present_ns);
        }
    }
    
    last_frame_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void FramePacer::recordFrame(int64_t timestamp_ns, int frame_type, bool is_interpolated) {
    frame_count_.fetch_add(1, std::memory_order_relaxed);
    
    if (is_interpolated) {
        interpolated_count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Calculate frame time
    if (last_frame_ns_ > 0) {
        float frame_time = static_cast<float>(timestamp_ns - last_frame_ns_) / 1000000.0f;  // ms
        frame_time_ms_.store(frame_time, std::memory_order_relaxed);
        
        // Calculate jitter (frame-to-frame variation)
        float jitter = std::abs(frame_time - prev_frame_time_ms_);
        jitter_ms_.store(jitter, std::memory_order_relaxed);
        prev_frame_time_ms_ = frame_time;
        
        // Store in history for percentile calculation
        size_t idx = history_index_.fetch_add(1, std::memory_order_relaxed) % HISTORY_SIZE;
        frame_time_history_[idx] = frame_time;
        size_t count = history_count_.load(std::memory_order_relaxed);
        if (count < HISTORY_SIZE) {
            history_count_.store(count + 1, std::memory_order_relaxed);
        }
        
        // Update percentiles periodically
        if ((frame_count_.load(std::memory_order_relaxed) % 60) == 0) {
            updatePercentiles();
        }
        
        // Update FPS
        if (frame_time > 0.0f) {
            current_fps_.store(1000.0f / frame_time, std::memory_order_relaxed);
        }
    }
    
    last_frame_ns_ = timestamp_ns;
}

void FramePacer::reset() {
    last_frame_ns_ = 0;
    frame_count_.store(0, std::memory_order_relaxed);
    interpolated_count_.store(0, std::memory_order_relaxed);
    dropped_count_.store(0, std::memory_order_relaxed);
    current_fps_.store(0.0f, std::memory_order_relaxed);
    effective_fps_.store(0.0f, std::memory_order_relaxed);
    frame_time_ms_.store(0.0f, std::memory_order_relaxed);
    jitter_ms_.store(0.0f, std::memory_order_relaxed);
    p99_frame_time_ms_.store(0.0f, std::memory_order_relaxed);
    p999_frame_time_ms_.store(0.0f, std::memory_order_relaxed);
    history_index_.store(0, std::memory_order_relaxed);
    history_count_.store(0, std::memory_order_relaxed);
    prev_frame_time_ms_ = 0.0f;
    frame_time_history_.fill(0.0f);
    
    LOGI("FramePacer reset");
}

void FramePacer::updateConfig(const FrameGenConfig& config) {
    enable_pacing_.store(config.enable_frame_pacing, std::memory_order_relaxed);
    vsync_mode_.store(config.vsync_mode, std::memory_order_relaxed);
    min_fps_.store(config.min_fps, std::memory_order_relaxed);
    
    if (config.max_fps > 0) {
        target_fps_.store(std::clamp(config.max_fps, config.min_fps, 5000), std::memory_order_relaxed);
    }
    
    frame_interval_ns_ = 1000000000LL / target_fps_.load();
    min_frame_interval_ns_ = 1000000000LL / 240;  // Safety cap
    
    LOGI("FramePacer config updated: target_fps=%d, vsync=%d, pacing=%s",
         target_fps_.load(), static_cast<int>(vsync_mode_.load()),
         enable_pacing_.load() ? "enabled" : "disabled");
}

RuntimeTelemetrySnapshot FramePacer::getMetrics() const {
    RuntimeTelemetrySnapshot snapshot;
    snapshot.fps = current_fps_.load(std::memory_order_relaxed);
    snapshot.effective_fps = effective_fps_.load(std::memory_order_relaxed);
    snapshot.frame_time_ms = frame_time_ms_.load(std::memory_order_relaxed);
    snapshot.jitter_ms = jitter_ms_.load(std::memory_order_relaxed);
    snapshot.p99_frame_time_ms = p99_frame_time_ms_.load(std::memory_order_relaxed);
    snapshot.p999_frame_time_ms = p999_frame_time_ms_.load(std::memory_order_relaxed);
    snapshot.interpolated_frames_this_second = interpolated_count_.load(std::memory_order_relaxed);
    snapshot.dropped_frames_this_second = dropped_count_.load(std::memory_order_relaxed);
    return snapshot;
}

// ============================================================================
// Private Methods
// ============================================================================

bool FramePacer::tryUsePresentationTime(int64_t target_present_ns) {
    // Try to use EGL_ANDROID_presentation_time extension
    // This requires access to EGLDisplay and EGLSurface which we don't have here
    // The actual implementation would be in the GL hook where we have the display/surface
    // For now, return false to use fallback timing
    return false;
}

void FramePacer::busyWaitUntil(int64_t target_ns) {
    auto now = std::chrono::steady_clock::now();
    auto target = std::chrono::time_point<std::chrono::steady_clock>(
        std::chrono::nanoseconds(target_ns));
    
    while (now < target) {
        // Short spin - yield to other threads occasionally
        static int spin_count = 0;
        if (++spin_count >= 100) {
            std::this_thread::yield();
            spin_count = 0;
        }
        now = std::chrono::steady_clock::now();
    }
}

void FramePacer::sleepUntil(int64_t target_ns) {
    auto now = std::chrono::steady_clock::now();
    auto target = std::chrono::time_point<std::chrono::steady_clock>(
        std::chrono::nanoseconds(target_ns));
    
    if (now < target) {
        // Sleep for most of the time, leave ~1ms for busy wait
        auto sleep_duration = target - now - std::chrono::milliseconds(1);
        if (sleep_duration > std::chrono::nanoseconds(0)) {
            std::this_thread::sleep_for(sleep_duration);
        }
        
        // Busy wait for the remaining time
        busyWaitUntil(target_ns);
    }
}

float FramePacer::calculatePercentile(float percentile) const {
    size_t count = history_count_.load(std::memory_order_relaxed);
    if (count == 0) return 0.0f;
    
    // Copy valid samples
    std::vector<float> samples;
    samples.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        float v = frame_time_history_[i];
        if (v > 0.0f) samples.push_back(v);
    }
    
    if (samples.empty()) return 0.0f;
    
    // Sort for percentile
    std::sort(samples.begin(), samples.end());
    
    size_t index = static_cast<size_t>(std::ceil(percentile / 100.0f * samples.size())) - 1;
    index = std::min(index, samples.size() - 1);
    
    return samples[index];
}

void FramePacer::updatePercentiles() {
    p99_frame_time_ms_.store(calculatePercentile(99.0f), std::memory_order_relaxed);
    p999_frame_time_ms_.store(calculatePercentile(99.9f), std::memory_order_relaxed);
    
    // Calculate effective FPS (including interpolated frames)
    uint64_t total_frames = frame_count_.load(std::memory_order_relaxed) + 
                           interpolated_count_.load(std::memory_order_relaxed);
    if (total_frames > 0 && frame_time_ms_.load(std::memory_order_relaxed) > 0) {
        effective_fps_.store(1000.0f / frame_time_ms_.load(std::memory_order_relaxed) * 
                            (1.0f + static_cast<float>(interpolated_count_.load()) / 
                             std::max<uint64_t>(1, frame_count_.load())),
                            std::memory_order_relaxed);
    }
}

} // namespace framegen