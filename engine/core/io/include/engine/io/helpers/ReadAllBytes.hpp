#pragma once

#include "engine/io/helpers/FileAllCommon.hpp"

namespace GYO::Engine::IO::Helpers {

    /// The whole file at uri. More than opt.maxBytes is TooLarge.
    inline IOResult<std::vector<std::byte>> ReadAllBytes(FS::IFileSystem& fs, const Path::Uri& uri,
                                                         const ReadAllOptions& opt = {}) {
        auto orr = OpenRead(fs, uri);
        if (!orr) return Base::Err(orr.error());
        auto& s = *orr.value();
        auto br = ReadAllFromStream(s, opt);
        if (!br) return Base::Err(br.error());
        (void)s.Close();
        return std::move(br.value());
    }

} // namespace GYO::Engine::IO::Helpers
