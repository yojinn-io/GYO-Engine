#pragma once

#include <string>
#include <string_view>

#include "engine/base/Result.hpp"
#include "engine/io/IOError.hpp"

namespace GYO::Engine::IO::Path {

    struct NormalizeOptions {
        bool convertBackslash     = true;  // "\" -> "/"
        bool collapseSlashes      = true;  // "a//b" -> "a/b"
        bool removeDot            = true;  // "./a"  -> "a"
        bool resolveDotDot        = true;  // "a/../b" -> "b" (climbing above the root fails)
        bool rejectAbsoluteLike   = true;  // refuses "/a", "C:", "\\server"
        bool rejectTraversal      = true;  // refuses ".." (also when resolveDotDot is false)
        bool rejectNullByte       = true;  // refuses '\0'
        bool keepTrailingSlash    = false; // keeps a trailing "/"
    };

    // A logical path in '/' form, by the options' lexical rules.
    Base::Result<std::string, IOError> Normalize(std::string_view raw, const NormalizeOptions& opt = {});

    // The path without its last segment ("a/b" -> "a"); "" when there is none.
    std::string Parent(std::string_view s);

    // Resolves "." and ".." lexically, keeping a drive, UNC or POSIX root prefix. escapedAboveRoot is set
    // when a ".." would climb above the start.
    std::string RemoveDotSegments(std::string_view path, bool& escapedAboveRoot);

} // namespace GYO::Engine::IO::Path
