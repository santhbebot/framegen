#pragma once

#include "framegen/FrameGenCore.hpp"
#include <string>
#include <string_view>

namespace framegen {

// ============================================================================
// ConfigManager Implementation
// ============================================================================

class ConfigManager : public IConfigManager {
public:
    ConfigManager();
    ~ConfigManager() override = default;
    
    FrameGenConfig& getConfig() override;
    const FrameGenConfig& getConfig() const override;
    bool loadFromFile(const std::string& path) override;
    bool saveToFile(const std::string& path) const override;
    bool applyKeyValue(std::string_view key, std::string_view value) override;
    std::string getConfigFilePath() const override;
    FrameGenConfig getSnapshot() const override;

private:
    FrameGenConfig config_;
};

} // namespace framegen