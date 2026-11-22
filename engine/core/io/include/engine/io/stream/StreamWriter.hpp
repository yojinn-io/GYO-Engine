#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"
#include "engine/io/IOError.hpp"
#include "engine/io/stream/IStream.hpp"

namespace GYO::Engine::IO::Stream {


    struct TextWriteOptions final {
        bool writeUtf8Bom = false;
        bool normalizeNewlines = false; // "\n" -> "\r\n"（tools向け）
    };

    class StreamWriter final {
    public:
        explicit StreamWriter(IStream& s);

        IOResult<void> WriteAllBytes(GYO::Engine::Base::Span<const std::byte> bytes);
        IOResult<void> WriteAllText(std::string_view text, const TextWriteOptions& opt = {});

        IOResult<void> WriteU8(std::uint8_t v);
        IOResult<void> WriteU16LE(std::uint16_t v);
        IOResult<void> WriteU32LE(std::uint32_t v);
        IOResult<void> WriteU64LE(std::uint64_t v);

        IOResult<void> WriteLine(std::string_view line);

        IOResult<void> Flush();

    private:
        IOResult<void> WriteExactly(const void* src, std::size_t bytes);

    private:
        IStream& s_;
    };

} // namespace GYO::Engine::IO::Stream
