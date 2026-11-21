#pragma once
#include <cstdint>

namespace Engine::IO::Stream {

    /// ファイル/ストリームの Open フラグ
    /// - OS 依存の詳細は platform 側で吸収する
    /// - エンジン内部は基本 Binary（bytes）運用を推奨
    enum class FileOpenMode : std::uint32_t {
        None   = 0,

        // access
        Read   = 1u << 0,
        Write  = 1u << 1,

        // behavior
        Append = 1u << 2,  // Write との組み合わせを想定（末尾追記）
        CreateIfMissing = 1u << 3,
        Truncate        = 1u << 4,

        // hint
        Binary = 1u << 5,  // 基本ON推奨
        Text   = 1u << 6,  // tools 用のヒント（改行変換等を OS に任せたい場合）
    };

    // --- bit ops ---
    inline constexpr FileOpenMode operator|(FileOpenMode a, FileOpenMode b) noexcept {
        return static_cast<FileOpenMode>(
            static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b)
        );
    }
    inline constexpr FileOpenMode operator&(FileOpenMode a, FileOpenMode b) noexcept {
        return static_cast<FileOpenMode>(
            static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b)
        );
    }
    inline constexpr FileOpenMode operator^(FileOpenMode a, FileOpenMode b) noexcept {
        return static_cast<FileOpenMode>(
            static_cast<std::uint32_t>(a) ^ static_cast<std::uint32_t>(b)
        );
    }
    inline constexpr FileOpenMode operator~(FileOpenMode a) noexcept {
        return static_cast<FileOpenMode>(~static_cast<std::uint32_t>(a));
    }
    inline constexpr FileOpenMode& operator|=(FileOpenMode& a, FileOpenMode b) noexcept {
        a = a | b;
        return a;
    }
    inline constexpr FileOpenMode& operator&=(FileOpenMode& a, FileOpenMode b) noexcept {
        a = a & b;
        return a;
    }
    inline constexpr bool Has(FileOpenMode v, FileOpenMode f) noexcept {
        return (static_cast<std::uint32_t>(v) & static_cast<std::uint32_t>(f)) != 0;
    }

    // --- helpers (呼び出し側の分岐を単純化) ---
    inline constexpr bool CanRead(FileOpenMode m) noexcept  { return Has(m, FileOpenMode::Read); }
    inline constexpr bool CanWrite(FileOpenMode m) noexcept { return Has(m, FileOpenMode::Write); }
    inline constexpr bool IsAppend(FileOpenMode m) noexcept { return Has(m, FileOpenMode::Append); }

    /// 代表的なプリセット（エンジン側で統一しやすい）
    inline constexpr FileOpenMode OpenReadBinary() noexcept {
        return FileOpenMode::Read | FileOpenMode::Binary;
    }
    inline constexpr FileOpenMode OpenWriteBinaryTruncate(bool createIfMissing = true) noexcept {
        auto m = FileOpenMode::Write | FileOpenMode::Binary | FileOpenMode::Truncate;
        if (createIfMissing) m |= FileOpenMode::CreateIfMissing;
        return m;
    }
    inline constexpr FileOpenMode OpenWriteBinaryAppend(bool createIfMissing = true) noexcept {
        auto m = FileOpenMode::Write | FileOpenMode::Binary | FileOpenMode::Append;
        if (createIfMissing) m |= FileOpenMode::CreateIfMissing;
        return m;
    }

    /// “明らかに矛盾した指定” を事前チェックするための簡易バリデータ
    /// - 実際の可否（OS/FS都合）は platform 実装で最終判断
    inline constexpr bool IsValid(FileOpenMode m) noexcept {
        // access がないのは無効
        if (!CanRead(m) && !CanWrite(m)) return false;

        // Append と Truncate は同時に使わない（意味が衝突）
        if (Has(m, FileOpenMode::Append) && Has(m, FileOpenMode::Truncate)) return false;

        // Text と Binary は同時指定しない（ヒントが衝突）
        if (Has(m, FileOpenMode::Text) && Has(m, FileOpenMode::Binary)) return false;

        // Append は Write 前提（Read+Append だけ、は基本想定しない）
        if (Has(m, FileOpenMode::Append) && !CanWrite(m)) return false;

        return true;
    }

} // namespace Engine::IO::Stream
