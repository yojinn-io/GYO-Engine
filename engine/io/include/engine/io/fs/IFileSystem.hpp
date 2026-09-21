#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include "engine/io/IoError.hpp"
#include "engine/io/path/Uri.hpp"
#include "engine/io/fs/FileInfo.hpp"
#include "engine/io/stream/FileOpenMode.hpp"
#include "engine/io/stream/IStream.hpp"

#include "engine/io/fs/FileSystemCapabilities.hpp"
#include "engine/io/fs/DirectoryIterator.hpp"
#include "engine/io/fs/IFileWatcher.hpp"

namespace Engine::IO::FS {

    using IoError = Engine::Base::Error<Engine::IO::IoErrorCode>;

    template<class T>
    using IoResult = Engine::Base::Result<T, IoError>;

    using IoResultVoid = Engine::Base::Result<void, IoError>;

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
        virtual IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>
        Open(const Engine::IO::Path::Uri& uri, Engine::IO::Stream::FileOpenMode mode) = 0;

        /// Exists：存在確認
        virtual IoResult<bool> Exists(const Engine::IO::Path::Uri& uri) = 0;

        /// Stat：ファイル情報取得（存在しない場合は type=None を返すか、NotFound を返すかは実装方針）
        /// 推奨：存在しない場合は Err(NotFound)
        virtual IoResult<FileInfo> Stat(const Engine::IO::Path::Uri& uri) = 0;

        /// CreateDirectories：親を含めて作る（mkdir -p）
        virtual IoResultVoid CreateDirectories(const Engine::IO::Path::Uri& uri) = 0;

        /// Remove：ファイル/ディレクトリ削除
        virtual IoResultVoid Remove(const Engine::IO::Path::Uri& uri, const RemoveOptions& opt = {}) = 0;

        /// Move/Rename：同一 backend 内の移動
        virtual IoResultVoid Move(const Engine::IO::Path::Uri& from, const Engine::IO::Path::Uri& to) = 0;

        /// Copy：必要なら（大きいのでデフォルトで入れない方針でもOK）
        virtual IoResultVoid Copy(const Engine::IO::Path::Uri& from, const Engine::IO::Path::Uri& to) = 0;

        /// List：ディレクトリ列挙
        virtual IoResult<std::vector<DirectoryEntry>>
        List(const Engine::IO::Path::Uri& uri, const ListOptions& opt = {}) = 0;

        /// 可能なら絶対化/正規化など（実装が提供できる範囲で）
        /// - PakFS などは “物理パス” ではないので NotSupported でOK
        virtual IoResult<std::string> ToNativePathString(const Engine::IO::Path::Uri& uri) = 0;

        IFileSystem(const IFileSystem&) = delete;
        IFileSystem& operator=(const IFileSystem&) = delete;

        virtual FileSystemCapabilities Capabilities() const noexcept = 0;

        virtual IoResult<std::unique_ptr<DirectoryIterator>> Iterate(const Engine::IO::Path::Uri& uri, const ListOptions& opt = {}) = 0;

        virtual IoResult<std::unique_ptr<IFileWatcher>> CreateWatcher() = 0;

    protected:
        IFileSystem() = default;
    };

} // namespace Engine::IO::FS
