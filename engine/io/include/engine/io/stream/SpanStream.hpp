#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"
#include "engine/io/IoError.hpp"
#include "engine/io/stream/IStream.hpp"
#include "engine/io/stream/Seek.hpp"

namespace Engine::IO::Stream {

    using IoError  = Base::Error<Engine::IO::IoErrorCode>;

    template<class T>
    using IoResult = Base::Result<T, IoError>;

    using IoResultVoid = Base::Result<void, IoError>;

    class SpanStream final : public IStream {
    public:
        // ReadOnly
        explicit SpanStream(Base::Span<const std::byte> ro)
            : ro_(ro.data()), rw_(nullptr), size_(static_cast<std::uint64_t>(ro.size())),
              writable_(false) {}

        // ReadWrite
        explicit SpanStream(Base::Span<std::byte> rw)
            : ro_(rw.data()), rw_(rw.data()), size_(static_cast<std::uint64_t>(rw.size())),
              writable_(true) {}

        Stream::StreamCaps Caps() const noexcept override {
            StreamCaps c;
            c.readable = true;
            c.writable = writable_;
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
        const std::byte* ro_ = nullptr;
        std::byte* rw_ = nullptr;
        std::uint64_t size_ = 0;
        std::uint64_t pos_  = 0;
        bool writable_ = false;
        bool open_ = true;
        bool eof_ = false;
    };

} // namespace Engine::IO::Stream
