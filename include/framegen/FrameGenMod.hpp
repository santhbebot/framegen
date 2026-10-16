#pragma once

#include "framegen/FrameGenCore.hpp"
#include <jni.h>
#include <memory>

namespace framegen {

// Forward declarations
class IConfigManager;
class IFrameInterpolator;
class ICameraSmoother;
class IFramePacer;
class IHudRenderer;

// ============================================================================
// FrameGenMod - Main Module Class
// ============================================================================

class FrameGenMod {
public:
    FrameGenMod();
    ~FrameGenMod();
    
    // Module lifecycle (called by launcher)
    bool load(void* mod_context);
    bool enable(void* mod_context);
    void unload(void* mod_context);
    void disable(void* mod_context);
    
    // Event handlers
    void onKeyEvent(const void* key_event);
    void onModuleToggle(std::string_view module_name, bool enabled);
    void onConfigChanged(std::string_view key, std::string_view value);
    void onFramePresented(const RuntimeTelemetrySnapshot& snapshot);
    
    // Control
    void handleToggle(bool enabled);
    void cyclePresetInGame();
    void handleConfigChange(std::string_view key, std::string_view value);
    void syncSubsystemsAndPersist(bool force);
    
    // Configuration
    IConfigManager& getConfigManager() { return *config_manager_; }
    const IConfigManager& getConfigManager() const { return *config_manager_; }
    FrameGenConfig& getConfig() { return config_manager_->getConfig(); }
    const FrameGenConfig& getConfig() const { return config_manager_->getConfig(); }
    std::string getConfigFilePath() const { return config_manager_->getConfigFilePath(); }
    FrameGenConfig getConfigSnapshot() const { return config_manager_->getSnapshot(); }
    
    // Singleton
    static FrameGenMod& instance();
    
private:
    // Subsystems
    std::unique_ptr<IConfigManager> config_manager_;
    std::unique_ptr<IFrameInterpolator> interpolator_;
    std::unique_ptr<ICameraSmoother> camera_smoother_;
    std::unique_ptr<IFramePacer> frame_pacer_;
    std::unique_ptr<IHudRenderer> hud_renderer_;
    std::unique_ptr<ILauncherIntegration> launcher_integration_;
    
    // State
    static FrameGenMod* instance_;
    JavaVM* java_vm_{nullptr};
    
    // Prevent copying
    FrameGenMod(const FrameGenMod&) = delete;
    FrameGenMod& operator=(const FrameGenMod&) = delete;
};

} // namespace framegen