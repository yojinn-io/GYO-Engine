#pragma once

#include <string>
#include <string_view>

#include "engine/base/Result.hpp"
#include "engine/io/path/PathUtils.hpp"

namespace Engine::IO::Path {
    using IoError = Base::Error<IoErrorCode>;

    class Path final {
    public:
        Path() = default;

        // 正規化済みから生成（内部用・信頼できる入力のみ）
        static Path FromNormalized(std::string normalized) {
            Path p;
            p.normalized_ = std::move(normalized);
            return p;
        }

        // 生入力から安全に生成
        static Base::Result<Path, IoError> Parse(std::string_view raw,
                                         const NormalizeOptions& opt = {}) {
            auto n = Normalize(raw, opt);
            if (!n) {
                return Base::Result<Path, IoError>::Err(std::move(n.error()));
            }
            return Base::Result<Path, IoError>::Ok(Path::FromNormalized(std::move(n.value())));
        }

        const std::string& Str() const noexcept { return normalized_; }
        bool Empty() const noexcept { return normalized_.empty(); }

        // 判定（論理パス観点）
        bool IsAbsoluteLike() const noexcept { return ::Engine::IO::Path::IsAbsoluteLike(normalized_); }
        bool HasTraversal() const noexcept { return ::Engine::IO::Path::ContainsTraversal(normalized_); }
        bool HasNullByte() const noexcept { return ::Engine::IO::Path::ContainsNullByte(normalized_); }

        std::string_view Filename() const noexcept { return ::Engine::IO::Path::Filename(normalized_); }
        std::string_view Extension() const noexcept { return ::Engine::IO::Path::Extension(normalized_); }

        Path Parent() const { return Path::FromNormalized(::Engine::IO::Path::Parent(normalized_)); }

        // すでに正規化済み同士を結合（relative は相対であることを期待）
        Path Join(const Path& relative) const {
            return Path::FromNormalized(::Engine::IO::Path::Join(normalized_, relative.normalized_));
        }

        friend Path operator/(const Path& a, const Path& b) { return a.Join(b); }
        friend bool operator==(const Path&, const Path&) = default;

    private:
        std::string normalized_; // 常に "/" 区切りの論理パス
    };

} // namespace Engine::IO::Path
