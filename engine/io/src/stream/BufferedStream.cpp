#include "engine/io/stream/BufferedStream.hpp"

#include <algorithm>
#include <cstring>

namespace Engine::IO::Stream {

    // static inline IoError IoError::Make(Engine::IO::IoErrorCode code, const char* msg, std::string detail = {}) {
    //     return IoError::Make(code, msg, std::move(detail));
    // }

    BufferedStream::BufferedStream(std::unique_ptr<IStream> inner, BufferingOptions opt)
        : inner_(std::move(inner)), opt_(opt) {

        if (opt_.enableRead && opt_.readBufferSize > 0) {
            rbuf_.resize(opt_.readBufferSize);
        }
        if (opt_.enableWrite && opt_.writeBufferSize > 0) {
            wbuf_.resize(opt_.writeBufferSize);
        }
    }

    BufferedStream::~BufferedStream() {
        // デストラクタで例外は禁止。best-effort flush
        (void)Flush();
    }

    StreamCaps BufferedStream::Caps() const noexcept {
        return inner_ ? inner_->Caps() : StreamCaps{};
    }

    bool BufferedStream::IsOpen() const noexcept {
        return inner_ && inner_->IsOpen();
    }

    bool BufferedStream::IsEof() const noexcept {
        // read buffer に未消費があれば EOF ではない
        if (rpos_ < rlen_) return false;
        return inner_ ? inner_->IsEof() : true;
    }

    IoResultVoid BufferedStream::SyncForRead() {
        // 読む前に書き込みを確定
        if (wlen_ > 0) {
            auto fr = FlushWriteBuffer();
            if (!fr) return fr;
        }
        // 読み側はそのまま（seek時に破棄する）
        return IoResultVoid::Ok();
    }

    IoResultVoid BufferedStream::SyncForWrite() {
        // 未消費の read buffer がある場合、
        // inner の位置は「先読み済み」の末尾にあるため、論理位置まで戻す必要がある
        if (rlen_ > rpos_) {
            const auto caps = inner_->Caps();
            if (!caps.seekable) {
                return IoResultVoid::Err(IoError::Make(
                    Engine::IO::IoErrorCode::NotSupported,
                    "BufferedStream: switching read->write requires seekable inner stream"));
            }

            const std::int64_t unread = static_cast<std::int64_t>(rlen_ - rpos_);
            auto sr = inner_->Seek(-unread, SeekWhence::Current);
            if (!sr) {
                return IoResultVoid::Err(IoError::Make(
                    Engine::IO::IoErrorCode::SeekFailed,
                    "BufferedStream: failed to rewind unread read-buffer bytes"));
            }
        }
        // read buffer を破棄
        rpos_ = 0;
        rlen_ = 0;
        return IoResultVoid::Ok();
    }

    IoResult<std::size_t> BufferedStream::FillReadBuffer() {
        rpos_ = 0;
        rlen_ = 0;

        if (!opt_.enableRead || rbuf_.empty()) {
            return IoResult<std::size_t>::Ok(0);
        }

        auto rr = inner_->Read(rbuf_.data(), rbuf_.size());
        if (!rr) return rr; // inner 由来のエラーをそのまま返す

        rlen_ = rr.value();
        return IoResult<std::size_t>::Ok(rlen_);
    }

    IoResultVoid BufferedStream::FlushWriteBuffer() {
        if (!opt_.enableWrite || wbuf_.empty() || wlen_ == 0) {
            return IoResultVoid::Ok();
        }

        std::size_t writtenTotal = 0;
        while (writtenTotal < wlen_) {
            auto wr = inner_->Write(wbuf_.data() + writtenTotal, wlen_ - writtenTotal);
            if (!wr) return IoResultVoid::Err(wr.error());

            const std::size_t n = wr.value();
            if (n == 0) {
                return IoResultVoid::Err(IoError::Make(
                    Engine::IO::IoErrorCode::WriteFailed,
                    "BufferedStream: inner write returned 0 (stalled)"));
            }
            writtenTotal += n;
        }

        wlen_ = 0;
        return IoResultVoid::Ok();
    }

