/**
 * Mock preloader headers for building FrameGen without the actual preloader library.
 * These are stubs to allow compilation - the actual symbols are provided by libpreloader.so at runtime.
 */

#pragma once

#include <cstdint>
#include <string_view>
#include <functional>

namespace pl {

namespace input {
    struct KeyEvent {
        int key_code;
        int action;
        int64_t timestamp_ns;
    };
    
    using KeyCallback = std::function<bool(const KeyEvent&)>;
    
    void registerKeyCallback(KeyCallback callback);
}

namespace memory {
    enum class HookPriority {
        Normal = 0,
        High = 1,
        Critical = 2
    };
    
    void* hook(void* target, void* replacement, void** original, HookPriority priority = HookPriority::Normal);
    void unhook(void* target, void* replacement);
    
    std::string_view resolveSignature(std::string_view class_name, std::string_view method_name);
    void* resolveVtableFunction(std::string_view class_name, std::string_view method_name, uintptr_t vtable_offset);
}

namespace modmenu {
    struct ModuleInfo {
        std::string_view id;
        std::string_view name;
        std::string_view version;
        std::string_view author;
        bool enabled;
    };
    
    struct DrawCommand {
        // Simplified draw command structure
        uint32_t type;
        float data[16];
    };
    
    void registerModule(const ModuleInfo& info);
    void unregisterModule(std::string_view module_id);
    void submitDrawCommands(std::string_view module_id, const void* commands, size_t count);
}

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
    // bool PLMod_Load(void* mod, void* ctx);
    // bool PLMod_Enable(void* mod, void* ctx);
    // void PLMod_Disable(void* mod, void* ctx);
    // void PLMod_Unload(void* mod, void* ctx);
}

} // namespace pl

// Preloader core functions (from libpreloader.so)
extern "C" {
    // Core initialization
    void GlossInit();
    
    // Hooking API
    void* GlossHook(void* target, void* replacement, void** original);
    void* GlossHookAddr(void* target, void* replacement, void** original);
    void* GlossHookByName(const char* library, const char* symbol, void* replacement, void** original);
    void* GlossHookAddrByName(void* address, void* replacement, void** original);
    void* GlossHookBranchBL(void* target, void* replacement, void** original);
    void* GlossPltHook(void* got_entry, void* replacement, void** original);
    void* GlossGotHook(void* got_entry, void* replacement, void** original);
    
    void GlossHookEnable(void* hook_handle);
    void GlossHookDisable(void* hook_handle);
    void GlossHookDelete(void* hook_handle);
    void GlossHookDeleteAll();
    void GlossHookEnableAll();
    void GlossHookDisableAll();
    
    // Symbol resolution
    void* GlossSymbol(const char* name);
    void* GlossSymbolEx(const char* library, const char* symbol);
    void* GlossGOT(void* handle, const char* symbol);
    
    // Library management
    void* GlossOpen(const char* path, int flags);
    void GlossClose(void* handle);
    void* GlossGetLibEntry(void* handle);
    void* GlossGetLibPath(void* handle);
    void* GlossGetLibPathEx(void* handle);
    void* GlossGetLibBias(void* handle);
    void* GlossGetLibBiasEx(void* handle);
    void* GlossGetLibSection(void* handle, const char* section);
    void* GlossGetLibSegment(void* handle, int segment);
    void* GlossGetLibFileSize(void* handle);
    void* GlossGetLibMapping(void* handle);
    
    // Hook management
    void* GlossHookGetLastHook();
    void* GlossHookGetNextHook(void* hook);
    void* GlossHookGetPrevHook(void* hook);
    void* GlossHookGetFixedInst(void* hook);
    void* GlossHookGetBackupsInst(void* hook);
    void* GlossHookGetOriglFunc(void* hook);
    void* GlossHookGetCount();
    void* GlossHookGetTotalCount();
    
    // Hook manipulation
    void* GlossHookReplaceNewFunc(void* hook, void* new_func);
    void* GlossHookRedirect(void* hook, void* target);
    void* GlossHookAddTrampolines(void* hook, void* trampolines, size_t count);
    
    // Memory permission
    int GetMemoryPermission(void* addr);
    
    // JNI
    jint JNI_OnLoad(JavaVM* vm, void* reserved);
}

} // extern "C"

namespace pl {
    namespace preloader {
        // Preloader initialization
        void initialize();
        void shutdown();
        
        // Mod management
        bool loadMod(const char* path, void** mod_handle);
        bool enableMod(void* mod_handle);
        void disableMod(void* mod_handle);
        void unloadMod(void* mod_handle);
    }
}

