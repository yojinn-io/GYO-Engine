#pragma once

#include "engine/io/helpers/FileAllCommon.hpp"

namespace Engine::IO::Helpers {

    inline IoResult<void>
    WriteAllBytes(Engine::IO::FS::IFileSystem& fs, const Engine::IO::Path::Uri& uri,
                  Engine::Base::ConstSpan<std::byte> data,
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

    inline IoResult<void>
    WriteAllBytes(Engine::IO::FS::Vfs& vfs, const Engine::IO::Path::Uri& uri,
                  Engine::Base::ConstSpan<std::byte> data,
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

} // namespace Engine::IO::Helpers
