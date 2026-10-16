#include "framegen/jni.hpp"
#include "framegen/FrameGenMod.hpp"
#include "framegen/FrameGenCore.hpp"
#include <android/log.h>
#include <string>

#define LOG_TAG "FrameGenJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace framegen {

// Global JavaVM
static JavaVM* g_java_vm = nullptr;

// Get JNIEnv for current thread
static JNIEnv* getJNIEnv() {
    if (!g_java_vm) return nullptr;
    
    JNIEnv* env = nullptr;
    jint res = g_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (res == JNI_EDETACHED) {
        res = g_java_vm->AttachCurrentThread(&env, nullptr);
        if (res != JNI_OK) {
            LOGE("Failed to attach thread: %d", res);
            return nullptr;
        }
    } else if (res != JNI_OK) {
        LOGE("Failed to get JNIEnv: %d", res);
        return nullptr;
    }
    return env;
}

} // namespace framegen

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnLoad called");
    
    framegen::g_java_vm = vm;
    
    // Get JNIEnv
    JNIEnv* env = nullptr;
    jint res = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (res != JNI_OK) {
        LOGE("JNI_OnLoad: GetEnv failed: %d", res);
        return JNI_ERR;
    }
    
    // Register native methods if needed
    // In a real implementation, would register with RegisterNatives
    
    LOGI("JNI_OnLoad successful");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnUnload called");
    framegen::g_java_vm = nullptr;
    (void)vm;
    (void)reserved;
}

// ============================================================================
// FPS Mod Interface (legacy compatibility)
// ============================================================================

JNIEXPORT jint JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FpsMod_nativeGetFps(JNIEnv* env, jobject thiz) {
    LOGI("FpsMod_nativeGetFps called");
    
    auto& mod = framegen::FrameGenMod::instance();
    const auto& pacer = mod.getConfigManager();  // Would need to expose pacer
    // For now, return a reasonable default
    return 60;
}

// ============================================================================
// FrameGen Mod Interface
// ============================================================================

