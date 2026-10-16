#include "framegen/config/ConfigManager.hpp"
#include "framegen/FrameGenCore.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <android/log.h>

#define LOG_TAG "FrameGenConfig"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace framegen {

// Forward declarations
bool applyConfigKeyValue(FrameGenConfig& config, std::string_view key, std::string_view value);
bool loadConfigFromFile(const std::string& path, FrameGenConfig& config);
bool saveConfigToFile(const std::string& path, const FrameGenConfig& config);

// ============================================================================
// ConfigManager Implementation
// ============================================================================

ConfigManager::ConfigManager() {
    config_.normalize();
    config_.applyPreset(config_.preset);
}

FrameGenConfig& ConfigManager::getConfig() { return config_; }
const FrameGenConfig& ConfigManager::getConfig() const { return config_; }

bool ConfigManager::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOGW("Config file not found: %s, using defaults", path.c_str());
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Simple JSON-like parsing (key: value per line)
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.empty() || line[0] == '#' || line[0] == '/') continue;
        
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            applyKeyValue(key, value);
        }
    }
    
    config_.normalize();
    LOGI("Config loaded from: %s", path.c_str());
    return true;
}

bool ConfigManager::saveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        LOGE("Failed to open config file for writing: %s", path.c_str());
        return false;
    }
    
    file << "# FrameGen Configuration\n";
    file << "# Generated automatically - edit with care\n\n";
    
    file << "preset: " << static_cast<int>(config_.preset) << "\n";
    file << "frame_gen_mode: " << static_cast<int>(config_.frame_gen_mode) << "\n";
    file << "multiplier: " << config_.multiplier << "\n";
    file << "blend_strength: " << config_.blend_strength << "\n";
    file << "motion_scale: " << config_.motion_scale << "\n";
    file << "camera_mode: " << static_cast<int>(config_.camera_mode) << "\n";
    file << "camera_smoothing_strength: " << config_.camera_smoothing_strength << "\n";
    file << "camera_responsiveness: " << config_.camera_responsiveness << "\n";
    file << "hud_style: " << static_cast<int>(config_.hud_style) << "\n";
    file << "hud_optimizer: " << static_cast<int>(config_.hud_optimizer) << "\n";
    file << "hud_protection: " << (config_.hud_protection ? "true" : "false") << "\n";
    file << "show_fps_graph: " << (config_.show_fps_graph ? "true" : "false") << "\n";
    file << "show_frame_times: " << (config_.show_frame_times ? "true" : "false") << "\n";
    file << "show_memory_usage: " << (config_.show_memory_usage ? "true" : "false") << "\n";
    file << "scene_cut_threshold: " << config_.scene_cut_threshold << "\n";
    file << "vsync_mode: " << static_cast<int>(config_.vsync_mode) << "\n";
    file << "max_fps: " << config_.max_fps << "\n";
    file << "min_fps: " << config_.min_fps << "\n";
    file << "enable_motion_vectors: " << (config_.enable_motion_vectors ? "true" : "false") << "\n";
    file << "enable_optical_flow: " << (config_.enable_optical_flow ? "true" : "false") << "\n";
    file << "max_interpolation_frames: " << config_.max_interpolation_frames << "\n";
    file << "gpu_memory_budget_mb: " << config_.gpu_memory_budget_mb << "\n";
    file << "enable_frame_pacing: " << (config_.enable_frame_pacing ? "true" : "false") << "\n";
    file << "low_latency_mode: " << (config_.low_latency_mode ? "true" : "false") << "\n";
    file << "debug_mode: " << (config_.debug_mode ? "true" : "false") << "\n";
    
    LOGI("Config saved to: %s", path.c_str());
    return true;
}

bool ConfigManager::applyKeyValue(std::string_view key, std::string_view value) {
    return applyConfigKeyValue(config_, key, value);
}

std::string ConfigManager::getConfigFilePath() const {
    return "/data/data/com.termux/files/home/jng_framegen_work/config.json";
}

FrameGenConfig ConfigManager::getSnapshot() const { return config_; }

// ============================================================================
// FrameGenConfig Implementation (moved from FrameGenCore.hpp)
// ============================================================================

