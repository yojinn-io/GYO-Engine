#include "engine/os/OSError.hpp"

#include <utility>

namespace GYO::Engine::OS {

std::string NativeErrorDetail(const NativeError native, const std::optional<std::size_t> pathBytes) {
    std::string text = native.domain == NativeErrorDomain::Win32 ? "win32=" : "errno=";
    text += std::to_string(native.value);
    if (pathBytes) {
        text += "; path_bytes=";
        text += std::to_string(*pathBytes);
    }
    return text;
}

OSError MakeOSError(const OSErrorCode code, std::string message, const NativeError native,
                    const std::optional<std::size_t> pathBytes) {
    return OSError::Make(code, std::move(message), NativeErrorDetail(native, pathBytes));
}

} // namespace GYO::Engine::OS
