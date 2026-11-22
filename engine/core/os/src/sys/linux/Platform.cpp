// The Linux steps of engine/os's shared implementation (sys/Platform.hpp).
#include "../Platform.hpp"
#include "../../common/Paths.hpp"

#include <cerrno>
#include <cstdlib>
#include <string>
#include <vector>

#include <unistd.h>

namespace GYO::Engine::OS::Detail {
namespace {

// The XDG data home is $XDG_DATA_HOME when it is an absolute path (the
// specification ignores a relative one), else .local/share in $HOME when HOME
// is absolute. DataHome gives the first and Home the second's HOME; each is
// null when its variable is not set to an absolute path.
const char* DataHome() noexcept {
    const char* data = std::getenv("XDG_DATA_HOME");
    return data != nullptr && data[0] == '/' ? data : nullptr;
}
const char* Home() noexcept {
    const char* home = std::getenv("HOME");
    return home != nullptr && home[0] == '/' ? home : nullptr;
}

class LinuxPlatform final : public Platform {
public:
    const timespec& ModifiedTimespec(const struct stat& status) const noexcept override { return status.st_mtim; }

    std::vector<std::string> FontDirectoryCandidates() const override {
        std::vector<std::string> candidates;
        candidates = {"/usr/share/fonts", "/usr/local/share/fonts"};
        if (const char* data = DataHome()) {
            candidates.push_back(std::string(data) + "/fonts");
        } else if (const char* home = Home()) {
            candidates.push_back(std::string(home) + "/.local/share/fonts");
        }
        return candidates;
    }

    bool CaseSensitivePaths() const noexcept override { return true; }

    Base::Result<std::string, OSError> NativeExecutablePath() const override {
        std::vector<char> buffer(1024);
        for (;;) {
            const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
            if (length < 0) {
                const int error = errno;
                return Base::Err(MakeOSError(OSErrorCode::IOFailed, "cannot locate the executable",
                    {NativeErrorDomain::Errno, error}));
            }
            if (static_cast<std::size_t>(length) < buffer.size()) {
                return std::string(buffer.data(), static_cast<std::size_t>(length));
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    Base::Result<std::string, OSError> UserDataBase() const override {
        if (const char* data = DataHome()) return std::string(data);
        if (const char* home = Home()) return std::string(WithoutTrailingSlashes(home)) + "/.local/share";
        return Base::Err(OSError::Make(OSErrorCode::IOFailed,
            "cannot name the user data directory: neither XDG_DATA_HOME nor HOME is an absolute path"));
    }
};

} // namespace

const Platform& CurrentPlatform() noexcept {
    static const LinuxPlatform platform;
    return platform;
}

} // namespace GYO::Engine::OS::Detail
