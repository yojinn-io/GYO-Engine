#include "gyo/framework/AbsolutePath.hpp"

#include "engine/os/Files.hpp"

#include <utility>

// Path queries go through engine/os (docs/architecture/os.md, Design Invariant 2);
// the framework holds no OS code.

namespace Gyo::Tools::Framework {

std::filesystem::path AbsolutePath(const std::filesystem::path& path) {
    namespace OsFiles = Engine::Os::Files;
    auto text = OsFiles::PathToUtf8(path);
    if (!text || text.value().empty()) {
        return path.lexically_normal();
    }
    auto absolute = OsFiles::MakeAbsolute(text.value());
    if (!absolute) {
        return path.lexically_normal();
    }
    auto converted = OsFiles::PathFromUtf8(absolute.value());
    return (converted ? std::move(converted.value()) : path).lexically_normal();
}

} // namespace Gyo::Tools::Framework
