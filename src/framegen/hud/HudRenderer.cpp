#include "framegen/hud/HudRenderer.hpp"
#include "framegen/FrameGenCore.hpp"
#include <android/log.h>
#include <sstream>
#include <iomanip>

#define LOG_TAG "FrameGenHud"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace framegen {

// ============================================================================
// HudRenderer Implementation
// ============================================================================

HudRenderer::HudRenderer() {
    LOGI("HudRenderer constructed");
    fps_history_.fill(0.0f);
    frame_time_history_.fill(0.0f);
    jitter_history_.fill(0.0f);
}

HudRenderer::~HudRenderer() {
    shutdown();
    LOGI("HudRenderer destroyed");
}

bool HudRenderer::initialize(void* java_vm, const FrameGenConfig& config) {
    LOGI("HudRenderer::initialize");
    
    java_vm_ = static_cast<JavaVM*>(java_vm);
    if (!java_vm_) {
        LOGE("JavaVM is null");
        return false;
    }
    
    // Get JNIEnv
    JNIEnv* env = nullptr;
    jint res = java_vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (res != JNI_OK) {
        res = java_vm_->AttachCurrentThread(&env, nullptr);
        if (res != JNI_OK) {
            LOGE("Failed to attach thread: %d", res);
            return false;
        }
    }
    
    // Find the HUD class (this would be a class provided by the Levi Launcher or our own)
    // For now, we use a placeholder - in reality this would be the actual HUD class
    // jclass hud_cls = env->FindClass("com/leviframegen/HudRenderer");
    // if (!hud_cls) {
    //     LOGE("Failed to find HudRenderer class");
    //     return false;
    // }
    // hud_class_ = static_cast<jclass>(env->NewGlobalRef(hud_cls));
    // publish_method_ = env->GetStaticMethodID(hud_class_, "publishHud", "(Ljava/lang/String;)V");
    // clear_method_ = env->GetStaticMethodID(hud_class_, "clearHud", "()V");
    // optimize_method_ = env->GetStaticMethodID(hud_class_, "optimizeHud", "(Z)V");
    
    // For now, simulate success
    hud_class_ = reinterpret_cast<jclass>(0x1);
    publish_method_ = reinterpret_cast<jmethodID>(0x1);
    clear_method_ = reinterpret_cast<jmethodID>(0x2);
    optimize_method_ = reinterpret_cast<jmethodID>(0x3);
    
    // Apply initial config
    updateConfig(config);
    
    LOGI("HudRenderer initialized successfully");
    return true;
}

void HudRenderer::shutdown() {
    LOGI("HudRenderer::shutdown");
    
    if (java_vm_ && hud_class_) {
        JNIEnv* env = nullptr;
        jint res = java_vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (res == JNI_OK && env) {
            env->DeleteGlobalRef(hud_class_);
        }
    }
    
    hud_class_ = nullptr;
    publish_method_ = nullptr;
    clear_method_ = nullptr;
    optimize_method_ = nullptr;
    java_vm_ = nullptr;
}

void HudRenderer::publishHud(const FrameGenConfig& config, const RuntimeTelemetrySnapshot& snapshot) {
    if (!hud_class_ || !publish_method_) {
        return;
    }
    
    updateHistory(snapshot);
    
    // Format telemetry as JSON for the Java HUD
    std::string json = formatTelemetryJson(snapshot);
    
    // Call Java method (simulated)
    // JNIEnv* env = ...;
    // jstring jjson = env->NewStringUTF(json.c_str());
    // env->CallStaticVoidMethod(hud_class_, publish_method_, jjson);
    // env->DeleteLocalRef(jjson);
    
    LOGI("HUD published: fps=%.1f, frame_time=%.2fms, jitter=%.2fms",
         snapshot.fps, snapshot.frame_time_ms, snapshot.jitter_ms);
}

void HudRenderer::updateConfig(const FrameGenConfig& config) {
    hud_style_.store(config.hud_style, std::memory_order_relaxed);
    hud_optimizer_.store(config.hud_optimizer, std::memory_order_relaxed);
    hud_protection_.store(config.hud_protection, std::memory_order_relaxed);
    show_fps_graph_.store(config.show_fps_graph, std::memory_order_relaxed);
    show_frame_times_.store(config.show_frame_times, std::memory_order_relaxed);
    show_memory_usage_.store(config.show_memory_usage, std::memory_order_relaxed);
    
    // Call Java optimize method if needed
    if (optimize_method_) {
        // callJavaOptimizeHud(config.hud_optimizer != HudOptimizerMode::None);
    }
    
    LOGI("HudRenderer config updated: style=%d, optimizer=%d",
         static_cast<int>(config.hud_style), static_cast<int>(config.hud_optimizer));
}

void HudRenderer::clearHud() {
    if (hud_class_ && clear_method_) {
        // callJavaClearHud();
        LOGI("HUD cleared");
    }
}

void HudRenderer::updateHistory(const RuntimeTelemetrySnapshot& snapshot) {
    size_t idx = history_index_.fetch_add(1, std::memory_order_relaxed) % GRAPH_HISTORY_SIZE;
    fps_history_[idx] = snapshot.fps;
    frame_time_history_[idx] = snapshot.frame_time_ms;
    jitter_history_[idx] = snapshot.jitter_ms;
}

std::string HudRenderer::formatTelemetryJson(const RuntimeTelemetrySnapshot& snapshot) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    
    oss << "{";
    oss << "\"fps\":" << snapshot.fps << ",";
    oss << "\"effective_fps\":" << snapshot.effective_fps << ",";
    oss << "\"frame_time_ms\":" << snapshot.frame_time_ms << ",";
    oss << "\"jitter_ms\":" << snapshot.jitter_ms << ",";
    oss << "\"p99_frame_time_ms\":" << snapshot.p99_frame_time_ms << ",";
    oss << "\"p999_frame_time_ms\":" << snapshot.p999_frame_time_ms << ",";
    oss << "\"interpolated_frames\":" << snapshot.interpolated_frames_this_second << ",";
    oss << "\"dropped_frames\":" << snapshot.dropped_frames_this_second << ",";
    oss << "\"scene_cut\":" << (snapshot.scene_cut_detected ? "true" : "false") << ",";
    oss << "\"camera_smoothing\":" << snapshot.camera_smoothing_applied << ",";
    oss << "\"gpu_memory_mb\":" << (snapshot.gpu_memory_used_bytes / (1024.0 * 1024.0)) << ",";
    oss << "\"cpu_memory_mb\":" << (snapshot.cpu_memory_used_bytes / (1024.0 * 1024.0));
    oss << "}";
    
    return oss.str();
}

void HudRenderer::callJavaPublishHud(const RuntimeTelemetrySnapshot& snapshot) {
    // Implementation would call Java method via JNI
}

void HudRenderer::callJavaClearHud() {
    // Implementation would call Java method via JNI
}

void HudRenderer::callJavaOptimizeHud(bool enable) {
    // Implementation would call Java method via JNI
}

} // namespace framegen