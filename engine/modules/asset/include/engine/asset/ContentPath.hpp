#pragma once

#include <optional>
#include <string>
#include <string_view>

// Paths under the content root (declaration 5.3 (1)). A content path is the
// text a catalog writes, compared byte for byte and independent of the host
// file system's rules.

namespace GYO::Engine::Asset {

// The name of the entry file at the content root. No catalog or asset path may
// be it (rules (A) 1 and 5 of 5.3).
inline constexpr std::string_view kContentManifestName = "content.json";

// A relative path under the content root: not empty, '/' separated, no empty,
// "." or ".." segment (so no leading, trailing or repeated '/'), and no '\\',
// ':' or control character (bytes below 0x20).
[[nodiscard]] bool IsRelativeContentPath(std::string_view path) noexcept;

// A file under the content root: contentRoot (a native path in UTF-8) and a
// relative content path, joined with one '/' (none added when the root already
// ends with '/' or '\\'). An empty root gives the relative path as it is. The
// catalog reader (content.json, the catalogs) and the store (an entry's file)
// both name files this way.
[[nodiscard]] std::string JoinContentRoot(std::string_view contentRoot, std::string_view relative);

// The extension of a path: what follows the last '.' of its last segment,
// lowercased (ASCII). Empty when the last segment has no '.' or ends with one;
// a '.' in a directory name does not count (rule (A) 6 of 5.3).
[[nodiscard]] std::string PathExtension(std::string_view path);

// A path a loader read from a file, resolved against the directory of the file
// that names it (parentPath, a content path) and normalized: "." segments go,
// ".." takes back one segment. Empty when the reference is absolute (leading
// '/'), has a '\\', goes above the content root with "..", or the result is not
// a relative content path; LoadContext reports that as BadReference
// (declaration 5.17). Case is kept.
[[nodiscard]] std::optional<std::string> ResolveReferencePath(std::string_view parentPath,
                                                              std::string_view reference);

} // namespace GYO::Engine::Asset
