#pragma once

#include <cstdint>

namespace Engine::IO::FS {

    struct FileSystemCapabilities final {
        // 基本機能
        bool canOpenRead  = true;
        bool canOpenWrite = true;
        bool canList      = true;
        bool canIterate   = false; // DirectoryIterator を提供できるか
        bool canStat      = true;

        // 追加操作
        bool canCreateDirectories = true;
        bool canRemove            = true;
        bool canRemoveRecursive   = true;
        bool canMove              = true;
        bool canCopy              = true;

        // 特性
        bool supportsSymlink       = false;
        bool supportsPermissions   = false;
        bool supportsHiddenFlag    = false; // hidden 判定が可能か
        bool caseSensitivePaths    = false; // Windows=false, Linux=true のような差異
        bool supportsToNativePath  = false; // Uri -> OS パス文字列へ変換できるか

        // 時刻
        bool supportsMtime = true;
        bool supportsCtime = false;
        bool supportsAtime = false;

        // 監視
        bool supportsWatch = false;
        bool supportsRecursiveWatch = false;

        // 制限（不明なら 0）
        std::uint32_t maxPathBytes = 0;
        std::uint32_t maxNameBytes = 0;
    };

} // namespace Engine::IO::FS
