#include "Wide.hpp"

#include "../common/Paths.hpp"

#include <climits>
#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Engine::Os::Detail {

Base::Result<std::string, OsError> WideToUtf8(const std::wstring_view wide) {
    if (wide.empty()) return std::string{};
    if (wide.size() > static_cast<std::size_t>(INT_MAX)) {
        return Base::Err(RefusedArgument(OsErrorCode::TooLarge, "text is too long", wide.size()));
    }
    const int length = static_cast<int>(wide.size());
    const int bytes = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), length, nullptr, 0, nullptr,
                                            nullptr);
    if (bytes <= 0) {
        const DWORD error = ::GetLastError();
        return Base::Err(MakeOsError(OsErrorCode::InvalidEncoding, "text has no UTF-8 form",
                                     {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)}));
    }
    std::string utf8(static_cast<std::size_t>(bytes), '\0');
    if (::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), length, utf8.data(), bytes, nullptr,
                              nullptr) != bytes) {
        const DWORD error = ::GetLastError();
        return Base::Err(MakeOsError(OsErrorCode::InvalidEncoding, "text has no UTF-8 form",
                                     {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)}));
    }
    return utf8;
}

} // namespace Engine::Os::Detail