void FrameGenConfig::normalize() {
    // Clamp multiplier
    multiplier = std::clamp(multiplier, 1.0f, 10.0f);
    
    // Clamp blend strength
    blend_strength = std::clamp(blend_strength, 0.0f, 1.0f);
    
    // Clamp motion scale
    motion_scale = std::clamp(motion_scale, 0.0f, 2.0f);
    
    // Clamp camera smoothing strength
    camera_smoothing_strength = std::clamp(camera_smoothing_strength, 0.0f, 1.0f);
    
    // Clamp camera responsiveness
    camera_responsiveness = std::clamp(camera_responsiveness, 0.0f, 1.0f);
    
    // Clamp scene cut threshold
    scene_cut_threshold = std::clamp(scene_cut_threshold, 0.0f, 100.0f);
    
    // Clamp max_fps
    if (max_fps != 0) {
        max_fps = std::clamp(max_fps, 60, 5000);
    }
    
    // Clamp min_fps
    min_fps = std::clamp(min_fps, 10, 240);
    
    // Clamp GPU memory budget
    gpu_memory_budget_mb = std::clamp(gpu_memory_budget_mb, 16.0f, 1024.0f);
    
    // Clamp max interpolation frames
    max_interpolation_frames = std::clamp(max_interpolation_frames, 1u, 3u);
    
    LOGI("Config normalized: multiplier=%.2f, blend=%.2f, motion=%.2f, max_fps=%d",
         multiplier, blend_strength, motion_scale, max_fps);
}

void FrameGenConfig::applyPreset(QualityPreset value) {
    preset = value;
    
    switch (value) {
        case QualityPreset::Performance:
            frame_gen_mode = FrameGenMode::SingleSwap;
            multiplier = 1.5f;
            blend_strength = 0.5f;
            motion_scale = 0.5f;
            camera_mode = CameraSmoothingMode::EMA;
            camera_smoothing_strength = 0.7f;
            camera_responsiveness = 0.3f;
            hud_style = HudStyle::Minimal;
            hud_optimizer = HudOptimizerMode::HideAllOverlays;
            enable_optical_flow = false;
            max_interpolation_frames = 1;
            gpu_memory_budget_mb = 64.0f;
            low_latency_mode = true;
            break;
            
        case QualityPreset::Balanced:
            frame_gen_mode = FrameGenMode::SingleSwap;
            multiplier = 2.0f;
            blend_strength = 0.75f;
            motion_scale = 0.75f;
            camera_mode = CameraSmoothingMode::AdaptiveSmart;
            camera_smoothing_strength = 0.85f;
            camera_responsiveness = 0.15f;
            hud_style = HudStyle::Standard;
            hud_optimizer = HudOptimizerMode::HideDefaultFPS;
            enable_optical_flow = false;
            max_interpolation_frames = 2;
            gpu_memory_budget_mb = 128.0f;
            low_latency_mode = true;
            break;
            
        case QualityPreset::Quality:
            frame_gen_mode = FrameGenMode::DoubleSwap;
            multiplier = 3.0f;
            blend_strength = 1.0f;
            motion_scale = 1.0f;
            camera_mode = CameraSmoothingMode::Spring;
            camera_smoothing_strength = 0.9f;
            camera_responsiveness = 0.1f;
            hud_style = HudStyle::Detailed;
            hud_optimizer = HudOptimizerMode::None;
            enable_optical_flow = true;
            max_interpolation_frames = 3;
            gpu_memory_budget_mb = 256.0f;
            low_latency_mode = false;
            break;
            
        case QualityPreset::Custom:
        default:
            // Keep current values, just normalize
            break;
    }
    
    normalize();
    LOGI("Applied preset: %d", static_cast<int>(value));
}

bool FrameGenConfig::operator==(const FrameGenConfig& other) const {
    return preset == other.preset &&
           frame_gen_mode == other.frame_gen_mode &&
           std::abs(multiplier - other.multiplier) < 0.001f &&
           std::abs(blend_strength - other.blend_strength) < 0.001f &&
           std::abs(motion_scale - other.motion_scale) < 0.001f &&
           camera_mode == other.camera_mode &&
           std::abs(camera_smoothing_strength - other.camera_smoothing_strength) < 0.001f &&
           std::abs(camera_responsiveness - other.camera_responsiveness) < 0.001f &&
           hud_style == other.hud_style &&
           hud_optimizer == other.hud_optimizer &&
           hud_protection == other.hud_protection &&
           show_fps_graph == other.show_fps_graph &&
           show_frame_times == other.show_frame_times &&
           show_memory_usage == other.show_memory_usage &&
           std::abs(scene_cut_threshold - other.scene_cut_threshold) < 0.001f &&
           vsync_mode == other.vsync_mode &&
           max_fps == other.max_fps &&
           min_fps == other.min_fps &&
           enable_motion_vectors == other.enable_motion_vectors &&
           enable_optical_flow == other.enable_optical_flow &&
           max_interpolation_frames == other.max_interpolation_frames &&
           std::abs(gpu_memory_budget_mb - other.gpu_memory_budget_mb) < 0.001f &&
           enable_frame_pacing == other.enable_frame_pacing &&
           low_latency_mode == other.low_latency_mode &&
           debug_mode == other.debug_mode;
}

