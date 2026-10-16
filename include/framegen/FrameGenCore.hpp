#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <atomic>
#include <functional>
#include <vector>
#include <memory>

namespace framegen {

// ============================================================================
// Core Types and Enums
// ============================================================================

enum class QualityPreset : uint32_t {
    Custom = 0,
    Performance,
    Balanced,
    Quality
};

enum class FrameGenMode : uint32_t {
    Off = 0,
    SingleSwap,      // 1 interpolated frame per game frame (2x)
    DoubleSwap,      // 2 interpolated frames per game frame (3x)
    TripleSwap       // 3 interpolated frames per game frame (4x)
};

enum class CameraSmoothingMode : uint32_t {
    None = 0,
    EMA,             // Exponential Moving Average
    Spring,          // Spring-damper system
    AdaptiveSmart    // Adaptive based on motion history
};

enum class HudStyle : uint32_t {
    Minimal = 0,
    Standard,
    Detailed,
    PerformanceGraph
};

enum class HudOptimizerMode : uint32_t {
    None = 0,
    HideDefaultFPS,
    HideAllOverlays,
    CustomOnly
};

enum class VSyncMode : uint32_t {
    Auto = 0,        // Use system/default
    Enabled,         // Force VSync on
    Disabled         // Force VSync off (unlock FPS)
};

// ============================================================================
// Configuration Structures
// ============================================================================

struct FrameGenConfig {
    // Preset and mode
    QualityPreset preset = QualityPreset::Balanced;
    FrameGenMode frame_gen_mode = FrameGenMode::SingleSwap;
    
    // Frame generation parameters
    float multiplier = 2.0f;              // Target FPS multiplier (1.0 - 10.0)
    float blend_strength = 1.0f;          // Frame blend strength (0.0 - 1.0)
    float motion_scale = 1.0f;            // Motion vector scale (0.0 - 2.0)
    
    // Camera smoothing
    CameraSmoothingMode camera_mode = CameraSmoothingMode::AdaptiveSmart;
    float camera_smoothing_strength = 0.85f;  // EMA alpha / spring stiffness
    float camera_responsiveness = 0.15f;      // Spring damping / adaptive factor
    
    // HUD and visual
    HudStyle hud_style = HudStyle::Standard;
    HudOptimizerMode hud_optimizer = HudOptimizerMode::HideDefaultFPS;
    bool hud_protection = true;           // Protect HUD from interpolation artifacts
    bool show_fps_graph = true;
    bool show_frame_times = true;
    bool show_memory_usage = false;
    
    // Scene cut detection
    float scene_cut_threshold = 42.0f;    // Threshold for detecting scene cuts (0-100)
    
    // FPS control
    VSyncMode vsync_mode = VSyncMode::Auto;
    int32_t max_fps = 0;                  // 0 = unlimited, 60-5000 when set
    int32_t min_fps = 30;                 // Minimum target FPS
    
    // Performance
    bool enable_motion_vectors = true;    // Use motion vectors when available
    bool enable_optical_flow = false;     // Compute optical flow (expensive)
    uint32_t max_interpolation_frames = 3; // Max extra frames per game frame
    float gpu_memory_budget_mb = 128.0f;  // GPU memory budget for frame buffers
    
    // Advanced
    bool enable_frame_pacing = true;      // Adaptive frame pacing
    bool low_latency_mode = true;         // Reduce input latency
    bool debug_mode = false;              // Extra logging and validation
    
    // Normalize and validate configuration
    void normalize();
    
    // Apply a quality preset
    void applyPreset(QualityPreset value);
    