JNIEXPORT jint JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeInitialize(
    JNIEnv* env, jobject thiz, jint width, jint height) {
    
    LOGI("FrameGenMod_nativeInitialize(%d, %d)", width, height);
    
    auto& mod = framegen::FrameGenMod::instance();
    
    // Load and enable
    if (!mod.load(nullptr)) {
        LOGE("Failed to load mod");
        return -1;
    }
    
    if (!mod.enable(nullptr)) {
        LOGE("Failed to enable mod");
        return -2;
    }
    
    // Initialize interpolator with actual dimensions
    // This would be called from the interpolator directly
    // mod.getInterpolator()->initialize(width, height);
    
    LOGI("FrameGenMod initialized successfully");
    return 0;
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeShutdown(
    JNIEnv* env, jobject thiz) {
    
    LOGI("FrameGenMod_nativeShutdown");
    
    auto& mod = framegen::FrameGenMod::instance();
    mod.unload(nullptr);
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeSetConfig(
    JNIEnv* env, jobject thiz, jstring json_config) {
    
    LOGI("FrameGenMod_nativeSetConfig");
    
    if (!json_config) {
        LOGW("Null config string");
        return;
    }
    
    const char* json_str = env->GetStringUTFChars(json_config, nullptr);
    if (!json_str) return;
    
    // Parse JSON and apply config
    // For now, simple key:value parsing
    auto& config_manager = framegen::FrameGenMod::instance().getConfigManager();
    // Would need a proper JSON parser here
    
    env->ReleaseStringUTFChars(json_config, json_str);
}

JNIEXPORT jstring JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeGetConfig(
    JNIEnv* env, jobject thiz) {
    
    LOGI("FrameGenMod_nativeGetConfig");
    
    auto& config_manager = framegen::FrameGenMod::instance().getConfigManager();
    const auto& config = config_manager.getConfig();
    
    // Generate JSON config string
    std::string json = "{";
    json += "\"preset\":" + std::to_string(static_cast<int>(config.preset)) + ",";
    json += "\"frame_gen_mode\":" + std::to_string(static_cast<int>(config.frame_gen_mode)) + ",";
    json += "\"multiplier\":" + std::to_string(config.multiplier) + ",";
    json += "\"blend_strength\":" + std::to_string(config.blend_strength) + ",";
    json += "\"motion_scale\":" + std::to_string(config.motion_scale) + ",";
    json += "\"camera_mode\":" + std::to_string(static_cast<int>(config.camera_mode)) + ",";
    json += "\"camera_smoothing_strength\":" + std::to_string(config.camera_smoothing_strength) + ",";
    json += "\"camera_responsiveness\":" + std::to_string(config.camera_responsiveness) + ",";
    json += "\"hud_style\":" + std::to_string(static_cast<int>(config.hud_style)) + ",";
    json += "\"hud_optimizer\":" + std::to_string(static_cast<int>(config.hud_optimizer)) + ",";
    json += "\"hud_protection\":" + std::string(config.hud_protection ? "true" : "false") + ",";
    json += "\"show_fps_graph\":" + std::string(config.show_fps_graph ? "true" : "false") + ",";
    json += "\"show_frame_times\":" + std::string(config.show_frame_times ? "true" : "false") + ",";
    json += "\"show_memory_usage\":" + std::string(config.show_memory_usage ? "true" : "false") + ",";
    json += "\"scene_cut_threshold\":" + std::to_string(config.scene_cut_threshold) + ",";
    json += "\"vsync_mode\":" + std::to_string(static_cast<int>(config.vsync_mode)) + ",";
    json += "\"max_fps\":" + std::to_string(config.max_fps) + ",";
    json += "\"min_fps\":" + std::to_string(config.min_fps) + ",";
    json += "\"enable_motion_vectors\":" + std::string(config.enable_motion_vectors ? "true" : "false") + ",";
    json += "\"enable_optical_flow\":" + std::string(config.enable_optical_flow ? "true" : "false") + ",";
    json += "\"max_interpolation_frames\":" + std::to_string(config.max_interpolation_frames) + ",";
    json += "\"gpu_memory_budget_mb\":" + std::to_string(config.gpu_memory_budget_mb) + ",";
    json += "\"enable_frame_pacing\":" + std::string(config.enable_frame_pacing ? "true" : "false") + ",";
    json += "\"low_latency_mode\":" + std::string(config.low_latency_mode ? "true" : "false") + ",";
    json += "\"debug_mode\":" + std::string(config.debug_mode ? "true" : "false");
    json += "}";
    
    return env->NewStringUTF(json.c_str());
}

JNIEXPORT jstring JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeGetTelemetry(
    JNIEnv* env, jobject thiz) {
    
    LOGI("FrameGenMod_nativeGetTelemetry");
    
    auto& mod = framegen::FrameGenMod::instance();
    // Would need to get telemetry from interpolator
    framegen::RuntimeTelemetrySnapshot snapshot;
    snapshot.fps = 60.0f;
    snapshot.effective_fps = 120.0f;
    snapshot.frame_time_ms = 16.67f;
    snapshot.jitter_ms = 0.5f;
    snapshot.p99_frame_time_ms = 20.0f;
    snapshot.p999_frame_time_ms = 33.33f;
    snapshot.interpolated_frames_this_second = 60;
    snapshot.dropped_frames_this_second = 0;
    snapshot.scene_cut_detected = false;
    snapshot.camera_smoothing_applied = 0.85f;
    snapshot.gpu_memory_used_bytes = 64 * 1024 * 1024;
    snapshot.cpu_memory_used_bytes = 32 * 1024 * 1024;
    
    // Generate JSON telemetry
    std::string json = "{";
    json += "\"fps\":" + std::to_string(snapshot.fps) + ",";
    json += "\"effective_fps\":" + std::to_string(snapshot.effective_fps) + ",";
    json += "\"frame_time_ms\":" + std::to_string(snapshot.frame_time_ms) + ",";
    json += "\"jitter_ms\":" + std::to_string(snapshot.jitter_ms) + ",";
    json += "\"p99_frame_time_ms\":" + std::to_string(snapshot.p99_frame_time_ms) + ",";
    json += "\"p999_frame_time_ms\":" + std::to_string(snapshot.p999_frame_time_ms) + ",";
    json += "\"interpolated_frames\":" + std::to_string(snapshot.interpolated_frames_this_second) + ",";
    json += "\"dropped_frames\":" + std::to_string(snapshot.dropped_frames_this_second) + ",";
    json += "\"scene_cut\":" + std::string(snapshot.scene_cut_detected ? "true" : "false") + ",";
    json += "\"camera_smoothing\":" + std::to_string(snapshot.camera_smoothing_applied) + ",";
    json += "\"gpu_memory_mb\":" + std::to_string(snapshot.gpu_memory_used_bytes / (1024*1024)) + ",";
    json += "\"cpu_memory_mb\":" + std::to_string(snapshot.cpu_memory_used_bytes / (1024*1024));
    json += "}";
    
    return env->NewStringUTF(json.c_str());
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeToggle(
    JNIEnv* env, jobject thiz, jboolean enabled) {
    
    LOGI("FrameGenMod_nativeToggle: %s", enabled ? "true" : "false");
    
    framegen::FrameGenMod::instance().handleToggle(static_cast<bool>(enabled));
}

JNIEXPORT void JNICALL
Java_org_levimc_launcher_core_mods_inbuilt_nativemod_FrameGenMod_nativeCyclePreset(
    JNIEnv* env, jobject thiz) {
    
    LOGI("FrameGenMod_nativeCyclePreset");
    
    framegen::FrameGenMod::instance().cyclePresetInGame();
}

} // extern "C"