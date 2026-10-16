#pragma once

#include <cstdint>
#include <cstddef>

namespace pl {
namespace mod {

struct ModContext {
    void* launcher_handle;
    void* mod_handle;
    const char* mod_id;
    const char* mod_path;
    void* config_data;
    size_t config_size;
};

// Lifecycle functions that external native mods must export
// These are the functions the preloader calls
// bool PLMod_Load(void* mod, void* ctx);
// bool PLMod_Enable(void* mod, void* ctx);
// void PLMod_Disable(void* mod, void* ctx);
// void PLMod_Unload(void* mod, void* ctx);

} // namespace mod
} // namespace pl

