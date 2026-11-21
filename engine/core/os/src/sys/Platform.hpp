#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OsError.hpp"

#include <string>
#include <vector>

#include <sys/stat.h>
#include <time.h>

// The steps of engine/os's shared implementation (sys/Files.cpp, Process.cpp,
// Console.cpp) that differ between platforms. The shared implementation calls
// only these; each platform implements them in sys/<platform>/Platform.cpp, and
// the build links the one platform it targets. Windows implements the file,
// process and console primitives itself (sys/windows) and leaves these empty.
namespace Engine::Os::Detail {

class Platform {
public:
    Platform() = default;
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;
    virtual ~Platform() = default;

    // Files: the modification time in a stat result.
    [[nodiscard]] virtual const timespec& ModifiedTimespec(const struct stat& status) const noexcept = 0;
    // Files: the directories that may hold fonts, before existence checks.
    [[nodiscard]] virtual std::vector<std::string> FontDirectoryCandidates() const = 0;
    // Files: whether paths compare case-sensitively by default.
    [[nodiscard]] virtual bool CaseSensitivePaths() const noexcept = 0;
    // Process: the executable's path as the OS reports it (not yet checked for UTF-8).
    [[nodiscard]] virtual Base::Result<std::string, OsError> NativeExecutablePath() const = 0;
};

// The platform this build targets.
[[nodiscard]] const Platform& CurrentPlatform() noexcept;

} // namespace Engine::Os::Detail
