#include "framegen/PluginRegistration.hpp"
#include "framegen/FrameGenMod.hpp"
#include "framegen/FrameGenCore.hpp"
#include <android/log.h>

#define LOG_TAG "FrameGenPlugin"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace framegen {

// Static module instance
static FrameGenMod* g_module = nullptr;

} // namespace framegen

// C-style callbacks for External Native Mod ABI
extern "C" {

// Called when the mod library is loaded
// mod: pointer to store the mod instance (will be set to FrameGenMod*)
// ctx: pl::mod::ModContext*
bool PLMod_Load(void* mod, void* ctx) {
    LOGI("PLMod_Load called");
    
    if (!framegen::g_module) {
        framegen::g_module = new framegen::FrameGenMod();
    }
    
    // Store the module instance pointer
    *static_cast<framegen::FrameGenMod**>(mod) = framegen::g_module;
    
    return framegen::g_module->load(ctx);
}

// Called when the mod is enabled
// mod: FrameGenMod* (set by PLMod_Load)
// ctx: pl::mod::ModContext*
bool PLMod_Enable(void* mod, void* ctx) {
    LOGI("PLMod_Enable called");
    
    if (!framegen::g_module) {
        LOGE("Module not loaded");
        return false;
    }
    
    return framegen::g_module->enable(ctx);
}

// Called when the mod is disabled
// mod: FrameGenMod*
// ctx: pl::mod::ModContext*
void PLMod_Disable(void* mod, void* ctx) {
    LOGI("PLMod_Disable called");
    
    if (framegen::g_module) {
        framegen::g_module->disable(ctx);
    }
}

// Called when the mod is unloaded
// mod: FrameGenMod*
// ctx: pl::mod::ModContext*
void PLMod_Unload(void* mod, void* ctx) {
    LOGI("PLMod_Unload called");
    
    if (framegen::g_module) {
        framegen::g_module->unload(ctx);
        delete framegen::g_module;
        framegen::g_module = nullptr;
    }
    
    // Clear the stored pointer
    *static_cast<framegen::FrameGenMod**>(mod) = nullptr;
}

// PLGetModRegistration is for internal C++ mods only
// For external native mods (levipack), this should not be called
// We provide a stub that returns nullptr to avoid crashes if called
extern "C" void* PLGetModRegistration() {
    LOGW("PLGetModRegistration called - not used for external native mods");
    return nullptr;
}

} // extern "C"