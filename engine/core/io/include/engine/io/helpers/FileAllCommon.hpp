#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"
#include "engine/io/IOError.hpp"
#include "engine/io/fs/IFileSystem.hpp"
#include "engine/io/stream/FileOpenMode.hpp"
#include "engine/io/stream/IStream.hpp"

namespace GYO::Engine::IO::Helpers {

    struct ReadAllOptions final {
        /// More bytes than this is TooLarge.
        std::size_t maxBytes = 64u * 1024u * 1024u;
        std::size_t chunkBytes = 64u * 1024u;
        /// Tries stream.Size() first, to reserve and to refuse a file over maxBytes before reading it.
        bool tryUseSizeHint = true;
    };

    inline IOError MakeErr(IOErrorCode code, const char* msg, std::string detail = {}) {
        return IOError::Make(code, msg, std::move(detail));
    }

    /// The one error of a whole-file read over its caller's limit.
    inline IOError TooLargeErr() { return MakeErr(IOErrorCode::TooLarge, "ReadAllBytes: exceeds maxBytes"); }

    inline IOResult<std::unique_ptr<Stream::IStream>> OpenRead(FS::IFileSystem& fs, const Path::Uri& uri) {
        using Stream::FileOpenMode;
        return fs.Open(uri, FileOpenMode::Read | FileOpenMode::Binary);
    }

    inline IOResult<std::unique_ptr<Stream::IStream>> OpenWriteTruncate(FS::IFileSystem& fs, const Path::Uri& uri) {
        using Stream::FileOpenMode;
        return fs.Open(uri, FileOpenMode::Write | FileOpenMode::Binary | FileOpenMode::Truncate |
                                FileOpenMode::CreateIfMissing);
    }

    /// Reads the stream to its end. More than opt.maxBytes is TooLarge, so a caller can tell a file that is too
    /// big from one it could not read (docs/architecture/io.md).
    inline IOResult<std::vector<std::byte>> ReadAllFromStream(Stream::IStream& s, const ReadAllOptions& opt) {
        std::vector<std::byte> out;
        if (opt.tryUseSizeHint) {
            auto szr = s.Size();
            if (szr) {
                if (szr.value() > opt.maxBytes) {
                    return Base::Err(TooLargeErr());
                }
                out.reserve(static_cast<std::size_t>(szr.value()));
            }
        }
        std::vector<std::byte> buf(opt.chunkBytes);
        while (true) {
            auto rr = s.Read(buf.data(), buf.size());
            if (!rr) return Base::Err(rr.error());
            const std::size_t n = rr.value();
            if (n == 0) break;
            if (out.size() + n > opt.maxBytes) {
                return Base::Err(TooLargeErr());
            }
            const std::size_t old = out.size();
            out.resize(old + n);
            std::memcpy(out.data() + old, buf.data(), n);
        }
        return std::move(out);
    }

    inline IOResult<void> WriteAllToStream(Stream::IStream& s, Base::ConstSpan<std::byte> data) {
        std::size_t offset = 0;
        while (offset < data.size()) {
            const std::size_t remain = data.size() - offset;
            auto wr = s.Write(data.data() + offset, remain);
            if (!wr) return Base::Err(wr.error());
            const std::size_t n = wr.value();
            if (n == 0) {
                return Base::Err(MakeErr(IOErrorCode::WriteFailed, "WriteAllToStream: zero write"));
            }
            offset += n;
        }
        return {};
    }

    /// Removes a UTF-8 byte order mark (EF BB BF).
    inline void StripUtf8Bom(std::string& s) {
        if (s.size() >= 3) {
            const unsigned char b0 = static_cast<unsigned char>(s[0]);
            const unsigned char b1 = static_cast<unsigned char>(s[1]);
            const unsigned char b2 = static_cast<unsigned char>(s[2]);
            if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) {
                s.erase(0, 3);
            }
        }
    }

} // namespace GYO::Engine::IO::Helpers
