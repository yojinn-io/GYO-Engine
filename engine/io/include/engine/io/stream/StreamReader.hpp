#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/io/IoError.hpp"
#include "engine/io/stream/IStream.hpp"

namespace Engine::IO::Stream {

    using IoError  = Engine::Base::Error<Engine::IO::IoErrorCode>;

    template<class T>
    using IoResult = Engine::Base::Result<T, IoError>;

    using IoResultVoid = Engine::Base::Result<void, IoError>;

    struct TextReadOptions final {
        bool stripUtf8Bom = true;
        bool normalizeNewlines = false; // "\r\n" or "\r" -> "\n"
        std::size_t maxBytes = 64 * 1024 * 1024; // 0=無制限
    };

    class StreamReader final {
    public:
        explicit StreamReader(IStream& s);

        IoResult<std::vector<std::byte>> ReadAllBytes(std::size_t maxBytes = 0);
        IoResult<std::size_t> ReadExactly(void* dst, std::size_t bytes);

        IoResult<std::uint8_t>  ReadU8();
        IoResult<std::uint16_t> ReadU16LE();
        IoResult<std::uint32_t> ReadU32LE();
        IoResult<std::uint64_t> ReadU64LE();

        IoResult<std::string> ReadAllText(const TextReadOptions& opt = {});
        IoResult<bool> ReadLine(std::string& outLine, std::size_t maxLineBytes = 4096);

    private:
        IoResult<std::size_t> FillLineBuffer();

    private:
        IStream& s_;

        // ReadLine 用の内部バッファ（過剰に複雑化しない範囲で高速化）
        std::vector<std::byte> lbuf_;
        std::size_t lpos_ = 0;
        std::size_t llen_ = 0;
    };

} // namespace Engine::IO::Stream
