#ifndef MOCK_PL_MEMORY_H
#define MOCK_PL_MEMORY_H

#include <string_view>

namespace pl {
    namespace memory {
        enum class HookPriority { Normal = 0, High = 1, Critical = 2 };
        
        // External declarations - these are resolved from libpreloader.so at runtime
        // Using weak linkage so they can be overridden by libpreloader.so
        extern "C" {
            __attribute__((weak)) void* hook(void* target, void* detour, void** original, HookPriority priority);
            __attribute__((weak)) void unhook(void* target, void* detour);
            __attribute__((weak)) void* resolveSignature(std::string_view module, std::string_view symbol);
            __attribute__((weak)) void* resolveVtableFunction(std::string_view module, std::string_view symbol, size_t index, void* fallback);
        }
    }
}

#endif // MOCK_PL_MEMORY_H