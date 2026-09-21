#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/base/Span.hpp"

#include "engine/io/IoError.hpp"
#include "engine/io/IoFwd.hpp"                // Path::Uri, Stream::IStream fwd
#include "engine/io/fs/IFileSystem.hpp"       // IFileSystem
#include "engine/io/fs/Vfs.hpp"              // Vfs（使わないなら外してOK）
#include "engine/io/stream/FileOpenMode.hpp"  // FileOpenMode, Has/IsValid
#include "engine/io/stream/IStream.hpp"

namespace Engine::IO::Helpers {

    using IoError = Engine::Base::Error<Engine::IO::IoErrorCode>;
    template<class T>
    using IoResult = Engine::Base::Result<T, IoError>;
    using IoResultVoid = Engine::Base::Result<void, IoError>;

    struct ReadAllOptions final {
        std::size_t maxBytes = 64u * 1024u * 1024u; // 安全上限（64MB）
        std::size_t chunkBytes = 64u * 1024u;       // 64KB
        bool tryUseSizeHint = true;                 // stream.Size() を試す
    };

    struct WriteAllOptions final {
        bool flush = true;
    };

    inline IoError MakeErr(Engine::IO::IoErrorCode code, const char* msg, std::string detail = {}) {
        return IoError::Make(code, msg, std::move(detail));
    }

    inline IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>
    OpenRead(Engine::IO::FS::IFileSystem& fs, const Engine::IO::Path::Uri& uri) {
        using Engine::IO::Stream::FileOpenMode;
        auto mode = FileOpenMode::Read | FileOpenMode::Binary;
        return fs.Open(uri, mode);
    }

    inline IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>
    OpenWriteTruncate(Engine::IO::FS::IFileSystem& fs, const Engine::IO::Path::Uri& uri) {
        using Engine::IO::Stream::FileOpenMode;
        auto mode = FileOpenMode::Write | FileOpenMode::Binary | FileOpenMode::Truncate | FileOpenMode::CreateIfMissing;
        return fs.Open(uri, mode);
    }

    // VFS overload（必要ないなら削ってOK）
    inline IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>
    OpenRead(Engine::IO::FS::Vfs& vfs, const Engine::IO::Path::Uri& uri) {
        using Engine::IO::Stream::FileOpenMode;
        auto mode = FileOpenMode::Read | FileOpenMode::Binary;
        return vfs.Open(uri, mode);
    }
    inline IoResult<std::unique_ptr<Engine::IO::Stream::IStream>>
    OpenWriteTruncate(Engine::IO::FS::Vfs& vfs, const Engine::IO::Path::Uri& uri) {
        using Engine::IO::Stream::FileOpenMode;
        auto mode = FileOpenMode::Write | FileOpenMode::Binary | FileOpenMode::Truncate | FileOpenMode::CreateIfMissing;
        return vfs.Open(uri, mode);
    }

    inline IoResult<std::vector<std::byte>>
    ReadAllFromStream(Engine::IO::Stream::IStream& s, const ReadAllOptions& opt) {
        std::vector<std::byte> out;

        // size hint（取れれば reserve）
        if (opt.tryUseSizeHint) {
            auto szr = s.Size();
            if (szr) {
                const auto sz = static_cast<std::size_t>(szr.value());
                if (sz > opt.maxBytes) {
                    return IoResult<std::vector<std::byte>>::Err(
                        MakeErr(Engine::IO::IoErrorCode::ReadFailed, "ReadAllBytes: exceeds maxBytes"));
                }
                out.reserve(sz);
            }
        }

        std::vector<std::byte> buf(opt.chunkBytes);

        while (true) {
            auto rr = s.Read(buf.data(), buf.size());
            if (!rr) return IoResult<std::vector<std::byte>>::Err(rr.error());

            const std::size_t n = rr.value();
            if (n == 0) break;

            if (out.size() + n > opt.maxBytes) {
                return IoResult<std::vector<std::byte>>::Err(
                    MakeErr(Engine::IO::IoErrorCode::ReadFailed, "ReadAllBytes: exceeds maxBytes"));
            }

            const std::size_t old = out.size();
            out.resize(old + n);
            std::memcpy(out.data() + old, buf.data(), n);
        }

        return IoResult<std::vector<std::byte>>::Ok(std::move(out));
    }

    inline IoResultVoid
    WriteAllToStream(Engine::IO::Stream::IStream& s, Engine::Base::ConstSpan<std::byte> data) {
        std::size_t offset = 0;
        while (offset < data.size()) {
            const std::size_t remain = data.size() - offset;
            auto wr = s.Write(data.data() + offset, remain);
            if (!wr) return IoResultVoid::Err(wr.error());

            const std::size_t n = wr.value();
            if (n == 0) {
                return IoResultVoid::Err(MakeErr(
                    Engine::IO::IoErrorCode::WriteFailed, "WriteAllBytes: zero write"));
            }
            offset += n;
        }
        return IoResultVoid::Ok();
    }

    // UTF-8 BOM (EF BB BF) を除去
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

} // namespace Engine::IO::Helpers::detail
