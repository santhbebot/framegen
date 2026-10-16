#include "framegen/launcher/LauncherIntegration.hpp"
#include "framegen/FrameGenCore.hpp"
#include <android/log.h>
#include <dlfcn.h>

#define LOG_TAG "FrameGenLauncher"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace framegen {

// Static instance
static LauncherIntegration* g_instance = nullptr;

LauncherIntegration& LauncherIntegration::instance() {
    if (!g_instance) {
        // This should not happen if setInstance was called properly
        LOGE("LauncherIntegration instance not set!");
        // Return a reference to a static dummy to avoid crash
        static LauncherIntegration dummy(nullptr, nullptr, nullptr, nullptr, nullptr);
        return dummy;
    }
    return *g_instance;
}

void LauncherIntegration::setInstance(LauncherIntegration* instance) {
    g_instance = instance;
}

LauncherIntegration::LauncherIntegration(IConfigManager* config,
                                        IFrameInterpolator* interpolator,
                                        ICameraSmoother* camera,
                                        IFramePacer* pacer,
                                        IHudRenderer* hud)
    : config_manager_(config)
    , interpolator_(interpolator)
    , camera_smoother_(camera)
    , frame_pacer_(pacer)
    , hud_renderer_(hud)
    , config_path_(config ? config->getConfigFilePath() : "/data/data/com.termux/files/home/jng_framegen_work/config.json") {
    
    LOGI("LauncherIntegration constructed");
    setInstance(this);
}

LauncherIntegration::~LauncherIntegration() {
    LOGI("LauncherIntegration destroyed");
    shutdownSubsystems();
    if (g_instance == this) {
        g_instance = nullptr;
    }
}

bool LauncherIntegration::load(void* mod_context) {
    LOGI("LauncherIntegration::load");
    
    if (loaded_.exchange(true, std::memory_order_acq_rel)) {
        LOGW("Already loaded");
        return true;
    }
    
    // Load configuration
    if (config_manager_) {
        config_manager_->loadFromFile(config_path_);
        LOGI("Config loaded from: %s", config_path_.c_str());
    }
    
    // Initialize subsystems
    initializeSubsystems(mod_context);
    
    LOGI("LauncherIntegration loaded successfully");
    return true;
}

bool LauncherIntegration::enable(void* mod_context) {
    LOGI("LauncherIntegration::enable");
    
    if (!loaded_.load(std::memory_order_acquire)) {
        LOGE("Not loaded, cannot enable");
        return false;
    }
    
    if (enabled_.exchange(true, std::memory_order_acq_rel)) {
        LOGW("Already enabled");
        return true;
    }
    
    // Enable subsystems
    if (interpolator_) {
        interpolator_->setEnabled(true);
    }
    if (camera_smoother_) {
        camera_smoother_->installHooks();
    }
    if (frame_pacer_) {
        frame_pacer_->reset();
    }
    if (hud_renderer_) {
        // HUD is enabled by publishHud calls
    }
    
    LOGI("LauncherIntegration enabled successfully");
    return true;
}

void LauncherIntegration::unload(void* mod_context) {
    LOGI("LauncherIntegration::unload");
    
    if (!loaded_.load(std::memory_order_acquire)) {
        LOGW("Not loaded");
        return;
    }
    
    // Disable first
    disable(mod_context);
    
    // Save configuration
    if (config_manager_) {
        config_manager_->saveToFile(config_path_);
        LOGI("Config saved to: %s", config_path_.c_str());
    }
    
    // Shutdown subsystems
    shutdownSubsystems();
    
    loaded_.store(false, std::memory_order_release);
    LOGI("LauncherIntegration unloaded");
}

void LauncherIntegration::disable(void* mod_context) {
    LOGI("LauncherIntegration::disable");
    
    if (!enabled_.load(std::memory_order_acquire)) {
        LOGW("Not enabled");
        return;
    }
    
    // Disable subsystems
    if (interpolator_) {
        interpolator_->setEnabled(false);
    }
    if (camera_smoother_) {
        camera_smoother_->uninstallHooks();
    }
    if (hud_renderer_) {
        hud_renderer_->clearHud();
    }
    
    enabled_.store(false, std::memory_order_release);
    LOGI("LauncherIntegration disabled");
}

void LauncherIntegration::onKeyEvent(const void* key_event) {
    // Handle key events for in-game toggles
    // KeyEvent would contain key code, action, etc.
    // For now, just log
    LOGI("Key event received");
    (void)key_event;
}

void LauncherIntegration::onModuleToggle(std::string_view module_name, bool enabled) {
    LOGI("Module toggle: %s = %s", std::string(module_name).c_str(), enabled ? "enabled" : "disabled");
    
    if (module_name == "framegen" || module_name == "leviframegen") {
        handleToggle(enabled);
    }
    else if (module_name == "camera_smoothing" && camera_smoother_) {
        if (enabled) camera_smoother_->installHooks();
        else camera_smoother_->uninstallHooks();
    }
    else if (module_name == "frame_interpolation" && interpolator_) {
        interpolator_->setEnabled(enabled);
    }
    else if (module_name == "hud" && hud_renderer_) {
        if (!enabled) hud_renderer_->clearHud();
    }
}

