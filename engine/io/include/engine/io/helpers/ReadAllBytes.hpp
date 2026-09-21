#pragma once

#include "engine/io/helpers/FileAllCommon.hpp"
#include "engine/io/stream/IStream.hpp"

namespace Engine::IO::Helpers {


    // IFileSystem 版
    inline IoResult<std::vector<std::byte>>
    ReadAllBytes(Engine::IO::FS::IFileSystem& fs, const Engine::IO::Path::Uri& uri,
                 const ReadAllOptions& opt = {}) {
        auto orr = OpenRead(fs, uri);
        if (!orr) return IoResult<std::vector<std::byte>>::Err(orr.error());

        auto& s = *orr.value();
        auto br = ReadAllFromStream(s, opt);
        if (!br) return IoResult<std::vector<std::byte>>::Err(br.error());

        (void)s.Close();
        return IoResult<std::vector<std::byte>>::Ok(std::move(br.value()));
    }

    // Vfs 版（必要なければ削除）
    inline IoResult<std::vector<std::byte>>
    ReadAllBytes(Engine::IO::FS::Vfs& vfs, const Engine::IO::Path::Uri& uri,
                 const ReadAllOptions& opt = {}) {
        auto orr = OpenRead(vfs, uri);
        if (!orr) return IoResult<std::vector<std::byte>>::Err(orr.error());

        auto& s = *orr.value();
        auto br = ReadAllFromStream(s, opt);
        if (!br) return IoResult<std::vector<std::byte>>::Err(br.error());

        (void)s.Close();
        return IoResult<std::vector<std::byte>>::Ok(std::move(br.value()));
    }

} // namespace Engine::IO::Helpers
