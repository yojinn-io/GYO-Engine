#include "engine/os/OsError.hpp"

#include <utility>

namespace Engine::Os {

std::string NativeErrorDetail(const NativeError native, const std::optional<std::size_t> pathBytes) {
    std::string text = native.domain == NativeErrorDomain::Win32 ? "win32=" : "errno=";
    text += std::to_string(native.value);
    if (pathBytes) {
        text += "; path_bytes=";
        text += std::to_string(*pathBytes);
    }
    return text;
}

OsError MakeOsError(const OsErrorCode code, std::string message, const NativeError native,
                    const std::optional<std::size_t> pathBytes) {
    return OsError::Make(code, std::move(message), NativeErrorDetail(native, pathBytes));
}

} // namespace Engine::Os
