#pragma once

#include "engine/io/helpers/WriteAllBytes.hpp"

/**
 * UTF-8 として書く
 */
namespace GYO::Engine::IO::Helpers {

    inline IoResult<void>
    WriteAllTextUtf8(GYO::Engine::IO::FS::IFileSystem& fs, const GYO::Engine::IO::Path::Uri& uri,
                     std::string_view text,
                     const WriteAllOptions& opt = {}) {
        const auto* p = reinterpret_cast<const std::byte*>(text.data());
        GYO::Engine::Base::ConstSpan<std::byte> span(p, text.size());
        return WriteAllBytes(fs, uri, span, opt);
    }

    inline IoResult<void>
    WriteAllTextUtf8(GYO::Engine::IO::FS::Vfs& vfs, const GYO::Engine::IO::Path::Uri& uri,
                     std::string_view text,
                     const WriteAllOptions& opt = {}) {
        const auto* p = reinterpret_cast<const std::byte*>(text.data());
        GYO::Engine::Base::ConstSpan<std::byte> span(p, text.size());
        return WriteAllBytes(vfs, uri, span, opt);
    }

} // namespace GYO::Engine::IO::Helpers
