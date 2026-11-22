// The macOS steps of engine/os's shared implementation (sys/Platform.hpp).
#include "../Platform.hpp"
#include "../../common/Paths.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <mach-o/dyld.h>

namespace GYO::Engine::OS::Detail {
namespace {

// $HOME when it is an absolute path, else null.
const char* Home() noexcept {
    const char* home = std::getenv("HOME");
    return home != nullptr && home[0] == '/' ? home : nullptr;
}

class MacOSPlatform final : public Platform {
public:
    const timespec& ModifiedTimespec(const struct stat& status) const noexcept override { return status.st_mtimespec; }

    std::vector<std::string> FontDirectoryCandidates() const override {
        std::vector<std::string> candidates;
        candidates = {"/System/Library/Fonts", "/Library/Fonts"};
        if (const char* home = Home()) candidates.push_back(std::string(home) + "/Library/Fonts");
        return candidates;
    }

    bool CaseSensitivePaths() const noexcept override { return false; }

    Base::Result<std::string, OSError> NativeExecutablePath() const override {
        std::vector<char> buffer(1024);
        auto size = static_cast<std::uint32_t>(buffer.size());
        if (::_NSGetExecutablePath(buffer.data(), &size) != 0) {
            // size now holds the length needed.
            buffer.resize(size);
            if (::_NSGetExecutablePath(buffer.data(), &size) != 0) {
                return Base::Err(OSError::Make(OSErrorCode::IOFailed, "cannot locate the executable"));
            }
        }
        const std::unique_ptr<char, decltype(&std::free)> resolved(::realpath(buffer.data(), nullptr), &std::free);
        if (!resolved) {
            const int error = errno;
            return Base::Err(MakeOSError(OSErrorCode::IOFailed, "cannot resolve the executable path",
                {NativeErrorDomain::Errno, error}));
        }
        return std::string(resolved.get());
    }

    Base::Result<std::string, OSError> UserDataBase() const override {
        const char* home = Home();
        if (home == nullptr) {
            return Base::Err(OSError::Make(OSErrorCode::IOFailed,
                "cannot name the user data directory: HOME is not an absolute path"));
        }
        return std::string(WithoutTrailingSlashes(home)) + "/Library/Application Support";
    }
};

} // namespace

const Platform& CurrentPlatform() noexcept {
    static const MacOSPlatform platform;
    return platform;
}

} // namespace GYO::Engine::OS::Detail
