// Files::UserDataDirectory on every platform (docs/architecture/os.md, Files;
// v8-arch-gyo2-os-user-data). Only the base directory differs between
// platforms: each Platform names it (sys/<platform>/Platform.cpp), and the
// names, the path and the directories are made here once.
#include "engine/os/Files.hpp"

#include "Platform.hpp"
#include "../common/Paths.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace GYO::Engine::OS::Files {

namespace {

// One path component (the user's decision Q1 (b)): not a dot segment and no
// separator of any platform. Empty is refused unless allowed. A NUL or bytes
// that are not UTF-8 are left to CreateDirectories, which checks the whole path.
Base::Result<void, OSError> CheckName(const std::string_view name, const bool mayBeEmpty) {
    if (name.empty()) {
        if (mayBeEmpty) return {};
        return Base::Err(Detail::RefusedArgument(OSErrorCode::InvalidPath, "application name is empty", 0));
    }
    if (name == "." || name == "..") {
        return Base::Err(Detail::RefusedArgument(OSErrorCode::InvalidPath, "name is a dot segment", name.size()));
    }
    if (name.find_first_of("/\\") != std::string_view::npos) {
        return Base::Err(
            Detail::RefusedArgument(OSErrorCode::InvalidPath, "name contains a path separator", name.size()));
    }
    return {};
}

} // namespace

Base::Result<std::string, OSError> UserDataDirectory(const std::string_view organization,
                                                     const std::string_view application) {
    if (auto checked = CheckName(application, false); !checked) return Base::Err(std::move(checked.error()));
    if (auto checked = CheckName(organization, true); !checked) return Base::Err(std::move(checked.error()));
    auto base = Detail::CurrentPlatform().UserDataBase();
    if (!base) return Base::Err(std::move(base.error()));
    // A base of "/" leaves "" here, so the result is "/<organization>/<application>".
    std::string path(Detail::WithoutTrailingSlashes(base.value()));
    if (!organization.empty()) path.append("/").append(organization);
    path.append("/").append(application);
    if (auto made = Detail::CreateDirectories(path, Detail::NewDirectoryAccess::OwnerOnly); !made) {
        return Base::Err(std::move(made.error()));
    }
    return path;
}

} // namespace GYO::Engine::OS::Files
