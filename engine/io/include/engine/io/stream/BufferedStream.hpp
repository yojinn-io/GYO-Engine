#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/io/IoError.hpp"
#include "engine/io/stream/IStream.hpp"
#include "engine/io/stream/Seek.hpp"

namespace Engine::IO::Stream {

    using IoError  = Engine::Base::Error<Engine::IO::IoErrorCode>;
    template<class T>
    using IoResult = Engine::Base::Result<T, IoError>;
    using IoResultVoid = Engine::Base::Result<void, IoError>;

    struct BufferingOptions final {
        std::size_t readBufferSize  = 64 * 1024;
        std::size_t writeBufferSize = 64 * 1024;
        bool enableRead  = true;
        bool enableWrite = true;
    };

    class BufferedStream final : public IStream {
    public:
        explicit BufferedStream(std::unique_ptr<IStream> inner, BufferingOptions opt = {});
        ~BufferedStream() override;

        StreamCaps Caps() const noexcept override;
        bool IsOpen() const noexcept override;
        bool IsEof() const noexcept override;

        IoResult<std::size_t> Read(void* dst, std::size_t bytes) override;
        IoResult<std::size_t> Write(const void* src, std::size_t bytes) override;

        IoResult<std::uint64_t> Tell() const override;
        IoResult<std::uint64_t> Seek(std::int64_t offset, SeekWhence whence) override;
        IoResult<std::uint64_t> Size() const override;

        IoResultVoid Flush() override;
        IoResultVoid Close() override;

        IStream& Inner() noexcept { return *inner_; }
        const IStream& Inner() const noexcept { return *inner_; }

    private:
        IoResultVoid FlushWriteBuffer();
        IoResult<std::size_t> FillReadBuffer();

        // read -> write の切替で「未消費 read buffer」を元に戻す
        IoResultVoid SyncForWrite();

        // write -> read の切替で write を確定
        IoResultVoid SyncForRead();

    private:
        std::unique_ptr<IStream> inner_;
        BufferingOptions opt_;

        // read buffer
        std::vector<std::byte> rbuf_;
        std::size_t rpos_ = 0; // 次に読む位置
        std::size_t rlen_ = 0; // 有効データ長

        // write buffer
        std::vector<std::byte> wbuf_;
        std::size_t wlen_ = 0; // バッファ済みデータ長
    };

} // namespace Engine::IO::Stream
