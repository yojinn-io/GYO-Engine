#include "engine/io/stream/StreamReader.hpp"

#include <algorithm>
#include <cstring>

namespace Engine::IO::Stream {


StreamReader::StreamReader(IStream& s) : s_(s) {
    lbuf_.resize(4096);
}

IoResult<std::size_t> StreamReader::ReadExactly(void* dst, std::size_t bytes) {
    std::size_t done = 0;
    auto* out = static_cast<std::byte*>(dst);

    while (done < bytes) {
        auto rr = s_.Read(out + done, bytes - done);
        if (!rr) return rr;

        const std::size_t n = rr.value();
        if (n == 0) {
            return IoResult<std::size_t>::Err(IoError::Make(
                Engine::IO::IoErrorCode::EndOfStream,
                "StreamReader: unexpected EOF in ReadExactly"));
        }
        done += n;
    }
    return IoResult<std::size_t>::Ok(done);
}

IoResult<std::vector<std::byte>> StreamReader::ReadAllBytes(std::size_t maxBytes) {
    if (maxBytes == 0) maxBytes = 0; // 0 は無制限扱い（呼び出し側で opt.maxBytes を渡す想定）

    std::vector<std::byte> out;
    out.reserve(64 * 1024);

    std::vector<std::byte> tmp;
    tmp.resize(64 * 1024);

    for (;;) {
        auto rr = s_.Read(tmp.data(), tmp.size());
        if (!rr) return IoResult<std::vector<std::byte>>::Err(rr.error());

        const std::size_t n = rr.value();
        if (n == 0) break;

        if (maxBytes != 0 && out.size() + n > maxBytes) {
            return IoResult<std::vector<std::byte>>::Err(IoError::Make(
                Engine::IO::IoErrorCode::ReadFailed,
                "StreamReader: ReadAllBytes exceeded maxBytes"));
        }

        out.insert(out.end(), tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(n));
    }

    return IoResult<std::vector<std::byte>>::Ok(std::move(out));
}

IoResult<std::uint8_t> StreamReader::ReadU8() {
    std::uint8_t v = 0;
    auto r = ReadExactly(&v, 1);
    if (!r) return IoResult<std::uint8_t>::Err(r.error());
    return IoResult<std::uint8_t>::Ok(v);
}

IoResult<std::uint16_t> StreamReader::ReadU16LE() {
    std::uint8_t b[2]{};
    auto r = ReadExactly(b, 2);
    if (!r) return IoResult<std::uint16_t>::Err(r.error());
    const std::uint16_t v = static_cast<std::uint16_t>(b[0]) |
                            (static_cast<std::uint16_t>(b[1]) << 8);
    return IoResult<std::uint16_t>::Ok(v);
}

IoResult<std::uint32_t> StreamReader::ReadU32LE() {
    std::uint8_t b[4]{};
    auto r = ReadExactly(b, 4);
    if (!r) return IoResult<std::uint32_t>::Err(r.error());
    const std::uint32_t v = static_cast<std::uint32_t>(b[0]) |
                            (static_cast<std::uint32_t>(b[1]) << 8) |
                            (static_cast<std::uint32_t>(b[2]) << 16) |
                            (static_cast<std::uint32_t>(b[3]) << 24);
    return IoResult<std::uint32_t>::Ok(v);
}

IoResult<std::uint64_t> StreamReader::ReadU64LE() {
    std::uint8_t b[8]{};
    auto r = ReadExactly(b, 8);
    if (!r) return IoResult<std::uint64_t>::Err(r.error());
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= (static_cast<std::uint64_t>(b[i]) << (8 * i));
    }
    return IoResult<std::uint64_t>::Ok(v);
}

IoResult<std::string> StreamReader::ReadAllText(const TextReadOptions& opt) {
    const std::size_t limit = opt.maxBytes; // 0=無制限
    auto br = ReadAllBytes(limit);
    if (!br) return IoResult<std::string>::Err(br.error());

    auto bytes = std::move(br.value());

    // UTF-8 BOM を削除
    std::size_t start = 0;
    if (opt.stripUtf8Bom && bytes.size() >= 3) {
        if (bytes[0] == std::byte{0xEF} && bytes[1] == std::byte{0xBB} && bytes[2] == std::byte{0xBF}) {
            start = 3;
        }
    }

    std::string s;
    s.reserve(bytes.size() - start);
    for (std::size_t i = start; i < bytes.size(); ++i) {
        s.push_back(static_cast<char>(bytes[i]));
    }

    if (!opt.normalizeNewlines) {
        return IoResult<std::string>::Ok(std::move(s));
    }

    // 改行正規化: "\r\n" -> "\n", "\r" -> "\n"
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '\r') {
            if (i + 1 < s.size() && s[i + 1] == '\n') {
                ++i; // skip '\n'
            }
            out.push_back('\n');
        } else {
            out.push_back(c);
        }
    }
    return IoResult<std::string>::Ok(std::move(out));
}

IoResult<std::size_t> StreamReader::FillLineBuffer() {
    lpos_ = 0;
    llen_ = 0;
    auto rr = s_.Read(lbuf_.data(), lbuf_.size());
    if (!rr) return rr;
    llen_ = rr.value();
    return IoResult<std::size_t>::Ok(llen_);
}

IoResult<bool> StreamReader::ReadLine(std::string& outLine, std::size_t maxLineBytes) {
    outLine.clear();

    for (;;) {
        // バッファが空なら補充
        if (lpos_ >= llen_) {
            auto fr = FillLineBuffer();
            if (!fr) return IoResult<bool>::Err(fr.error());
            if (fr.value() == 0) {
                // EOF：途中まで読んでいた行があれば true、無ければ false
                return IoResult<bool>::Ok(!outLine.empty());
            }
        }

        // '\n' を探す
        std::size_t i = lpos_;
        for (; i < llen_; ++i) {
            if (lbuf_[i] == std::byte{'\n'}) break;
        }

        const std::size_t chunk = i - lpos_;
        if (maxLineBytes != 0 && outLine.size() + chunk > maxLineBytes) {
            return IoResult<bool>::Err(IoError::Make(
                Engine::IO::IoErrorCode::ReadFailed,
                "StreamReader: line too long"));
        }

        // 追加
        for (std::size_t k = 0; k < chunk; ++k) {
            outLine.push_back(static_cast<char>(lbuf_[lpos_ + k]));
        }

        // '\n' 見つかった
        if (i < llen_ && lbuf_[i] == std::byte{'\n'}) {
            lpos_ = i + 1;

            // 末尾の '\r' を削除（CRLF 対応）
            if (!outLine.empty() && outLine.back() == '\r') outLine.pop_back();

            return IoResult<bool>::Ok(true);
        }

        // 見つからなかった：バッファを使い切った
        lpos_ = llen_;
    }
}

} // namespace Engine::IO::Stream
