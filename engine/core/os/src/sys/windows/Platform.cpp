// Windows implements the file, process and console primitives itself
// (sys/windows), so the steps of the shared implementation are empty here
// (sys/Platform.hpp). Nothing on Windows calls them.
#include "../Platform.hpp"

namespace Engine::Os::Detail {
namespace {

class WindowsPlatform final : public Platform {
public:
    const timespec& ModifiedTimespec(const struct stat&) const noexcept override {
        static const timespec none{};
        return none;
    }
    std::vector<std::string> FontDirectoryCandidates() const override { return {}; }
    bool CaseSensitivePaths() const noexcept override { return false; }
    Base::Result<std::string, OsError> NativeExecutablePath() const override {
        return Base::Err(OsError::Make(OsErrorCode::Unsupported, "not used on Windows"));
    }
};

} // namespace

const Platform& CurrentPlatform() noexcept {
    static const WindowsPlatform platform;
    return platform;
}

} // namespace Engine::Os::Detail
