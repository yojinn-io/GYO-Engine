#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OSError.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <sys/stat.h>
#include <time.h>

// The steps of engine/os's shared implementation that differ between
// platforms. sys/Files.cpp, Process.cpp and Console.cpp are shared by macOS and
// Linux; sys/UserData.cpp is shared by all three. The shared implementation
// calls only these; each platform implements them in
// sys/<platform>/Platform.cpp, and the build links the one platform it targets.
// Windows implements the file, process and console primitives itself
// (sys/windows), so it leaves their steps empty and implements only
// UserDataBase.
namespace GYO::Engine::OS::Detail {

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
    [[nodiscard]] virtual Base::Result<std::string, OSError> NativeExecutablePath() const = 0;
    // Files: the directory every application's user data directory is made in,
    // absolute, in '/' form (not yet checked for UTF-8); IOFailed when the
    // platform cannot name one (sys/UserData.cpp).
    [[nodiscard]] virtual Base::Result<std::string, OSError> UserDataBase() const = 0;
};

// Files::CreateDirectories, with the access of the directories it makes. Unlike
// the steps above it is a function, implemented next to CreateDirectories in
// sys/Files.cpp and sys/windows/Files.cpp. OwnerOnly makes them 0700 on POSIX;
// Windows ignores it, since a new directory inherits its parent's ACL.
enum class NewDirectoryAccess : std::uint8_t { Default, OwnerOnly };
[[nodiscard]] Base::Result<void, OSError> CreateDirectories(std::string_view utf8Path, NewDirectoryAccess access);

// The platform this build targets.
[[nodiscard]] const Platform& CurrentPlatform() noexcept;

} // namespace GYO::Engine::OS::Detail
