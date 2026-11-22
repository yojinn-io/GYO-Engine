#include "engine/asset/ReadWholeFile.hpp"

#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/stream/FileOpenMode.hpp"
#include "engine/io/stream/IStream.hpp"

namespace GYO::Engine::Asset {
namespace {

FileReadFailure FromIO(const IO::IOError& error, const std::string_view path, const char* message) {
    const FileReadFailureCode code =
        error.code == IO::IOErrorCode::NotFound ? FileReadFailureCode::NotFound : FileReadFailureCode::ReadFailed;
    return FileReadFailure::Make(code, message, Base::CauseDetail(error, path));
}

FileReadFailure TooLarge(const std::string_view path, const std::size_t maxBytes) {
    return FileReadFailure::Make(FileReadFailureCode::TooLarge,
                                 "the file has more than " + std::to_string(maxBytes) + " bytes", std::string(path));
}

} // namespace

Base::Result<std::vector<std::byte>, FileReadFailure> ReadWholeFile(const std::string_view nativePath,
                                                                    const std::size_t maxBytes) {
    auto uri = IO::FS::NativeFileUri(nativePath);
    if (!uri) return Base::Err(FromIO(uri.error(), nativePath, "the path cannot name a file"));
    IO::FS::NativeFileSystem files;
    auto opened = files.Open(*uri, IO::Stream::FileOpenMode::Read | IO::Stream::FileOpenMode::Binary);
    if (!opened) return Base::Err(FromIO(opened.error(), nativePath, "the file cannot be opened"));
    IO::Stream::IStream& stream = **opened;

    std::vector<std::byte> bytes;
    if (auto size = stream.Size()) {
        if (*size > maxBytes) return Base::Err(TooLarge(nativePath, maxBytes));
        bytes.reserve(static_cast<std::size_t>(*size));
    }
    std::array<std::byte, 64 * 1024> chunk{};
    while (true) {
        auto read = stream.Read(chunk.data(), chunk.size());
        if (!read) return Base::Err(FromIO(read.error(), nativePath, "the file cannot be read"));
        const std::size_t count = *read;
        if (count == 0) break;
        if (count > maxBytes - bytes.size()) return Base::Err(TooLarge(nativePath, maxBytes));
        const std::size_t old = bytes.size();
        bytes.resize(old + count);
        std::memcpy(bytes.data() + old, chunk.data(), count);
    }
    (void)stream.Close();
    return bytes;
}

} // namespace GYO::Engine::Asset
