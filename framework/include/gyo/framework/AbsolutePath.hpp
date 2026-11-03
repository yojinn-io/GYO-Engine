#pragma once

#include <filesystem>

namespace Gyo::Tools::Framework {

// The absolute, lexically normal form of path, through engine/os. Unlike
// std::filesystem::absolute it never throws: if the current directory is
// unavailable the path is kept as given, and the file operation that follows
// reports the failure. Content mounting and the tools' remembered paths use it
// (framework-rework D-2).
[[nodiscard]] std::filesystem::path AbsolutePath(const std::filesystem::path& path);

} // namespace Gyo::Tools::Framework