// ============================================================================
// applyConfigKeyValue Implementation
// ============================================================================

bool applyConfigKeyValue(FrameGenConfig& config, std::string_view key, std::string_view value) {
    auto parseBool = [](std::string_view v) -> bool {
        return v == "true" || v == "1" || v == "yes" || v == "on";
    };
    
    auto parseFloat = [](std::string_view v) -> float {
        try { return std::stof(std::string(v)); } catch (...) { return 0.0f; }
    };
    
    auto parseInt = [](std::string_view v) -> int {
        try { return std::stoi(std::string(v)); } catch (...) { return 0; }
    };
    
    auto parseUInt = [](std::string_view v) -> uint32_t {
        try { return std::stoul(std::string(v)); } catch (...) { return 0; }
    };
    
    if (key == "preset") {
        QualityPreset p;
        if (parseQualityPreset(value, p)) config.applyPreset(p);
        else if (auto i = parseInt(value); i >= 0 && i <= 3) config.applyPreset(static_cast<QualityPreset>(i));
    }
    else if (key == "frame_gen_mode") {
        FrameGenMode m;
        if (parseFrameGenMode(value, m)) config.frame_gen_mode = m;
        else if (auto i = parseInt(value); i >= 0 && i <= 3) config.frame_gen_mode = static_cast<FrameGenMode>(i);
    }
    else if (key == "multiplier") config.multiplier = parseFloat(value);
    else if (key == "blend_strength") config.blend_strength = parseFloat(value);
    else if (key == "motion_scale") config.motion_scale = parseFloat(value);
    else if (key == "camera_mode") {
        CameraSmoothingMode m;
        if (parseCameraSmoothingMode(value, m)) config.camera_mode = m;
        else if (auto i = parseInt(value); i >= 0 && i <= 3) config.camera_mode = static_cast<CameraSmoothingMode>(i);
    }
    else if (key == "camera_smoothing_strength") config.camera_smoothing_strength = parseFloat(value);
    else if (key == "camera_responsiveness") config.camera_responsiveness = parseFloat(value);
    else if (key == "hud_style") {
        HudStyle s;
        if (parseHudStyle(value, s)) config.hud_style = s;
        else if (auto i = parseInt(value); i >= 0 && i <= 3) config.hud_style = static_cast<HudStyle>(i);
    }
    else if (key == "hud_optimizer") {
        HudOptimizerMode m;
        if (parseHudOptimizerMode(value, m)) config.hud_optimizer = m;
        else if (auto i = parseInt(value); i >= 0 && i <= 3) config.hud_optimizer = static_cast<HudOptimizerMode>(i);
    }
    else if (key == "hud_protection") config.hud_protection = parseBool(value);
    else if (key == "show_fps_graph") config.show_fps_graph = parseBool(value);
    else if (key == "show_frame_times") config.show_frame_times = parseBool(value);
    else if (key == "show_memory_usage") config.show_memory_usage = parseBool(value);
    else if (key == "scene_cut_threshold") config.scene_cut_threshold = parseFloat(value);
    else if (key == "vsync_mode") {
        VSyncMode m;
        if (parseVSyncMode(value, m)) config.vsync_mode = m;
        else if (auto i = parseInt(value); i >= 0 && i <= 2) config.vsync_mode = static_cast<VSyncMode>(i);
    }
    else if (key == "max_fps") config.max_fps = parseInt(value);
    else if (key == "min_fps") config.min_fps = parseInt(value);
    else if (key == "enable_motion_vectors") config.enable_motion_vectors = parseBool(value);
    else if (key == "enable_optical_flow") config.enable_optical_flow = parseBool(value);
    else if (key == "max_interpolation_frames") config.max_interpolation_frames = parseUInt(value);
    else if (key == "gpu_memory_budget_mb") config.gpu_memory_budget_mb = parseFloat(value);
    else if (key == "enable_frame_pacing") config.enable_frame_pacing = parseBool(value);
    else if (key == "low_latency_mode") config.low_latency_mode = parseBool(value);
    else if (key == "debug_mode") config.debug_mode = parseBool(value);
    else {
        LOGW("Unknown config key: %s", std::string(key).c_str());
        return false;
    }
    
    config.normalize();
    return true;
}

