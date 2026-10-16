#pragma once

#include <cstdint>
#include <string_view>

namespace pl {
namespace memory {

enum class HookPriority {
    Normal = 0,
    High = 1,
    Critical = 2
};

extern "C" {
    void* hook(void* target, void* replacement, void** original, int priority = 0);
    void unhook(void* target, void* replacement);
    
    const char* resolveSignature(const char* class_name, const char* method_name);
    void* resolveVtableFunction(const char* class_name, const char* method_name, uintptr_t vtable_offset);
    
    void* hook(void* target, void* replacement, void** original, int priority);
    void unhook(void* target, void* replacement);
}

} // namespace memory
} // namespace pl

