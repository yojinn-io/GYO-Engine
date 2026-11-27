#include "engine/window_camera/WindowCameraModule.hpp"

#include <utility>

#include "engine/base/Result.hpp"
#include "engine/camera/Camera.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/window/Window.hpp"
#include "engine/window/WindowModule.hpp"

namespace GYO::Engine::WindowCamera {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

const WindowCameraOptions& OptionsOf(const WindowCameraOptions* given) {
    static const WindowCameraOptions defaults{};
    return given != nullptr ? *given : defaults;
}

// The main target is the window: its size once known, written only when it differs (camera's WriteTargetSize; a
// write every frame would mark CameraTargetSizes changed every frame).
SystemResult WriteWindowTargetSize(Kernel::Res<Window::WindowState> window, Kernel::ResMut<Camera::CameraTargetSizes> sizes) {
    const Window::WindowState& state = *window;
    if (!state.known) return {};
    Camera::WriteTargetSize(sizes, Camera::kMainTarget, Camera::TargetSize{state.physicalSize, state.scaleFactor});
    return {};
}

} // namespace

void WindowCameraModule::Build(Orchestration::WorldSetup& setup) {
    const WindowCameraOptions& options = OptionsOf(setup.Options<WindowCameraOptions>());
    Kernel::SystemOptions write;
    write.name = "WriteWindowTargetSize";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.stage;
    mount.after.push_back(std::string(Window::kWindowSet));
    write.mounts.push_back(std::move(mount));
    setup.Registrar().AddSystem<&WriteWindowTargetSize>(std::move(write));
}

} // namespace GYO::Engine::WindowCamera
