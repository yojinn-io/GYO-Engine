#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/io/IoError.hpp"
#include "engine/io/fs/DirectoryEntry.hpp"

namespace Engine::IO::FS {

    using IoError = Engine::Base::Error<Engine::IO::IoErrorCode>;
    template<class T>
    using IoResult = Engine::Base::Result<T, IoError>;
    using IoResultVoid = Engine::Base::Result<void, IoError>;

    /// ストリーミング列挙インターフェース（任意）
    class DirectoryIterator {
    public:
        virtual ~DirectoryIterator() = default;

        virtual const char* BackendName() const noexcept = 0;
        virtual bool IsOpen() const noexcept = 0;

        /// 次の要素を取得
        /// - Ok(true)  : out が埋まった（1件取得）
        /// - Ok(false) : 列挙終了
        /// - Err(...)  : エラー
        virtual IoResult<bool> Next(DirectoryEntry& out) = 0;

        /// 先頭に戻す（対応しない backend は NotSupported を返してOK）
        virtual IoResultVoid Reset() {
            return IoResultVoid::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported,
                "DirectoryIterator: reset not supported"));
        }

        /// 明示クローズ（OSハンドル解放）
        virtual IoResultVoid Close() = 0;

        DirectoryIterator(const DirectoryIterator&) = delete;
        DirectoryIterator& operator=(const DirectoryIterator&) = delete;

    protected:
        DirectoryIterator() = default;
    };

    /// --- 便利：vector を iterator として扱うアダプタ（engine 側） ---
    /// platform を作る前の暫定や、List() から Iter() を実装したい時に使える
    class VectorDirectoryIterator final : public DirectoryIterator {
    public:
        explicit VectorDirectoryIterator(std::vector<DirectoryEntry> entries,
                                         const char* backendName = "VectorIterator")
            : entries_(std::move(entries)), backend_(backendName) {}

        const char* BackendName() const noexcept override { return backend_; }
        bool IsOpen() const noexcept override { return open_; }

        IoResult<bool> Next(DirectoryEntry& out) override {
            if (!open_) {
                return IoResult<bool>::Err(IoError::Make(
                    Engine::IO::IoErrorCode::ReadFailed,
                    "VectorDirectoryIterator: next on closed iterator"));
            }
            if (index_ >= entries_.size()) return IoResult<bool>::Ok(false);
            out = entries_[index_++];
            return IoResult<bool>::Ok(true);
        }

        IoResultVoid Reset() override {
            if (!open_) {
                return IoResultVoid::Err(IoError::Make(
                    Engine::IO::IoErrorCode::NotSupported,
                    "VectorDirectoryIterator: reset on closed iterator"));
            }
            index_ = 0;
            return IoResultVoid::Ok();
        }

        IoResultVoid Close() override {
            open_ = false;
            return IoResultVoid::Ok();
        }

    private:
        std::vector<DirectoryEntry> entries_;
        std::size_t index_ = 0;
        const char* backend_ = "VectorIterator";
        bool open_ = true;
    };

} // namespace Engine::IO::FS
