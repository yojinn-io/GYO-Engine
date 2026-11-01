#pragma once

#include <cstdint>

#include "engine/io/helpers/FileAllCommon.hpp"

namespace Engine::IO::Helpers {

    /// What a file looked like when probed, to tell whether it changed since (docs/architecture/io.md,
    /// "User files").
    struct FileStamp final {
        bool exists = false;
        std::uint64_t sizeBytes = 0;
        /// As the file system reports it; 0 when it keeps none.
        FS::TimeNs mtimeNs = 0;
        /// Base::Fnv1a64 of the bytes.
        std::uint64_t contentHash = 0;

        friend bool operator==(const FileStamp&, const FileStamp&) noexcept = default;
    };

    /// The stamp of the file at uri, reading it whole for the hash. A missing file is a success with
    /// exists == false; anything that is not a regular file is InvalidPath; a file larger than read.maxBytes
    /// fails as ReadAllBytes does, so a caller that means "any size" says so.
    IoResult<FileStamp> ProbeFileStamp(FS::IFileSystem& fs, const Path::Uri& uri, const ReadAllOptions& read);

} // namespace Engine::IO::Helpers
