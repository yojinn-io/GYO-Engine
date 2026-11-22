// The macOS steps of engine/os's shared implementation (sys/Platform.hpp).
#include "../Platform.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <mach-o/dyld.h>

namespace GYO::Engine::OS::Detail {
namespace {

class MacOsPlatform final : public Platform {
public:
    const timespec& ModifiedTimespec(const struct stat& status) const noexcept override { return status.st_mtimespec; }

    std::vector<std::string> FontDirectoryCandidates() const override {
        std::vector<std::string> candidates;
        candidates = {"/System/Library/Fonts", "/Library/Fonts"};
        if (const char* home = std::getenv("HOME"); home != nullptr && home[0] == '/') {
            candidates.push_back(std::string(home) + "/Library/Fonts");
        }
        return candidates;
    }

    bool CaseSensitivePaths() const noexcept override { return false; }

    Base::Result<std::string, OsError> NativeExecutablePath() const override {
        std::vector<char> buffer(1024);
        auto size = static_cast<std::uint32_t>(buffer.size());
        if (::_NSGetExecutablePath(buffer.data(), &size) != 0) {
            // size now holds the length needed.
            buffer.resize(size);
            if (::_NSGetExecutablePath(buffer.data(), &size) != 0) {
                return Base::Err(OsError::Make(OsErrorCode::IoFailed, "cannot locate the executable"));
            }
        }
        const std::unique_ptr<char, decltype(&std::free)> resolved(::realpath(buffer.data(), nullptr), &std::free);
        if (!resolved) {
            const int error = errno;
            return Base::Err(MakeOsError(OsErrorCode::IoFailed, "cannot resolve the executable path",
                {NativeErrorDomain::Errno, error}));
        }
        return std::string(resolved.get());
    }
};

} // namespace

const Platform& CurrentPlatform() noexcept {
    static const MacOsPlatform platform;
    return platform;
}

} // namespace GYO::Engine::OS::Detail
