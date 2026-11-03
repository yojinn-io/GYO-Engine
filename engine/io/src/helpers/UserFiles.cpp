#include "engine/io/helpers/UserFiles.hpp"

#include <string_view>
#include <utility>

#include "engine/base/Fnv1a.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"

namespace Engine::IO::Helpers {

    IoResult<FileStamp> ProbeFileStamp(FS::IFileSystem& fs, const Path::Uri& uri, const ReadAllOptions& read) {
        auto info = fs.Stat(uri);
        if (!info) {
            if (info.error().code == IoErrorCode::NotFound) return FileStamp{};
            return Base::Err(std::move(info.error()));
        }
        // Refused before opening: opening a pipe, a device or a terminal has effects of its own.
        if (!info.value().IsFile()) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "not a regular file", uri.ToString()));
        }
        auto bytes = ReadAllBytes(fs, uri, read);
        if (!bytes) return Base::Err(std::move(bytes.error()));
        const auto& data = bytes.value();
        FileStamp stamp;
        stamp.exists = true;
        stamp.sizeBytes = info.value().sizeBytes;
        stamp.mtimeNs = info.value().mtimeNs;
        stamp.contentHash =
            Base::Fnv1a64(std::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
        return stamp;
    }

} // namespace Engine::IO::Helpers
