// Windows implements the file, process and console primitives itself
// (sys/windows), so their steps of the shared implementation are empty here
// (sys/Platform.hpp) and nothing on Windows calls them. UserDataBase is the one
// step Windows implements: sys/UserData.cpp is shared by every platform.
#include "../Platform.hpp"

#include "Wide.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>

namespace GYO::Engine::OS::Detail {
namespace {

class WindowsPlatform final : public Platform {
public:
    const timespec& ModifiedTimespec(const struct stat&) const noexcept override {
        static const timespec none{};
        return none;
    }
    std::vector<std::string> FontDirectoryCandidates() const override { return {}; }
    bool CaseSensitivePaths() const noexcept override { return false; }
    Base::Result<std::string, OSError> NativeExecutablePath() const override {
        return Base::Err(OSError::Make(OSErrorCode::Unsupported, "not used on Windows"));
    }

    // The Roaming AppData known folder (%APPDATA%). SHGetFolderPathW wraps
    // SHGetKnownFolderPath (FOLDERID_RoamingAppData) and lives in shell32, which
    // engine/os already links; SHGetKnownFolderPath would need ole32 for
    // CoTaskMemFree (Design Invariant 3).
    Base::Result<std::string, OSError> UserDataBase() const override {
        wchar_t path[MAX_PATH] = {};
        const HRESULT result =
            ::SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, path);
        if (FAILED(result)) {
            if (HRESULT_FACILITY(result) == FACILITY_WIN32) {
                return Base::Err(MakeOSError(OSErrorCode::IOFailed, "cannot name the user data directory",
                    {NativeErrorDomain::Win32, static_cast<std::int64_t>(HRESULT_CODE(result))}));
            }
            return Base::Err(OSError::Make(OSErrorCode::IOFailed, "cannot name the user data directory"));
        }
        auto utf8 = WideToUtf8(path);
        if (!utf8) return Base::Err(std::move(utf8.error()));
        std::string base = std::move(utf8.value());
        std::replace(base.begin(), base.end(), '\\', '/');
        return base;
    }
};

} // namespace

const Platform& CurrentPlatform() noexcept {
    static const WindowsPlatform platform;
    return platform;
}

} // namespace GYO::Engine::OS::Detail
