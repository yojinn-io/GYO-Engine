#pragma once

#include "engine/io/helpers/FileAllCommon.hpp"

namespace GYO::Engine::IO::Helpers {

    inline IOResult<void>
    WriteAllBytes(FS::IFileSystem& fs, const Path::Uri& uri,
                  Base::ConstSpan<std::byte> data,
                  const WriteAllOptions& opt = {}) {
        auto orw = OpenWriteTruncate(fs, uri);
        if (!orw) return Base::Err(orw.error());

        auto& s = *orw.value();
        auto wr = WriteAllToStream(s, data);
        if (!wr) return Base::Err(wr.error());

        if (opt.flush) {
            auto fr = s.Flush();
            if (!fr) return Base::Err(fr.error());
        }

        (void)s.Close();
        return {};
    }

    inline IOResult<void>
    WriteAllBytes(FS::Vfs& vfs, const Path::Uri& uri,
                  Base::ConstSpan<std::byte> data,
                  const WriteAllOptions& opt = {}) {
        auto orw = OpenWriteTruncate(vfs, uri);
        if (!orw) return Base::Err(orw.error());

        auto& s = *orw.value();
        auto wr = WriteAllToStream(s, data);
        if (!wr) return Base::Err(wr.error());

        if (opt.flush) {
            auto fr = s.Flush();
            if (!fr) return Base::Err(fr.error());
        }

        (void)s.Close();
        return {};
    }

} // namespace GYO::Engine::IO::Helpers
