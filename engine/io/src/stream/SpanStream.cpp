#include "engine/io/stream/SpanStream.hpp"

namespace Engine::IO::Stream {
    IoResult<std::size_t> SpanStream::Read(void* dst, std::size_t bytes)  {
        if (!open_) {
            return IoResult<std::size_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::ReadFailed,
                "SpanStream: read on closed stream"));
        }
        if (bytes == 0) return IoResult<std::size_t>::Ok(0);

        if (pos_ >= size_) {
            eof_ = true;
            return IoResult<std::size_t>::Ok(0);
        }

        const std::uint64_t remain = size_ - pos_;
        const std::size_t n = static_cast<std::size_t>(
            (std::min<std::uint64_t>)(remain, static_cast<std::uint64_t>(bytes))
        );

        std::memcpy(dst, ro_ + pos_, n);
        pos_ += static_cast<std::uint64_t>(n);
        eof_ = (pos_ >= size_);
        return IoResult<std::size_t>::Ok(n);
    }

    IoResult<std::size_t> SpanStream::Write(const void* src, std::size_t bytes)  {
        if (!open_) {
            return IoResult<std::size_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::WriteFailed,
                "SpanStream: write on closed stream"));
        }
        if (!writable_) {
            return IoResult<std::size_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported,
                "SpanStream: write not supported (read-only)"));
        }
        if (bytes == 0) return IoResult<std::size_t>::Ok(0);

        const std::uint64_t need = pos_ + static_cast<std::uint64_t>(bytes);
        if (need > size_) {
            return IoResult<std::size_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::WriteFailed,
                "SpanStream: write beyond end",
                "need=" + std::to_string(need) + " size=" + std::to_string(size_)));
        }

        std::memcpy(rw_ + pos_, src, bytes);
        pos_ += static_cast<std::uint64_t>(bytes);
        eof_ = false;
        return IoResult<std::size_t>::Ok(bytes);
    }

    IoResult<std::uint64_t> SpanStream::Tell() const  {
        if (!open_) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::SeekFailed,
                "SpanStream: tell on closed stream"));
        }
        return IoResult<std::uint64_t>::Ok(pos_);
    }

    IoResult<std::uint64_t> SpanStream::Seek(std::int64_t offset, SeekWhence whence)  {
        if (!open_) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::SeekFailed,
                "SpanStream: seek on closed stream"));
        }

        const std::int64_t cur = static_cast<std::int64_t>(pos_);
        const std::int64_t end = static_cast<std::int64_t>(size_);
        std::int64_t target = 0;

        switch (whence) {
        case SeekWhence::Begin:   target = offset; break;
        case SeekWhence::Current: target = cur + offset; break;
        case SeekWhence::End:     target = end + offset; break;
        default:                  target = offset; break;
        }

        if (target < 0 || target > end) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::SeekFailed,
                "SpanStream: seek out of range",
                "target=" + std::to_string(target) + " size=" + std::to_string(end)));
        }

        pos_ = static_cast<std::uint64_t>(target);
        eof_ = false;
        return IoResult<std::uint64_t>::Ok(pos_);
    }

    IoResult<std::uint64_t> SpanStream::Size() const  {
        if (!open_) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported,
                "SpanStream: size on closed stream"));
        }
        return IoResult<std::uint64_t>::Ok(size_);
    }

    IoResultVoid SpanStream::Flush()  {
        if (!open_) {
            return IoResultVoid::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported,
                "SpanStream: flush on closed stream"));
        }
        return IoResultVoid::Ok(); // no-op
    }

    IoResultVoid SpanStream::Close()  {
        open_ = false;
        eof_ = false;
        return IoResultVoid::Ok();
    }
}