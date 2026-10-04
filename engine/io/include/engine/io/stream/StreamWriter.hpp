#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"
#include "engine/io/IoError.hpp"
#include "engine/io/stream/IStream.hpp"

namespace Engine::IO::Stream {

    using IoError  = Engine::Base::Error<Engine::IO::IoErrorCode>;
    template<class T>
    using IoResult = Engine::Base::Result<T, IoError>;

    struct TextWriteOptions final {
        bool writeUtf8Bom = false;
        bool normalizeNewlines = false; // "\n" -> "\r\n"（tools向け）
    };

    class StreamWriter final {
    public:
        explicit StreamWriter(IStream& s);

        IoResult<void> WriteAllBytes(Engine::Base::Span<const std::byte> bytes);
        IoResult<void> WriteAllText(std::string_view text, const TextWriteOptions& opt = {});

        IoResult<void> WriteU8(std::uint8_t v);
        IoResult<void> WriteU16LE(std::uint16_t v);
        IoResult<void> WriteU32LE(std::uint32_t v);
        IoResult<void> WriteU64LE(std::uint64_t v);

        IoResult<void> WriteLine(std::string_view line);

        IoResult<void> Flush();

    private:
        IoResult<void> WriteExactly(const void* src, std::size_t bytes);

    private:
        IStream& s_;
    };

} // namespace Engine::IO::Stream
