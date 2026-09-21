#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/io/IoError.hpp"
#include "engine/io/stream/IStream.hpp" // StreamCaps, IStream
#include "engine/io/stream/Seek.hpp"    // SeekWhence

namespace Engine::IO::Stream {

    using IoError  = Engine::Base::Error<Engine::IO::IoErrorCode>;
    template<class T>
    using IoResult = Engine::Base::Result<T, IoError>;
    using IoResultVoid = Engine::Base::Result<void, IoError>;

    class MemoryStream final : public IStream {
    public:
        struct Options final {
            bool readable = true;
            bool writable = true;
            bool growable = true; // true: 書き込みで自動拡張
        };

        MemoryStream() : opt_{} {}
        explicit MemoryStream(Options opt) : opt_(opt) {}

        explicit MemoryStream(std::vector<std::byte> data, Options opt)
            : opt_(opt), buf_(std::move(data)) {}

        const std::vector<std::byte>& Buffer() const noexcept { return buf_; }
        std::vector<std::byte>& Buffer() noexcept { return buf_; }

        std::uint64_t Position() const noexcept { return pos_; }

        // ----- IStream -----
        StreamCaps Caps() const noexcept override {
            StreamCaps c;
            c.readable = opt_.readable;
            c.writable = opt_.writable;
            c.seekable = true;
            return c;
        }

        bool IsOpen() const noexcept override { return open_; }
        bool IsEof() const noexcept override { return eof_; }

        IoResult<std::size_t> Read(void* dst, std::size_t bytes) override;
        IoResult<std::size_t> Write(const void* src, std::size_t bytes) override;

        IoResult<std::uint64_t> Tell() const override;
        IoResult<std::uint64_t> Seek(std::int64_t offset, SeekWhence whence) override;
        IoResult<std::uint64_t> Size() const override;

        IoResultVoid Flush() override;
        IoResultVoid Close() override;

    private:
        Options opt_{};
        std::vector<std::byte> buf_;
        std::uint64_t pos_ = 0;
        bool open_ = true;
        bool eof_ = false;
    };

} // namespace Engine::IO::Stream
