#pragma once

#include <cstdint>
#include <string_view>

namespace pl {
namespace modmenu {

struct ModuleInfo {
    const char* id;
    const char* name;
    const char* version;
    const char* author;
    bool enabled;
};

struct DrawCommand {
    uint32_t type;
    float data[16];
};

extern "C" {
    void registerModule(const ModuleInfo* info);
    void unregisterModule(const char* module_id);
    void submitDrawCommands(const char* module_id, const void* commands, size_t count);
}

} // namespace modmenu
} // namespace pl

