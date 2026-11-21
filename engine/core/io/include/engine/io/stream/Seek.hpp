#pragma once
#include <cstdint>

namespace Engine::IO::Stream {

    /// Seek の基準位置
    enum class SeekWhence : std::uint8_t {
        Begin,   // 先頭から
        Current, // 現在位置から
        End      // 末尾から
    };

    /// Seek 指定をまとめる値オブジェクト（API読みやすさ用）
    struct Seek final {
        std::int64_t offset = 0;
        SeekWhence whence = SeekWhence::Begin;

        static constexpr Seek FromBegin(std::uint64_t pos) noexcept {
            return Seek{ static_cast<std::int64_t>(pos), SeekWhence::Begin };
        }
        static constexpr Seek FromCurrent(std::int64_t delta) noexcept {
            return Seek{ delta, SeekWhence::Current };
        }
        static constexpr Seek FromEnd(std::int64_t delta) noexcept {
            return Seek{ delta, SeekWhence::End };
        }
    };

} // namespace Engine::IO::Stream