    IoResult<std::size_t> BufferedStream::Read(void* dst, std::size_t bytes) {
        if (!inner_ || !inner_->IsOpen()) {
            return IoResult<std::size_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::ReadFailed, "BufferedStream: read on closed stream"));
        }
        if (bytes == 0) return IoResult<std::size_t>::Ok(0);

        // write -> read の同期
        auto sr = SyncForRead();
        if (!sr) return IoResult<std::size_t>::Err(sr.error());

        // バッファ無効ならパススルー
        if (!opt_.enableRead || rbuf_.empty()) {
            return inner_->Read(dst, bytes);
        }

        std::size_t out = 0;
        auto* outPtr = static_cast<std::byte*>(dst);

        while (out < bytes) {
            const std::size_t avail = (rlen_ > rpos_) ? (rlen_ - rpos_) : 0;

            if (avail == 0) {
                // バッファ補充
                auto fr = FillReadBuffer();
                if (!fr) return fr;
                if (fr.value() == 0) {
                    // EOF
                    break;
                }
                continue;
            }

            const std::size_t need = bytes - out;
            const std::size_t n = (std::min)(need, avail);

            std::memcpy(outPtr + out, rbuf_.data() + rpos_, n);
            rpos_ += n;
            out += n;

            if (out == bytes) break;
        }

        return IoResult<std::size_t>::Ok(out);
    }

    IoResult<std::size_t> BufferedStream::Write(const void* src, std::size_t bytes) {
        if (!inner_ || !inner_->IsOpen()) {
            return IoResult<std::size_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::WriteFailed, "BufferedStream: write on closed stream"));
        }
        if (bytes == 0) return IoResult<std::size_t>::Ok(0);

        // read -> write の同期（未消費 read buffer を巻き戻す）
        auto sw = SyncForWrite();
        if (!sw) return IoResult<std::size_t>::Err(sw.error());

        // バッファ無効ならパススルー
        if (!opt_.enableWrite || wbuf_.empty()) {
            return inner_->Write(src, bytes);
        }

        const auto* inPtr = static_cast<const std::byte*>(src);
        std::size_t inOff = 0;

        while (inOff < bytes) {
            const std::size_t cap = wbuf_.size();
            const std::size_t free = cap - wlen_;

            // 直接書いたほうが良い（大きい）ケース：既存バッファを flush して直書き
            if (wlen_ == 0 && (bytes - inOff) >= cap) {
                auto wr = inner_->Write(inPtr + inOff, bytes - inOff);
                if (!wr) return wr;
                const std::size_t n = wr.value();
                if (n == 0) {
                    return IoResult<std::size_t>::Err(IoError::Make(
                        Engine::IO::IoErrorCode::WriteFailed,
                        "BufferedStream: inner write returned 0 (stalled)"));
                }
                inOff += n;
                continue;
            }

            // バッファへコピー
            const std::size_t n = (std::min)(free, bytes - inOff);
            std::memcpy(wbuf_.data() + wlen_, inPtr + inOff, n);
            wlen_ += n;
            inOff += n;

            // 満杯なら flush
            if (wlen_ == cap) {
                auto fr = FlushWriteBuffer();
                if (!fr) return IoResult<std::size_t>::Err(fr.error());
            }
        }

        return IoResult<std::size_t>::Ok(bytes);
    }

    IoResult<std::uint64_t> BufferedStream::Tell() const {
        if (!inner_ || !inner_->IsOpen()) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::SeekFailed, "BufferedStream: tell on closed stream"));
        }

        const auto caps = inner_->Caps();
        if (!caps.seekable) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported, "BufferedStream: tell requires seekable inner stream"));
        }

        auto tr = inner_->Tell();
        if (!tr) return tr;

        std::uint64_t pos = tr.value();

        // 読み：inner は先読み済みの末尾に居るので、未消費分を戻す
        if (rlen_ > rpos_) {
            pos -= static_cast<std::uint64_t>(rlen_ - rpos_);
        }

        // 書き：未フラッシュ分を足す
        if (wlen_ > 0) {
            pos += static_cast<std::uint64_t>(wlen_);
        }

        return IoResult<std::uint64_t>::Ok(pos);
    }

    IoResult<std::uint64_t> BufferedStream::Seek(std::int64_t offset, SeekWhence whence) {
        if (!inner_ || !inner_->IsOpen()) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::SeekFailed, "BufferedStream: seek on closed stream"));
        }

        const auto caps = inner_->Caps();
        if (!caps.seekable) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported, "BufferedStream: seek requires seekable inner stream"));
        }

        // 書き込みは確定してから seek
        auto fr = FlushWriteBuffer();
        if (!fr) return IoResult<std::uint64_t>::Err(fr.error());

        // read buffer は無効化（位置が飛ぶので）
        rpos_ = 0;
        rlen_ = 0;

        return inner_->Seek(offset, whence);
    }

    IoResult<std::uint64_t> BufferedStream::Size() const {
        if (!inner_ || !inner_->IsOpen()) {
            return IoResult<std::uint64_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported, "BufferedStream: size on closed stream"));
        }

        auto sr = inner_->Size();
        if (!sr) return sr;

        std::uint64_t sz = sr.value();

        // 未フラッシュの write があれば “論理サイズ” は増えている可能性
        if (wlen_ > 0) {
            auto tr = inner_->Tell();
            if (tr) {
                const std::uint64_t logicalEnd = tr.value() + static_cast<std::uint64_t>(wlen_);
                if (logicalEnd > sz) sz = logicalEnd;
            }
        }
        return IoResult<std::uint64_t>::Ok(sz);
    }

    IoResultVoid BufferedStream::Flush() {
        if (!inner_ || !inner_->IsOpen()) {
            return IoResultVoid::Err(IoError::Make(
                Engine::IO::IoErrorCode::NotSupported, "BufferedStream: flush on closed stream"));
        }

        auto fr = FlushWriteBuffer();
        if (!fr) return fr;

        // inner flush
        return inner_->Flush();
    }

    IoResultVoid BufferedStream::Close() {
        if (!inner_) return IoResultVoid::Ok();

        // best-effort flush then close
        auto fr = FlushWriteBuffer();
        if (!fr) return fr;

        // read buffer 破棄
        rpos_ = 0;
        rlen_ = 0;

        return inner_->Close();
    }

} // namespace Engine::IO::Stream