bool loadConfigFromFile(const std::string& path, FrameGenConfig& config) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.empty() || line[0] == '#' || line[0] == '/') continue;
        
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string k = line.substr(0, colon);
            std::string v = line.substr(colon + 1);
            k.erase(0, k.find_first_not_of(" \t"));
            k.erase(k.find_last_not_of(" \t") + 1);
            v.erase(0, v.find_first_not_of(" \t"));
            v.erase(v.find_last_not_of(" \t") + 1);
            applyConfigKeyValue(config, k, v);
        }
    }
    
    config.normalize();
    return true;
}

bool saveConfigToFile(const std::string& path, const FrameGenConfig& config) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    
    file << "# FrameGen Configuration\n";
    file << "# Generated automatically\n\n";
    
    file << "preset: " << static_cast<int>(config.preset) << "\n";
    file << "frame_gen_mode: " << static_cast<int>(config.frame_gen_mode) << "\n";
    file << "multiplier: " << config.multiplier << "\n";
    file << "blend_strength: " << config.blend_strength << "\n";
    file << "motion_scale: " << config.motion_scale << "\n";
    file << "camera_mode: " << static_cast<int>(config.camera_mode) << "\n";
    file << "camera_smoothing_strength: " << config.camera_smoothing_strength << "\n";
    file << "camera_responsiveness: " << config.camera_responsiveness << "\n";
    file << "hud_style: " << static_cast<int>(config.hud_style) << "\n";
    file << "hud_optimizer: " << static_cast<int>(config.hud_optimizer) << "\n";
    file << "hud_protection: " << (config.hud_protection ? "true" : "false") << "\n";
    file << "show_fps_graph: " << (config.show_fps_graph ? "true" : "false") << "\n";
    file << "show_frame_times: " << (config.show_frame_times ? "true" : "false") << "\n";
    file << "show_memory_usage: " << (config.show_memory_usage ? "true" : "false") << "\n";
    file << "scene_cut_threshold: " << config.scene_cut_threshold << "\n";
    file << "vsync_mode: " << static_cast<int>(config.vsync_mode) << "\n";
    file << "max_fps: " << config.max_fps << "\n";
    file << "min_fps: " << config.min_fps << "\n";
    file << "enable_motion_vectors: " << (config.enable_motion_vectors ? "true" : "false") << "\n";
    file << "enable_optical_flow: " << (config.enable_optical_flow ? "true" : "false") << "\n";
    file << "max_interpolation_frames: " << config.max_interpolation_frames << "\n";
    file << "gpu_memory_budget_mb: " << config.gpu_memory_budget_mb << "\n";
    file << "enable_frame_pacing: " << (config.enable_frame_pacing ? "true" : "false") << "\n";
    file << "low_latency_mode: " << (config.low_latency_mode ? "true" : "false") << "\n";
    file << "debug_mode: " << (config.debug_mode ? "true" : "false") << "\n";
    
    return true;
}

// ============================================================================
// String Parsing Utilities
// ============================================================================

bool parseQualityPreset(std::string_view str, QualityPreset& out) {
    if (str == "custom" || str == "Custom" || str == "0") { out = QualityPreset::Custom; return true; }
    if (str == "performance" || str == "Performance" || str == "1") { out = QualityPreset::Performance; return true; }
    if (str == "balanced" || str == "Balanced" || str == "2") { out = QualityPreset::Balanced; return true; }
    if (str == "quality" || str == "Quality" || str == "3") { out = QualityPreset::Quality; return true; }
    return false;
}

bool parseFrameGenMode(std::string_view str, FrameGenMode& out) {
    if (str == "off" || str == "Off" || str == "0") { out = FrameGenMode::Off; return true; }
    if (str == "single" || str == "SingleSwap" || str == "1") { out = FrameGenMode::SingleSwap; return true; }
    if (str == "double" || str == "DoubleSwap" || str == "2") { out = FrameGenMode::DoubleSwap; return true; }
    if (str == "triple" || str == "TripleSwap" || str == "3") { out = FrameGenMode::TripleSwap; return true; }
    return false;
}

