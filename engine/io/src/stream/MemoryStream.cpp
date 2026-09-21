#include "engine/io/stream/MemoryStream.hpp"

namespace Engine::IO::Stream {
    IoResult<std::size_t> MemoryStream::Read(void* dst, std::size_t bytes) {
        if (!open_) {
            return IoResult<std::size_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::ReadFailed,
                "MemoryStream: read on closed stream"));
        }
        if (!opt_.readable) {
            return IoResult<std::size_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported,
                "MemoryStream: read not supported"));
        }
        if (bytes == 0) return IoResult<std::size_t>::Ok(0);

        const std::uint64_t size = static_cast<std::uint64_t>(buf_.size());
        if (pos_ >= size) {
            eof_ = true;
            return IoResult<std::size_t>::Ok(0);
        }

        const std::uint64_t remain = size - pos_;
        const std::size_t n = static_cast<std::size_t>(
            (std::min<std::uint64_t>)(remain, static_cast<std::uint64_t>(bytes))
        );

        std::memcpy(dst, buf_.data() + pos_, n);
        pos_ += static_cast<std::uint64_t>(n);
        eof_ = (pos_ >= size);
        return IoResult<std::size_t>::Ok(n);
    }

    IoResult<std::size_t> MemoryStream::Write(const void* src, std::size_t bytes)  {
        if (!open_) {
            return IoResult<std::size_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::WriteFailed,
                "MemoryStream: write on closed stream"));
        }
        if (!opt_.writable) {
            return IoResult<std::size_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported,
                "MemoryStream: write not supported"));
        }
        if (bytes == 0) return IoResult<std::size_t>::Ok(0);

        // 必要サイズ（pos_ は “末尾より先” も許可：growable+writable の場合）
        const std::uint64_t need = pos_ + static_cast<std::uint64_t>(bytes);

        if (need > static_cast<std::uint64_t>(buf_.size())) {
            if (!opt_.growable) {
                return IoResult<std::size_t>::Err(IoError::Make(
                    Engine::IO::IoErrorCode::WriteFailed,
                    "MemoryStream: no space (not growable)",
                    "need=" + std::to_string(need) + " size=" + std::to_string(buf_.size())));
            }
            buf_.resize(static_cast<std::size_t>(need));
        }

        std::memcpy(buf_.data() + pos_, src, bytes);
        pos_ += static_cast<std::uint64_t>(bytes);
        eof_ = false; // 書き込んだら EOF 状態は解除
        return IoResult<std::size_t>::Ok(bytes);
    }

    IoResult<std::uint64_t> MemoryStream::Tell() const  {
        if (!open_) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::SeekFailed,
                "MemoryStream: tell on closed stream"));
        }
        return IoResult<std::uint64_t>::Ok(pos_);
    }

    IoResult<std::uint64_t> MemoryStream::Seek(std::int64_t offset, SeekWhence whence)  {
        if (!open_) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::SeekFailed,
                "MemoryStream: seek on closed stream"));
        }

        const std::int64_t cur  = static_cast<std::int64_t>(pos_);
        const std::int64_t end  = static_cast<std::int64_t>(buf_.size());
        std::int64_t target = 0;

        switch (whence) {
        case SeekWhence::Begin:   target = offset; break;
        case SeekWhence::Current: target = cur + offset; break;
        case SeekWhence::End:     target = end + offset; break;
        default:                  target = offset; break;
        }

        if (target < 0) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::SeekFailed,
                "MemoryStream: seek before begin",
                "target=" + std::to_string(target)));
        }

        // growable+writable の場合は end を超える seek を許可（後で write が拡張する）
        const bool allowBeyondEnd = (opt_.writable && opt_.growable);
        if (!allowBeyondEnd && target > end) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::SeekFailed,
                "MemoryStream: seek beyond end",
                "target=" + std::to_string(target) + " end=" + std::to_string(end)));
        }

        pos_ = static_cast<std::uint64_t>(target);
        eof_ = false;
        return IoResult<std::uint64_t>::Ok(pos_);
    }

    IoResult<std::uint64_t> MemoryStream::Size() const  {
        if (!open_) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported,
                "MemoryStream: size on closed stream"));
        }
        return IoResult<std::uint64_t>::Ok(static_cast<std::uint64_t>(buf_.size()));
    }

    IoResultVoid MemoryStream::Flush()  {
        if (!open_) {
            return IoResultVoid::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported,
                "MemoryStream: flush on closed stream"));
        }
        // no-op
        return IoResultVoid::Ok();
    }

    IoResultVoid MemoryStream::Close()  {
        open_ = false;
        eof_ = false;
        return IoResultVoid::Ok();
    }
}
