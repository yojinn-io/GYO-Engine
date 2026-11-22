#pragma once

#include "engine/io/helpers/WriteAllBytes.hpp"

/**
 * UTF-8 として書く
 */
namespace GYO::Engine::IO::Helpers {

    inline IOResult<void>
    WriteAllTextUtf8(FS::IFileSystem& fs, const Path::Uri& uri,
                     std::string_view text,
                     const WriteAllOptions& opt = {}) {
        const auto* p = reinterpret_cast<const std::byte*>(text.data());
        Base::ConstSpan<std::byte> span(p, text.size());
        return WriteAllBytes(fs, uri, span, opt);
    }

    inline IOResult<void>
    WriteAllTextUtf8(FS::Vfs& vfs, const Path::Uri& uri,
                     std::string_view text,
                     const WriteAllOptions& opt = {}) {
        const auto* p = reinterpret_cast<const std::byte*>(text.data());
        Base::ConstSpan<std::byte> span(p, text.size());
        return WriteAllBytes(vfs, uri, span, opt);
    }

} // namespace GYO::Engine::IO::Helpers
