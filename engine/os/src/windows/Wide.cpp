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

Base::Result<std::wstring, OsError> Utf8ToWide(const std::string_view utf8) {
    if (utf8.empty()) return std::wstring{};
    if (utf8.size() > static_cast<std::size_t>(INT_MAX)) {
        return Base::Err(RefusedArgument(OsErrorCode::TooLarge, "text is too long", utf8.size()));
    }
    const int bytes = static_cast<int>(utf8.size());
    const int length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), bytes, nullptr, 0);
    if (length <= 0) {
        const DWORD error = ::GetLastError();
        return Base::Err(MakeOsError(OsErrorCode::InvalidEncoding, "text is not UTF-8",
                                     {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)}));
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), bytes, wide.data(), length) != length) {
        const DWORD error = ::GetLastError();
        return Base::Err(MakeOsError(OsErrorCode::InvalidEncoding, "text is not UTF-8",
                                     {NativeErrorDomain::Win32, static_cast<std::int64_t>(error)}));
    }
    return wide;
}

} // namespace Engine::Os::Detail
