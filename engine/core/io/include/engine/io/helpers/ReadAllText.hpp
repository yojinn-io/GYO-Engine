#pragma once

#include "engine/io/helpers/FileAllCommon.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"

namespace GYO::Engine::IO::Helpers {

    /// The whole file at uri as UTF-8 text, without a byte order mark. More than opt.maxBytes is TooLarge.
    inline IOResult<std::string> ReadAllTextUtf8(FS::IFileSystem& fs, const Path::Uri& uri,
                                                 const ReadAllOptions& opt = {}) {
        auto br = ReadAllBytes(fs, uri, opt);
        if (!br) return Base::Err(br.error());
        const auto& bytes = br.value();
        std::string s;
        s.resize(bytes.size());
        if (!bytes.empty()) {
            std::memcpy(s.data(), bytes.data(), bytes.size());
        }
        StripUtf8Bom(s);
        return std::move(s);
    }

} // namespace GYO::Engine::IO::Helpers
