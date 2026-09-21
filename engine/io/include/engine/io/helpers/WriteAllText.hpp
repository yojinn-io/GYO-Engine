#pragma once

#include "engine/io/helpers/WriteAllBytes.hpp"

/**
 * UTF-8 として書く
 */
namespace Engine::IO::Helpers {

    inline IoResultVoid
    WriteAllTextUtf8(Engine::IO::FS::IFileSystem& fs, const Engine::IO::Path::Uri& uri,
                     std::string_view text,
                     const WriteAllOptions& opt = {}) {
        const auto* p = reinterpret_cast<const std::byte*>(text.data());
        Engine::Base::ConstSpan<std::byte> span(p, text.size());
        return WriteAllBytes(fs, uri, span, opt);
    }

    inline IoResultVoid
    WriteAllTextUtf8(Engine::IO::FS::Vfs& vfs, const Engine::IO::Path::Uri& uri,
                     std::string_view text,
                     const WriteAllOptions& opt = {}) {
        const auto* p = reinterpret_cast<const std::byte*>(text.data());
        Engine::Base::ConstSpan<std::byte> span(p, text.size());
        return WriteAllBytes(vfs, uri, span, opt);
    }

} // namespace Engine::IO::Helpers
