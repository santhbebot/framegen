#pragma once

#include "framegen/FrameGenCore.hpp"
#include <memory>
#include <atomic>

namespace framegen {

// Forward declarations
class IConfigManager;
class IFrameInterpolator;
class ICameraSmoother;
class IFramePacer;
class IHudRenderer;

// ============================================================================
// LauncherIntegration Implementation
// ============================================================================

class LauncherIntegration : public ILauncherIntegration {
public:
    LauncherIntegration(IConfigManager* config,
                       IFrameInterpolator* interpolator,
                       ICameraSmoother* camera,
                       IFramePacer* pacer,
                       IHudRenderer* hud);
    ~LauncherIntegration() override;
    
    // PL Mod Lifecycle
    bool load(void* mod_context) override;
    bool enable(void* mod_context) override;
    void unload(void* mod_context) override;
    void disable(void* mod_context) override;
    
    // Event handlers
    void onKeyEvent(const void* key_event) override;
    void onModuleToggle(std::string_view module_name, bool enabled) override;
    void onConfigChanged(std::string_view key, std::string_view value) override;
    void onFramePresented(const RuntimeTelemetrySnapshot& snapshot) override;
    
    // Control
    void handleToggle(bool enabled) override;
    void cyclePresetInGame() override;
    void handleConfigChange(std::string_view key, std::string_view value) override;
    void syncSubsystemsAndPersist(bool force) override;
    
    // Singleton access
    static LauncherIntegration& instance();
    static void setInstance(LauncherIntegration* instance);

private:
    // Subsystems (not owned)
    IConfigManager* config_manager_;
    IFrameInterpolator* interpolator_;
    ICameraSmoother* camera_smoother_;
    IFramePacer* frame_pacer_;
    IHudRenderer* hud_renderer_;
    
    // State
    std::atomic<bool> loaded_{false};
    std::atomic<bool> enabled_{false};
    std::atomic<bool> interpolator_initialized_{false};
    std::atomic<int> current_preset_index_{1};  // Balanced default
    
    // Config file path
    std::string config_path_;
    
    // Internal methods
    void initializeSubsystems(void* mod_context);
    void shutdownSubsystems();
    void applyCurrentPreset();
    void notifyConfigChanged();
};

} // namespace framegen