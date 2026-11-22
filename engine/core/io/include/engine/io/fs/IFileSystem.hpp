#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include "engine/io/IOError.hpp"
#include "engine/io/path/Uri.hpp"
#include "engine/io/fs/FileInfo.hpp"
#include "engine/io/stream/FileOpenMode.hpp"
#include "engine/io/stream/IStream.hpp"

#include "engine/io/fs/FileSystemCapabilities.hpp"
#include "engine/io/fs/DirectoryIterator.hpp"
#include "engine/io/fs/IFileWatcher.hpp"

namespace GYO::Engine::IO::FS {




    /// IFileSystem：OS/バックエンド差異を隠す最小抽象
    /// - file:// (ネイティブ) / pak:// / memory:// / http:// 等の実装差を platform 側で吸収
    /// - ここは「契約」。ポリシー（assetsRoot制約やPath正規化）は上位（Resolver/VFS）でも良い
    class IFileSystem {
    public:
        virtual ~IFileSystem() = default;

        /// backend 名（例："NativeFS", "PakFS"）
        virtual const char* Name() const noexcept = 0;

        /// Open：uri でストリームを開く
        /// - mode は FileOpenMode（Read/Write/Append/Create/Truncate/Binary...）
        /// - 戻り値は IStream（読み書き・seek は stream 側の caps に従う）
        virtual IOResult<std::unique_ptr<Stream::IStream>>
        Open(const Path::Uri& uri, Stream::FileOpenMode mode) = 0;

        /// Exists：存在確認
        virtual IOResult<bool> Exists(const Path::Uri& uri) = 0;

        /// Stat：ファイル情報取得（存在しない場合は type=None を返すか、NotFound を返すかは実装方針）
        /// 推奨：存在しない場合は Err(NotFound)
        virtual IOResult<FileInfo> Stat(const Path::Uri& uri) = 0;

        /// CreateDirectories：親を含めて作る（mkdir -p）
        virtual IOResult<void> CreateDirectories(const Path::Uri& uri) = 0;

        /// Remove：ファイル/ディレクトリ削除
        virtual IOResult<void> Remove(const Path::Uri& uri, const RemoveOptions& opt = {}) = 0;

        /// Move/Rename：同一 backend 内の移動
        virtual IOResult<void> Move(const Path::Uri& from, const Path::Uri& to) = 0;

        /// Copy：必要なら（大きいのでデフォルトで入れない方針でもOK）
        virtual IOResult<void> Copy(const Path::Uri& from, const Path::Uri& to) = 0;

        /// List：ディレクトリ列挙
        virtual IOResult<std::vector<DirectoryEntry>>
        List(const Path::Uri& uri, const ListOptions& opt = {}) = 0;

        /// 可能なら絶対化/正規化など（実装が提供できる範囲で）
        /// - PakFS などは “物理パス” ではないので NotSupported でOK
        virtual IOResult<std::string> ToNativePathString(const Path::Uri& uri) = 0;

        IFileSystem(const IFileSystem&) = delete;
        IFileSystem& operator=(const IFileSystem&) = delete;

        virtual FileSystemCapabilities Capabilities() const noexcept = 0;

        virtual IOResult<std::unique_ptr<DirectoryIterator>> Iterate(const Path::Uri& uri, const ListOptions& opt = {}) = 0;

        virtual IOResult<std::unique_ptr<IFileWatcher>> CreateWatcher() = 0;

    protected:
        IFileSystem() = default;
    };

} // namespace GYO::Engine::IO::FS