    // Equality for change detection
    bool operator==(const FrameGenConfig& other) const;
    bool operator!=(const FrameGenConfig& other) const { return !(*this == other); }
};

// Runtime telemetry snapshot (produced each frame)
struct RuntimeTelemetrySnapshot {
    uint64_t frame_number = 0;
    uint64_t timestamp_ns = 0;
    float delta_time_ms = 0.0f;
    float fps = 0.0f;
    float effective_fps = 0.0f;           // Including interpolated frames
    float frame_time_ms = 0.0f;
    float jitter_ms = 0.0f;
    float p99_frame_time_ms = 0.0f;
    float p999_frame_time_ms = 0.0f;
    uint64_t gpu_memory_used_bytes = 0;
    uint64_t cpu_memory_used_bytes = 0;
    uint32_t interpolated_frames_this_second = 0;
    uint32_t dropped_frames_this_second = 0;
    bool scene_cut_detected = false;
    float camera_smoothing_applied = 0.0f;
};

// ============================================================================
// Module Interfaces (pure virtual for modularity)
// ============================================================================

class IConfigManager {
public:
    virtual ~IConfigManager() = default;
    virtual FrameGenConfig& getConfig() = 0;
    virtual const FrameGenConfig& getConfig() const = 0;
    virtual bool loadFromFile(const std::string& path) = 0;
    virtual bool saveToFile(const std::string& path) const = 0;
    virtual bool applyKeyValue(std::string_view key, std::string_view value) = 0;
    virtual std::string getConfigFilePath() const = 0;
    virtual FrameGenConfig getSnapshot() const = 0;
};

class IFrameInterpolator {
public:
    virtual ~IFrameInterpolator() = default;
    virtual bool initialize(int width, int height) = 0;
    virtual void shutdown() = 0;
    virtual bool installHook() = 0;
    virtual void uninstallHook() = 0;
    virtual void onSwapBuffers(void* display, void* surface) = 0;
    virtual void updateConfig(const FrameGenConfig& config) = 0;
    virtual void setPostFrameCallback(std::function<void(const RuntimeTelemetrySnapshot&)> callback) = 0;
    virtual const RuntimeTelemetrySnapshot& getTelemetry() const = 0;
    virtual void setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;
};

class ICameraSmoother {
public:
    virtual ~ICameraSmoother() = default;
    virtual bool installHooks() = 0;
    virtual void uninstallHooks() = 0;
    virtual void processTurnDelta(float yaw_delta, float pitch_delta, int64_t timestamp_ns) = 0;
    virtual void consumeFrameMotion(float motion, int64_t timestamp_ns) = 0;
    virtual void updateConfig(const FrameGenConfig& config) = 0;
    virtual void reset() = 0;
};

class IFramePacer {
public:
    virtual ~IFramePacer() = default;
    virtual void paceForTargetFps(int target_fps, int64_t frame_start_ns) = 0;
    virtual void recordFrame(int64_t timestamp_ns, int frame_type, bool is_interpolated) = 0;
    virtual void reset() = 0;
    virtual void updateConfig(const FrameGenConfig& config) = 0;
    virtual RuntimeTelemetrySnapshot getMetrics() const = 0;
};

class IHudRenderer {
public:
    virtual ~IHudRenderer() = default;
    virtual bool initialize(void* java_vm, const FrameGenConfig& config) = 0;
    virtual void shutdown() = 0;
    virtual void publishHud(const FrameGenConfig& config, const RuntimeTelemetrySnapshot& snapshot) = 0;
    virtual void updateConfig(const FrameGenConfig& config) = 0;
    virtual void clearHud() = 0;
};

class ILauncherIntegration {
public:
    virtual ~ILauncherIntegration() = default;
    virtual bool load(void* mod_context) = 0;
    virtual bool enable(void* mod_context) = 0;
    virtual void unload(void* mod_context) = 0;
    virtual void disable(void* mod_context) = 0;
    virtual void onKeyEvent(const void* key_event) = 0;
    virtual void onModuleToggle(std::string_view module_name, bool enabled) = 0;
    virtual void onConfigChanged(std::string_view key, std::string_view value) = 0;
    virtual void onFramePresented(const RuntimeTelemetrySnapshot& snapshot) = 0;
    virtual void handleToggle(bool enabled) = 0;
    virtual void cyclePresetInGame() = 0;
    virtual void handleConfigChange(std::string_view key, std::string_view value) = 0;
    virtual void syncSubsystemsAndPersist(bool force) = 0;
};

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<IConfigManager> createConfigManager();
std::unique_ptr<IFrameInterpolator> createFrameInterpolator();
std::unique_ptr<ICameraSmoother> createCameraSmoother();
std::unique_ptr<IFramePacer> createFramePacer();
std::unique_ptr<IHudRenderer> createHudRenderer();
std::unique_ptr<ILauncherIntegration> createLauncherIntegration(IConfigManager* config,
                                                                 IFrameInterpolator* interpolator,
                                                                 ICameraSmoother* camera,
                                                                 IFramePacer* pacer,
                                                                 IHudRenderer* hud);

// ============================================================================
// Utility Functions
// ============================================================================

const char* qualityPresetName(QualityPreset preset);
const char* frameGenModeName(FrameGenMode mode);
const char* cameraSmoothingModeName(CameraSmoothingMode mode);
const char* hudStyleName(HudStyle style);
const char* hudOptimizerModeName(HudOptimizerMode mode);
const char* vsyncModeName(VSyncMode mode);

bool parseQualityPreset(std::string_view str, QualityPreset& out);
bool parseFrameGenMode(std::string_view str, FrameGenMode& out);
bool parseCameraSmoothingMode(std::string_view str, CameraSmoothingMode& out);
bool parseHudStyle(std::string_view str, HudStyle& out);
bool parseHudOptimizerMode(std::string_view str, HudOptimizerMode& out);
bool parseVSyncMode(std::string_view str, VSyncMode& out);

} // namespace framegen