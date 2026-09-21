#pragma once

#include "engine/io/helpers/FileAllCommon.hpp"

namespace Engine::IO::Helpers {

    inline IoResultVoid
    WriteAllBytes(Engine::IO::FS::IFileSystem& fs, const Engine::IO::Path::Uri& uri,
                  Engine::Base::ConstSpan<std::byte> data,
                  const WriteAllOptions& opt = {}) {
        auto orw = OpenWriteTruncate(fs, uri);
        if (!orw) return IoResultVoid::Err(orw.error());

        auto& s = *orw.value();
        auto wr = WriteAllToStream(s, data);
        if (!wr) return IoResultVoid::Err(wr.error());

        if (opt.flush) {
            auto fr = s.Flush();
            if (!fr) return IoResultVoid::Err(fr.error());
        }

        (void)s.Close();
        return IoResultVoid::Ok();
    }

    inline IoResultVoid
    WriteAllBytes(Engine::IO::FS::Vfs& vfs, const Engine::IO::Path::Uri& uri,
                  Engine::Base::ConstSpan<std::byte> data,
                  const WriteAllOptions& opt = {}) {
        auto orw = OpenWriteTruncate(vfs, uri);
        if (!orw) return IoResultVoid::Err(orw.error());

        auto& s = *orw.value();
        auto wr = WriteAllToStream(s, data);
        if (!wr) return IoResultVoid::Err(wr.error());

        if (opt.flush) {
            auto fr = s.Flush();
            if (!fr) return IoResultVoid::Err(fr.error());
        }

        (void)s.Close();
        return IoResultVoid::Ok();
    }

} // namespace Engine::IO::Helpers
