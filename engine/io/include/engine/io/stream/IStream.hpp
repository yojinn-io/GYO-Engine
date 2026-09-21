#pragma once
#include <cstdint>
#include <cstddef>
#include <span>

#include "engine/base/Result.hpp"   // Engine::Result<T>
#include "engine/base/Error.hpp"    // Engine::Error（必要なら）
#include "engine/base/Span.hpp"
#include "engine/io/IoError.hpp"    // IoErrorCode（ReadFailed/WriteFailed/SeekFailed/NotSupported など）
#include "engine/io/stream/Seek.hpp"


namespace Engine::IO::Stream {
    using IoError = Base::Error<IoErrorCode>;

    struct StreamCaps final {
        bool readable = false;
        bool writable = false;
        bool seekable = false;
    };

    /// IStream：読み書き/seek の最小抽象
    /// - Open/Close は IFileSystem が責務を持つことが多いが、Close を持たせると取り回しが良い
    /// - Read/Write は「実際に処理できたバイト数」を返す（部分読み書きがあり得る）
    class IStream {
    public:
        virtual ~IStream() = default;

        // 能力（NotSupported を返す代わりに、事前判定できる）
        virtual StreamCaps Caps() const noexcept = 0;

        // 状態
        virtual bool IsOpen() const noexcept = 0;
        virtual bool IsEof() const noexcept = 0; // 末尾到達（Read=0 と同義でもOK）

        // ---- Read / Write ----
        // 成功：読み込めた/書き込めたバイト数（0もあり得る）
        // 失敗：unexpected(Error)（IoErrorCode::ReadFailed 等）
        virtual Base::Result<std::size_t, IoError> Read(void* dst, std::size_t bytes) = 0;
        virtual Base::Result<std::size_t, IoError> Write(const void* src, std::size_t bytes) = 0;

        // span 版（非virtualラッパ）
        Base::Result<std::size_t, IoError> Read(Base::Span<std::byte> dst) {
            return Read(dst.data(), dst.size());
        }
        Base::Result<std::size_t, IoError> Write(Base::Span<const std::byte> src) {
            return Write(src.data(), src.size());
        }

        // ---- Seek / Tell / Size ----
        // seekable でない場合は NotSupported を返す
        virtual Base::Result<std::uint64_t, IoError> Tell() const = 0;
        virtual Base::Result<std::uint64_t, IoError> Seek(std::int64_t offset, SeekWhence whence) = 0;

        // サイズ取得は「可能なら提供」。
        // - seekable な実装なら比較的簡単
        // - network stream 等は NotSupported でもOK
        virtual Base::Result<std::uint64_t, IoError> Size() const = 0;

        // ---- Flush / Close ----
        // writable でない場合 Flush は NotSupported でもOK
        virtual Base::Result<void, IoError> Flush() = 0;

        // Close を持たせると stream 単体で寿命管理しやすい（unique_ptr で扱いやすい）
        virtual Base::Result<void, IoError> Close() = 0;

        // copy禁止（基本は unique_ptr で持つ）
        IStream(const IStream&) = delete;
        IStream& operator=(const IStream&) = delete;

    protected:
        IStream() = default;
    };

} // namespace Engine::IO::Stream
