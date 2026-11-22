// The Linux steps of engine/os's shared implementation (sys/Platform.hpp).
#include "../Platform.hpp"

#include <cerrno>
#include <cstdlib>
#include <string>
#include <vector>

#include <unistd.h>

namespace GYO::Engine::OS::Detail {
namespace {

class LinuxPlatform final : public Platform {
public:
    const timespec& ModifiedTimespec(const struct stat& status) const noexcept override { return status.st_mtim; }

    std::vector<std::string> FontDirectoryCandidates() const override {
        std::vector<std::string> candidates;
        candidates = {"/usr/share/fonts", "/usr/local/share/fonts"};
        if (const char* data = std::getenv("XDG_DATA_HOME"); data != nullptr && data[0] == '/') {
            candidates.push_back(std::string(data) + "/fonts");
        } else if (const char* home = std::getenv("HOME"); home != nullptr && home[0] == '/') {
            candidates.push_back(std::string(home) + "/.local/share/fonts");
        }
        return candidates;
    }

    bool CaseSensitivePaths() const noexcept override { return true; }

    Base::Result<std::string, OsError> NativeExecutablePath() const override {
        std::vector<char> buffer(1024);
        for (;;) {
            const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
            if (length < 0) {
                const int error = errno;
                return Base::Err(MakeOsError(OsErrorCode::IoFailed, "cannot locate the executable",
                    {NativeErrorDomain::Errno, error}));
            }
            if (static_cast<std::size_t>(length) < buffer.size()) {
                return std::string(buffer.data(), static_cast<std::size_t>(length));
            }
            buffer.resize(buffer.size() * 2);
        }
    }
};

} // namespace

const Platform& CurrentPlatform() noexcept {
    static const LinuxPlatform platform;
    return platform;
}

} // namespace GYO::Engine::OS::Detail
