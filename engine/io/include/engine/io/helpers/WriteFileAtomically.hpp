#pragma once

#include "engine/io/helpers/FileAllCommon.hpp"

namespace Engine::IO::Helpers {

    /// Writes bytes to uri so that a reader of uri sees its old file or all of the new bytes, never a part
    /// (docs/architecture/io.md). The bytes go to a temporary file beside it, "<name>.tmp.<time>.<random>",
    /// which is closed and then moved over uri. The file system's Move must replace an existing target in one
    /// step, as the native backend's does (Os::Files::RenameReplacing). When a step fails the temporary file is
    /// removed and that step's error returned; uri keeps its old file. The parent directory must exist.
    IoResult<void> WriteFileAtomically(Engine::IO::FS::IFileSystem& fs, const Engine::IO::Path::Uri& uri,
                                       Engine::Base::ConstSpan<std::byte> data);

} // namespace Engine::IO::Helpers
