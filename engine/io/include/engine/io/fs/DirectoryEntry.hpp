#pragma once

#include <cstdint>
#include <string>


#include "engine/io/fs/FileInfo.hpp" // FileType, FileInfo

namespace Engine::IO::FS {

    /// ディレクトリ列挙で返す1件
    /// - VFS を想定して “path は URI 文字列” で持つ（表示/UI用に name も別で持つ）
    struct DirectoryEntry final {
        std::string path;   // 例: "assets://textures/a.png" / "file://C:/..."
        std::string name;   // 例: "a.png"
        FileType type = FileType::None;

        // includeInfo=true のときのみ有効（Stat を伴う列挙）
        FileInfo info;
        bool hasInfo = false;

        constexpr bool IsFile() const noexcept { return type == FileType::Regular; }
        constexpr bool IsDirectory() const noexcept { return type == FileType::Directory; }
        constexpr bool IsSymlink() const noexcept { return type == FileType::Symlink; }
    };

    /// 列挙の挙動指定（List/Iterator 共通）
    struct ListOptions final {
        bool recursive = false;
        bool includeFiles = true;
        bool includeDirectories = true;
        bool includeHidden = false;     // hidden 判定は backend 側で吸収
        bool followSymlinks = false;    // 必要なら
        bool includeInfo = false;       // 列挙と同時に Stat を取る（重いのでデフォルト false）
    };

} // namespace Engine::IO::FS
