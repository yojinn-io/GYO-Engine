#pragma once

#include <cstdint>
#include <string>

namespace Engine::IO::FS {

    /// ファイル種別（OS差は platform 側で吸収）
    enum class FileType : std::uint8_t {
        None = 0,    // 存在しない
        Regular,     // 通常ファイル
        Directory,   // ディレクトリ
        Symlink,     // シンボリックリンク
        Other        // ソケット等（必要なら）
    };

    inline constexpr const char* ToString(FileType t) noexcept {
        switch (t) {
        case FileType::None:      return "None";
        case FileType::Regular:   return "Regular";
        case FileType::Directory: return "Directory";
        case FileType::Symlink:   return "Symlink";
        case FileType::Other:     return "Other";
        default:                  return "Unknown";
        }
    }

    /// UNIX epoch を基準にした時間（ns）
    /// - OS実装で取れない場合は 0 を許容
    using TimeNs = std::int64_t;

    /// ファイル情報（Stat の戻り）
    struct FileInfo final {
        FileType type = FileType::None;

        // Regular のときのみ意味がある（Directory などは 0 でOK）
        std::uint64_t sizeBytes = 0;

        // 取れない/不要なら 0
        TimeNs mtimeNs = 0; // 最終更新
        TimeNs ctimeNs = 0; // メタデータ変更（OSによって意味が違う）
        TimeNs atimeNs = 0; // 最終アクセス（取れないOSもある）

        // 取れないOS/実装なら 0（UNIX permission 風の 0xxx を入れるなど）
        std::uint32_t permissions = 0;

        // 任意補助（VFSやpack等で “物理ソース” を持つなら埋める）
        // 例: "file", "pak", "http"
        std::string backend;

        constexpr bool Exists() const noexcept { return type != FileType::None; }
        constexpr bool IsFile() const noexcept { return type == FileType::Regular; }
        constexpr bool IsDirectory() const noexcept { return type == FileType::Directory; }
        constexpr bool IsSymlink() const noexcept { return type == FileType::Symlink; }
    };

    /// Remove の挙動指定
    struct RemoveOptions final {
        bool recursive = false; // ディレクトリ削除を許可するか
    };

} // namespace Engine::IO::FS
