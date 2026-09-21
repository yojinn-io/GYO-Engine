#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/io/IoError.hpp"
#include "engine/io/path/Uri.hpp"

namespace Engine::IO::FS {

    using IoError = Engine::Base::Error<Engine::IO::IoErrorCode>;
    template<class T>
    using IoResult = Engine::Base::Result<T, IoError>;
    using IoResultVoid = Engine::Base::Result<void, IoError>;

    using WatchId = std::uint64_t;

    enum class FileChangeKind : std::uint8_t {
        Unknown = 0,
        Created,
        Modified,
        Removed,
        Renamed
    };

    struct FileChangeEvent final {
        FileChangeKind kind = FileChangeKind::Unknown;

        // 監視対象の path
        Engine::IO::Path::Uri path;

        // Renamed のときのみ oldPath が有効
        Engine::IO::Path::Uri oldPath;
        bool hasOldPath = false;

        // backend 名（デバッグ用）
        std::string backend;
    };

    struct WatchOptions final {
        bool recursive = false;
        bool watchFiles = true;
        bool watchDirectories = true;

        // 実装ヒント：まとめ／遅延
        bool coalesce = true;                 // 近接イベントをまとめる
        std::uint32_t debounceMs = 50;        // 0=無効（実装が可能なら）
    };

    /// 変更監視の抽象（任意）
    class IFileWatcher {
    public:
        virtual ~IFileWatcher() = default;

        virtual const char* Name() const noexcept = 0;
        virtual bool IsOpen() const noexcept = 0;

        /// 監視を追加
        virtual IoResult<WatchId> AddWatch(const Engine::IO::Path::Uri& uri,
                                          const WatchOptions& opt = {}) = 0;

        /// 監視を削除
        virtual IoResultVoid RemoveWatch(WatchId id) = 0;

        /// イベントを回収（非ブロッキング想定）
        /// - outEvents に追記する（呼び出し側で clear してもOK）
        /// - maxEvents は安全のための上限
        virtual IoResult<std::size_t> Poll(std::vector<FileChangeEvent>& outEvents,
                                          std::size_t maxEvents = 256) = 0;

        /// OS バッファ等をフラッシュ（任意）
        virtual IoResultVoid Flush() {
            return IoResultVoid::Ok();
        }

        virtual IoResultVoid Close() = 0;

        IFileWatcher(const IFileWatcher&) = delete;
        IFileWatcher& operator=(const IFileWatcher&) = delete;

    protected:
        IFileWatcher() = default;
    };

} // namespace Engine::IO::FS
