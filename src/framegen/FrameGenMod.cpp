#include "framegen/FrameGenMod.hpp"
#include "framegen/FrameGenCore.hpp"
#include "framegen/config/ConfigManager.hpp"
#include "framegen/interpolation/GlFrameInterpolator.hpp"
#include "framegen/motion/CameraSmoother.hpp"
#include "framegen/pacing/FramePacer.hpp"
#include "framegen/hud/HudRenderer.hpp"
#include "framegen/launcher/LauncherIntegration.hpp"
#include <android/log.h>

#define LOG_TAG "FrameGenMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace framegen {

// Static instance
FrameGenMod* FrameGenMod::instance_ = nullptr;

FrameGenMod::FrameGenMod() {
    LOGI("FrameGenMod constructor");
    
    // Create subsystems
    config_manager_ = createConfigManager();
    interpolator_ = createFrameInterpolator();
    camera_smoother_ = createCameraSmoother();
    frame_pacer_ = createFramePacer();
    hud_renderer_ = createHudRenderer();
    launcher_integration_ = createLauncherIntegration(
        config_manager_.get(),
        interpolator_.get(),
        camera_smoother_.get(),
        frame_pacer_.get(),
        hud_renderer_.get()
    );
    
    instance_ = this;
}

FrameGenMod::~FrameGenMod() {
    LOGI("FrameGenMod destructor");
    
    // Shutdown in reverse order
    launcher_integration_.reset();
    hud_renderer_.reset();
    frame_pacer_.reset();
    camera_smoother_.reset();
    interpolator_.reset();
    config_manager_.reset();
    
    instance_ = nullptr;
}

bool FrameGenMod::load(void* mod_context) {
    LOGI("FrameGenMod::load");
    return launcher_integration_->load(mod_context);
}

bool FrameGenMod::enable(void* mod_context) {
    LOGI("FrameGenMod::enable");
    return launcher_integration_->enable(mod_context);
}

void FrameGenMod::unload(void* mod_context) {
    LOGI("FrameGenMod::unload");
    launcher_integration_->unload(mod_context);
}

void FrameGenMod::disable(void* mod_context) {
    LOGI("FrameGenMod::disable");
    launcher_integration_->disable(mod_context);
}

void FrameGenMod::onKeyEvent(const void* key_event) {
    launcher_integration_->onKeyEvent(key_event);
}

void FrameGenMod::onModuleToggle(std::string_view module_name, bool enabled) {
    launcher_integration_->onModuleToggle(module_name, enabled);
}

void FrameGenMod::onConfigChanged(std::string_view key, std::string_view value) {
    launcher_integration_->onConfigChanged(key, value);
}

void FrameGenMod::onFramePresented(const RuntimeTelemetrySnapshot& snapshot) {
    launcher_integration_->onFramePresented(snapshot);
}

void FrameGenMod::handleToggle(bool enabled) {
    launcher_integration_->handleToggle(enabled);
}

void FrameGenMod::cyclePresetInGame() {
    launcher_integration_->cyclePresetInGame();
}

void FrameGenMod::handleConfigChange(std::string_view key, std::string_view value) {
    launcher_integration_->handleConfigChange(key, value);
}

void FrameGenMod::syncSubsystemsAndPersist(bool force) {
    launcher_integration_->syncSubsystemsAndPersist(force);
}

FrameGenMod& FrameGenMod::instance() {
    if (!instance_) {
        instance_ = new FrameGenMod();
    }
    return *instance_;
}

} // namespace framegen