bool parseCameraSmoothingMode(std::string_view str, CameraSmoothingMode& out) {
    if (str == "none" || str == "None" || str == "0") { out = CameraSmoothingMode::None; return true; }
    if (str == "ema" || str == "EMA" || str == "1") { out = CameraSmoothingMode::EMA; return true; }
    if (str == "spring" || str == "Spring" || str == "2") { out = CameraSmoothingMode::Spring; return true; }
    if (str == "adaptive" || str == "AdaptiveSmart" || str == "3") { out = CameraSmoothingMode::AdaptiveSmart; return true; }
    return false;
}

bool parseHudStyle(std::string_view str, HudStyle& out) {
    if (str == "minimal" || str == "Minimal" || str == "0") { out = HudStyle::Minimal; return true; }
    if (str == "standard" || str == "Standard" || str == "1") { out = HudStyle::Standard; return true; }
    if (str == "detailed" || str == "Detailed" || str == "2") { out = HudStyle::Detailed; return true; }
    if (str == "graph" || str == "PerformanceGraph" || str == "3") { out = HudStyle::PerformanceGraph; return true; }
    return false;
}

bool parseHudOptimizerMode(std::string_view str, HudOptimizerMode& out) {
    if (str == "none" || str == "None" || str == "0") { out = HudOptimizerMode::None; return true; }
    if (str == "hide_fps" || str == "HideDefaultFPS" || str == "1") { out = HudOptimizerMode::HideDefaultFPS; return true; }
    if (str == "hide_all" || str == "HideAllOverlays" || str == "2") { out = HudOptimizerMode::HideAllOverlays; return true; }
    if (str == "custom" || str == "CustomOnly" || str == "3") { out = HudOptimizerMode::CustomOnly; return true; }
    return false;
}

bool parseVSyncMode(std::string_view str, VSyncMode& out) {
    if (str == "auto" || str == "Auto" || str == "0") { out = VSyncMode::Auto; return true; }
    if (str == "enabled" || str == "Enabled" || str == "on" || str == "1") { out = VSyncMode::Enabled; return true; }
    if (str == "disabled" || str == "Disabled" || str == "off" || str == "2") { out = VSyncMode::Disabled; return true; }
    return false;
}

const char* qualityPresetName(QualityPreset preset) {
    switch (preset) {
        case QualityPreset::Custom: return "Custom";
        case QualityPreset::Performance: return "Performance";
        case QualityPreset::Balanced: return "Balanced";
        case QualityPreset::Quality: return "Quality";
    }
    return "Unknown";
}

const char* frameGenModeName(FrameGenMode mode) {
    switch (mode) {
        case FrameGenMode::Off: return "Off";
        case FrameGenMode::SingleSwap: return "SingleSwap";
        case FrameGenMode::DoubleSwap: return "DoubleSwap";
        case FrameGenMode::TripleSwap: return "TripleSwap";
    }
    return "Unknown";
}

const char* cameraSmoothingModeName(CameraSmoothingMode mode) {
    switch (mode) {
        case CameraSmoothingMode::None: return "None";
        case CameraSmoothingMode::EMA: return "EMA";
        case CameraSmoothingMode::Spring: return "Spring";
        case CameraSmoothingMode::AdaptiveSmart: return "AdaptiveSmart";
    }
    return "Unknown";
}

const char* hudStyleName(HudStyle style) {
    switch (style) {
        case HudStyle::Minimal: return "Minimal";
        case HudStyle::Standard: return "Standard";
        case HudStyle::Detailed: return "Detailed";
        case HudStyle::PerformanceGraph: return "PerformanceGraph";
    }
    return "Unknown";
}

const char* hudOptimizerModeName(HudOptimizerMode mode) {
    switch (mode) {
        case HudOptimizerMode::None: return "None";
        case HudOptimizerMode::HideDefaultFPS: return "HideDefaultFPS";
        case HudOptimizerMode::HideAllOverlays: return "HideAllOverlays";
        case HudOptimizerMode::CustomOnly: return "CustomOnly";
    }
    return "Unknown";
}

const char* vsyncModeName(VSyncMode mode) {
    switch (mode) {
        case VSyncMode::Auto: return "Auto";
        case VSyncMode::Enabled: return "Enabled";
        case VSyncMode::Disabled: return "Disabled";
    }
    return "Unknown";
}

} // namespace framegen