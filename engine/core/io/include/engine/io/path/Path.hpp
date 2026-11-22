#pragma once

#include <string>
#include <string_view>

#include "engine/base/Result.hpp"
#include "engine/io/path/PathUtils.hpp"

namespace GYO::Engine::IO::Path {

    // A path in '/' form, normalized when it is made.
    class Path final {
    public:
        Path() = default;

        // From a string already normalized (trusted input only).
        static Path FromNormalized(std::string normalized) {
            Path p;
            p.normalized_ = std::move(normalized);
            return p;
        }

        // From raw input, by Normalize's rules.
        static Base::Result<Path, IOError> Parse(std::string_view raw, const NormalizeOptions& opt = {}) {
            auto n = Normalize(raw, opt);
            if (!n) {
                return Base::Err(std::move(n.error()));
            }
            return Path::FromNormalized(std::move(n.value()));
        }

        const std::string& Str() const noexcept { return normalized_; }
        bool Empty() const noexcept { return normalized_.empty(); }
        Path Parent() const { return Path::FromNormalized(IO::Path::Parent(normalized_)); }

        friend bool operator==(const Path&, const Path&) = default;

    private:
        std::string normalized_;
    };

} // namespace GYO::Engine::IO::Path