void LauncherIntegration::onConfigChanged(std::string_view key, std::string_view value) {
    LOGI("Config changed: %s = %s", std::string(key).c_str(), std::string(value).c_str());
    
    if (config_manager_) {
        bool success = config_manager_->applyKeyValue(key, value);
        if (success) {
            notifyConfigChanged();
            
            // Persist immediately for important settings
            if (key == "preset" || key == "frame_gen_mode" || key == "max_fps" || key == "vsync_mode") {
                config_manager_->saveToFile(config_path_);
            }
        }
    }
}

void LauncherIntegration::onFramePresented(const RuntimeTelemetrySnapshot& snapshot) {
    // Update HUD with latest telemetry
    if (hud_renderer_ && config_manager_) {
        hud_renderer_->publishHud(config_manager_->getConfig(), snapshot);
    }
    
    // Update frame pacer metrics
    if (frame_pacer_) {
        frame_pacer_->recordFrame(snapshot.timestamp_ns, 0, false);
    }
}

void LauncherIntegration::handleToggle(bool enabled) {
    LOGI("Handle toggle: %s", enabled ? "enable" : "disable");
    
    if (enabled) {
        enable(nullptr);
    } else {
        disable(nullptr);
    }
}

void LauncherIntegration::cyclePresetInGame() {
    LOGI("Cycling preset in-game");
    
    if (!config_manager_) return;
    
    FrameGenConfig& config = config_manager_->getConfig();
    int current = static_cast<int>(config.preset);
    int next = (current + 1) % 4;  // Custom, Performance, Balanced, Quality
    
    config.applyPreset(static_cast<QualityPreset>(next));
    current_preset_index_.store(next, std::memory_order_relaxed);
    
    notifyConfigChanged();
    config_manager_->saveToFile(config_path_);
    
    LOGI("Preset cycled to: %s", qualityPresetName(config.preset));
}

void LauncherIntegration::handleConfigChange(std::string_view key, std::string_view value) {
    onConfigChanged(key, value);
}

void LauncherIntegration::syncSubsystemsAndPersist(bool force) {
    LOGI("Sync subsystems and persist: force=%s", force ? "true" : "false");
    
    if (config_manager_) {
        // Propagate config to all subsystems
        const FrameGenConfig& config = config_manager_->getConfig();
        
        if (interpolator_) interpolator_->updateConfig(config);
        if (camera_smoother_) camera_smoother_->updateConfig(config);
        if (frame_pacer_) frame_pacer_->updateConfig(config);
        if (hud_renderer_) hud_renderer_->updateConfig(config);
        
        if (force) {
            config_manager_->saveToFile(config_path_);
        }
    }
}

// ============================================================================
// Private Methods
// ============================================================================

void LauncherIntegration::initializeSubsystems(void* mod_context) {
    LOGI("Initializing subsystems");
    
    // Get display dimensions for interpolator
    // In real implementation, would query from EGL or game
    int width = 1920;   // Placeholder
    int height = 1080;  // Placeholder
    
    if (interpolator_) {
        if (interpolator_->initialize(width, height)) {
            interpolator_initialized_.store(true, std::memory_order_relaxed);
            interpolator_->setPostFrameCallback([this](const RuntimeTelemetrySnapshot& snap) {
                onFramePresented(snap);
            });
            LOGI("Interpolator initialized");
        } else {
            LOGE("Failed to initialize interpolator");
        }
    }
    
    if (camera_smoother_) {
        camera_smoother_->installHooks();
        LOGI("Camera smoother hooks installed");
    }
    
    if (frame_pacer_) {
        frame_pacer_->reset();
        LOGI("Frame pacer reset");
    }
    
    if (hud_renderer_) {
        // JavaVM would come from JNI_OnLoad
        void* java_vm = nullptr;  // Would be stored globally from JNI_OnLoad
        if (java_vm) {
            hud_renderer_->initialize(java_vm, config_manager_->getConfig());
            LOGI("HUD renderer initialized");
        }
    }
    
    // Apply current preset
    applyCurrentPreset();
}

void LauncherIntegration::shutdownSubsystems() {
    LOGI("Shutting down subsystems");
    
    if (interpolator_) {
        interpolator_->shutdown();
        interpolator_initialized_.store(false, std::memory_order_relaxed);
    }
    
    if (camera_smoother_) {
        camera_smoother_->uninstallHooks();
    }
    
    if (hud_renderer_) {
        hud_renderer_->shutdown();
    }
}

void LauncherIntegration::applyCurrentPreset() {
    if (!config_manager_) return;
    
    FrameGenConfig& config = config_manager_->getConfig();
    config.applyPreset(config.preset);
    notifyConfigChanged();
}

void LauncherIntegration::notifyConfigChanged() {
    if (!config_manager_) return;
    
    const FrameGenConfig& config = config_manager_->getConfig();
    
    if (interpolator_) interpolator_->updateConfig(config);
    if (camera_smoother_) camera_smoother_->updateConfig(config);
    if (frame_pacer_) frame_pacer_->updateConfig(config);
    if (hud_renderer_) hud_renderer_->updateConfig(config);
}

} // namespace framegen