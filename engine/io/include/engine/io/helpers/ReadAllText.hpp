#pragma once

#include "engine/io/helpers/FileAllCommon.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"

/**
 * UTF-8 前提
 */
namespace Engine::IO::Helpers {



    inline IoResult<std::string>
    ReadAllTextUtf8(Engine::IO::FS::IFileSystem& fs, const Engine::IO::Path::Uri& uri,
                    const ReadAllOptions& opt = {}) {
        auto br = ReadAllBytes(fs, uri, opt);
        if (!br) return IoResult<std::string>::Err(br.error());

        const auto& bytes = br.value();
        std::string s;
        s.resize(bytes.size());
        if (!bytes.empty()) {
            std::memcpy(s.data(), bytes.data(), bytes.size());
        }
        StripUtf8Bom(s);
        return IoResult<std::string>::Ok(std::move(s));
    }

    inline IoResult<std::string>
    ReadAllTextUtf8(Engine::IO::FS::Vfs& vfs, const Engine::IO::Path::Uri& uri,
                    const ReadAllOptions& opt = {}) {
        auto br = ReadAllBytes(vfs, uri, opt);
        if (!br) return IoResult<std::string>::Err(br.error());

        const auto& bytes = br.value();
        std::string s;
        s.resize(bytes.size());
        if (!bytes.empty()) {
            std::memcpy(s.data(), bytes.data(), bytes.size());
        }
        StripUtf8Bom(s);
        return IoResult<std::string>::Ok(std::move(s));
    }

} // namespace Engine::IO::Helpers
