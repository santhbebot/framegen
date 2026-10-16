#pragma once

#include <cstdint>

// Forward declarations for Levi Launcher plugin interface
namespace pl {
    namespace mod {
        class ModContext;
    }
}

namespace framegen {
    class FrameGenMod;
}

// ============================================================================
// External Native Mod Plugin Registration for Levi Launcher 1.5.26
// ============================================================================
// External native mods (loaded via levipack) must export these four lifecycle
// functions directly. The launcher resolves these symbols and calls them with:
// - mod: pointer to the mod instance (FrameGenMod*)
// - ctx: pointer to pl::mod::ModContext
//
// PLGetModRegistration is for internal C++ mods only - NOT for external native mods.
// ============================================================================

extern "C" {

// Called when the mod library is loaded
// Returns true on success, false on failure
bool PLMod_Load(void* mod, void* ctx);

// Called when the mod is enabled
// Returns true on success, false on failure
bool PLMod_Enable(void* mod, void* ctx);

// Called when the mod is disabled
void PLMod_Disable(void* mod, void* ctx);

// Called when the mod is unloaded
void PLMod_Unload(void* mod, void* ctx);

} // extern "C"