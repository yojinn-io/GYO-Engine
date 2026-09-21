#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "engine/io/IoFwd.hpp"          // Engine::IO::Path::Uri fwd
#include "engine/io/fs/IFileSystem.hpp" // FS::IFileSystem
#include "engine/io/path/Uri.hpp"
#include "engine/io/stream/FileOpenMode.hpp"

namespace Engine::IO::FS {

    /// VFS のマウント単位
    /// - scheme（例: assets://）単位でルーティングする
    /// - priority が高いものが優先（overlay 用）
    /// - root は下位 FS に渡す “基準 URI”（例: file:///.../Project/assets）
    struct MountPoint final {
        std::string name; // 任意識別子（"assets_main", "assets_mod" など）
        std::int32_t priority = 0;
        bool readOnly = false;

        // 例: "assets://" / "user://" のような論理スキーム
        Engine::IO::Path::Uri mountUri;

        // 下位 FS の root（例: file:///.../assets , pak://base.pak#assets）
        Engine::IO::Path::Uri rootUri;

        // 実体
        std::shared_ptr<Engine::IO::FS::IFileSystem> fs;

        // オプション: 書き込み先として優先したい場合
        bool preferWrite = false;

        MountPoint() = default;
    };

} // namespace Engine::IO::VFS
