#include "engine/io/stream/StreamWriter.hpp"

#include <cstring>

namespace Engine::IO::Stream {


    StreamWriter::StreamWriter(IStream& s) : s_(s) {}

    IoResultVoid StreamWriter::WriteExactly(const void* src, std::size_t bytes) {
        std::size_t done = 0;
        const auto* in = static_cast<const std::byte*>(src);

        while (done < bytes) {
            auto wr = s_.Write(in + done, bytes - done);
            if (!wr) return IoResultVoid::Err(wr.error());

            const std::size_t n = wr.value();
            if (n == 0) {
                return IoResultVoid::Err(IoError::Make(
                    Engine::IO::IoErrorCode::WriteFailed,
                    "StreamWriter: inner write returned 0 (stalled)"));
            }
            done += n;
        }
        return IoResultVoid::Ok();
    }

    IoResultVoid StreamWriter::WriteAllBytes(Engine::Base::Span<const std::byte> bytes) {
        if (bytes.empty()) return IoResultVoid::Ok();
        return WriteExactly(bytes.data(), bytes.size());
    }

    IoResultVoid StreamWriter::WriteAllText(std::string_view text, const TextWriteOptions& opt) {
        // BOM
        if (opt.writeUtf8Bom) {
            const std::byte bom[3] = { std::byte{0xEF}, std::byte{0xBB}, std::byte{0xBF} };
            auto br = WriteExactly(bom, 3);
            if (!br) return br;
        }

        if (!opt.normalizeNewlines) {
            return WriteExactly(text.data(), text.size());
        }

        // "\n" -> "\r\n"
        std::string tmp;
        tmp.reserve(text.size() + 16);

        for (char c : text) {
            if (c == '\n') {
                tmp.push_back('\r');
                tmp.push_back('\n');
            } else {
                tmp.push_back(c);
            }
        }
        return WriteExactly(tmp.data(), tmp.size());
    }

    IoResultVoid StreamWriter::WriteU8(std::uint8_t v) {
        return WriteExactly(&v, 1);
    }

    IoResultVoid StreamWriter::WriteU16LE(std::uint16_t v) {
        std::uint8_t b[2] = {
            static_cast<std::uint8_t>(v & 0xFF),
            static_cast<std::uint8_t>((v >> 8) & 0xFF)
        };
        return WriteExactly(b, 2);
    }

    IoResultVoid StreamWriter::WriteU32LE(std::uint32_t v) {
        std::uint8_t b[4] = {
            static_cast<std::uint8_t>(v & 0xFF),
            static_cast<std::uint8_t>((v >> 8) & 0xFF),
            static_cast<std::uint8_t>((v >> 16) & 0xFF),
            static_cast<std::uint8_t>((v >> 24) & 0xFF)
        };
        return WriteExactly(b, 4);
    }

    IoResultVoid StreamWriter::WriteU64LE(std::uint64_t v) {
        std::uint8_t b[8];
        for (int i = 0; i < 8; ++i) {
            b[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
        }
        return WriteExactly(b, 8);
    }

    IoResultVoid StreamWriter::WriteLine(std::string_view line) {
        auto r1 = WriteExactly(line.data(), line.size());
        if (!r1) return r1;

        const char nl = '\n';
        return WriteExactly(&nl, 1);
    }

    IoResultVoid StreamWriter::Flush() {
        return s_.Flush();
    }

} // namespace Engine::IO::Stream
