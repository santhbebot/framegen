// Factory implementations - this file includes all concrete implementations
#include "framegen/config/ConfigManager.hpp"
#include "framegen/interpolation/GlFrameInterpolator.hpp"
#include "framegen/motion/CameraSmoother.hpp"
#include "framegen/pacing/FramePacer.hpp"
#include "framegen/hud/HudRenderer.hpp"
#include "framegen/launcher/LauncherIntegration.hpp"

namespace framegen {

std::unique_ptr<IConfigManager> createConfigManager() {
    return std::make_unique<ConfigManager>();
}

std::unique_ptr<IFrameInterpolator> createFrameInterpolator() {
    return std::make_unique<GlFrameInterpolator>();
}

std::unique_ptr<ICameraSmoother> createCameraSmoother() {
    return std::make_unique<CameraSmoother>();
}

std::unique_ptr<IFramePacer> createFramePacer() {
    return std::make_unique<FramePacer>();
}

std::unique_ptr<IHudRenderer> createHudRenderer() {
    return std::make_unique<HudRenderer>();
}

std::unique_ptr<ILauncherIntegration> createLauncherIntegration(IConfigManager* config,
                                                                IFrameInterpolator* interpolator,
                                                                ICameraSmoother* camera,
                                                                IFramePacer* pacer,
                                                                IHudRenderer* hud) {
    return std::make_unique<LauncherIntegration>(config, interpolator, camera, pacer, hud);
}

} // namespace framegen