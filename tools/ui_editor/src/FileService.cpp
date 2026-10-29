#include "gyo/ui_editor/FileService.hpp"

#include "engine/base/Fnv1a.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/FileAllCommon.hpp"
#include "engine/io/helpers/WriteFileAtomically.hpp"
#include "engine/os/Files.hpp"

#include <chrono>
#include <cstddef>
#include <limits>
#include <utility>

// Files go through engine/io's native backend and path queries through engine/os
// (docs/architecture/os.md, Design Invariant 2); the editor holds no OS code.

namespace Gyo::Tools::UiEditor {
namespace {

namespace Io = Engine::IO;
namespace OsFiles = Engine::Os::Files;

// The path in UTF-8 for messages; engine paths are UTF-8, not path::string()'s ANSI code page.
[[nodiscard]] std::string Text(const std::filesystem::path& path) {
    auto text = OsFiles::PathToUtf8(path);
    return text ? std::move(text.value()) : std::string("<path with no UTF-8 form>");
}

[[nodiscard]] FileError Failure(const FileErrorCode code, std::string message, std::string detail) {
    return FileError::Make(code, std::move(message), std::move(detail));
}

// The file URI of a path for the native backend; a relative path is made absolute as the OS would.
[[nodiscard]] Io::IoResult<Io::Path::Uri> NativeUri(const std::filesystem::path& path) {
    auto text = OsFiles::PathToUtf8(path);
    if (!text) {
        return Engine::Base::Err(Io::IoError::Make(
            Io::IoErrorCode::InvalidPath, text.error().message, Engine::Base::CauseDetail(text.error())));
    }
    return Io::FS::NativeFileUri(text.value());
}

[[nodiscard]] std::filesystem::path FromUtf8(const std::string& text, const std::filesystem::path& otherwise) {
    auto path = OsFiles::PathFromUtf8(text);
    return path ? std::move(path.value()) : otherwise;
}

[[nodiscard]] std::filesystem::path AbsoluteNormalized(
    const std::filesystem::path& path) {
    const std::filesystem::path absolute = AbsolutePath(path);
    const std::filesystem::path parent = absolute.parent_path();
    // Links in the existing part of the parent are resolved, as std::filesystem::weakly_canonical did.
    std::filesystem::path canonicalParent = parent.lexically_normal();
    if (auto text = OsFiles::PathToUtf8(parent)) {
        if (auto resolved = OsFiles::ResolvedPath(text.value())) {
            canonicalParent = FromUtf8(resolved.value(), canonicalParent);
        }
    }
    return (canonicalParent / absolute.filename()).lexically_normal();
}

[[nodiscard]] bool ComponentEqual(
    const std::filesystem::path& left,
    const std::filesystem::path& right) {
    auto leftText = OsFiles::PathToUtf8(left);
    auto rightText = OsFiles::PathToUtf8(right);
    if (!leftText || !rightText) {
        return left == right;
    }
    auto same = OsFiles::SamePathComponent(leftText.value(), rightText.value());
    return same && same.value();
}

} // namespace

Result<std::string, FileError> ReadTextFile(const std::filesystem::path& path) {
    auto uri = NativeUri(path);
    if (!uri) {
        return Engine::Base::Err(Failure(FileErrorCode::ReadFailed, "cannot open file: " + Text(path),
                                         Engine::Base::CauseDetail(uri.error())));
    }
    Io::FS::NativeFileSystem files;
    auto stream = Io::Helpers::OpenRead(files, uri.value());
    if (!stream) {
        return Engine::Base::Err(Failure(FileErrorCode::ReadFailed, "cannot open file: " + Text(path),
                                         Engine::Base::CauseDetail(stream.error())));
    }
    // No size limit: the editor read whole files with a C++ stream before (Design Invariant 9).
    Io::Helpers::ReadAllOptions whole;
    whole.maxBytes = (std::numeric_limits<std::size_t>::max)();
    auto bytes = Io::Helpers::ReadAllFromStream(*stream.value(), whole);
    if (!bytes) {
        return Engine::Base::Err(Failure(FileErrorCode::ReadFailed, "failed while reading file: " + Text(path),
                                         Engine::Base::CauseDetail(bytes.error())));
    }
    const auto& data = bytes.value();
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

Result<bool, FileError> PathExists(const std::filesystem::path& path) {
    auto uri = NativeUri(path);
    if (!uri) {
        return Engine::Base::Err(Failure(FileErrorCode::ProbeFailed, "cannot inspect '" + Text(path) + "'",
                                         Engine::Base::CauseDetail(uri.error())));
    }
    Io::FS::NativeFileSystem files;
    auto exists = files.Exists(uri.value());
    if (!exists) {
        return Engine::Base::Err(Failure(FileErrorCode::ProbeFailed, "cannot inspect '" + Text(path) + "'",
                                         Engine::Base::CauseDetail(exists.error())));
    }
    return exists.value();
}

Result<FileStamp, FileError> ProbeFileStamp(const std::filesystem::path& path) {
    auto uri = NativeUri(path);
    if (!uri) {
        return Engine::Base::Err(Failure(FileErrorCode::ProbeFailed, "cannot inspect '" + Text(path) + "'",
                                         Engine::Base::CauseDetail(uri.error())));
    }
    Io::FS::NativeFileSystem files;
    auto info = files.Stat(uri.value());
    if (!info) {
        if (info.error().code == Io::IoErrorCode::NotFound) {
            return FileStamp{};
        }
        return Engine::Base::Err(Failure(FileErrorCode::ProbeFailed, "cannot inspect '" + Text(path) + "'",
                                         Engine::Base::CauseDetail(info.error())));
    }
    if (!info.value().IsFile()) {
        return Engine::Base::Err(Failure(FileErrorCode::ProbeFailed,
            "path is not a regular file: " + Text(path), Text(path)));
    }

    auto text = ReadTextFile(path);
    if (!text) {
        return Engine::Base::Err(std::move(text).error());
    }
    FileStamp stamp;
    stamp.exists = true;
    stamp.size = info.value().sizeBytes;
    stamp.writeTime = std::chrono::nanoseconds(info.value().mtimeNs);
    stamp.contentHash = Engine::Base::Fnv1a64(*text);
    return stamp;
}

Result<void, FileError> WriteTextFileAtomically(
    const std::filesystem::path& path,
    const std::string_view text) {
    if (path.empty()) {
        return Engine::Base::Err(Failure(FileErrorCode::WriteFailed, "output path is empty", {}));
    }

    Io::FS::NativeFileSystem files;
    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        auto parentUri = NativeUri(parent);
        auto created = parentUri ? files.CreateDirectories(parentUri.value())
                                 : Io::IoResult<void>(Engine::Base::Err(parentUri.error()));
        if (!created) {
            return Engine::Base::Err(Failure(FileErrorCode::WriteFailed,
                "cannot create output directory '" + Text(parent) + "'", Engine::Base::CauseDetail(created.error())));
        }
    }

    auto uri = NativeUri(path);
    if (!uri) {
        return Engine::Base::Err(Failure(FileErrorCode::WriteFailed, "cannot write output '" + Text(path) + "'",
                                         Engine::Base::CauseDetail(uri.error())));
    }
    const Engine::Base::ConstSpan<std::byte> bytes(reinterpret_cast<const std::byte*>(text.data()), text.size());
    auto written = Io::Helpers::WriteFileAtomically(files, uri.value(), bytes);
    if (!written) {
        return Engine::Base::Err(Failure(FileErrorCode::WriteFailed, "cannot write output '" + Text(path) + "'",
                                         Engine::Base::CauseDetail(written.error())));
    }
    return {};
}

std::filesystem::path AbsolutePath(const std::filesystem::path& path) {
    auto text = OsFiles::PathToUtf8(path);
    if (!text || text.value().empty()) {
        return path.lexically_normal();
    }
    auto absolute = OsFiles::MakeAbsolute(text.value());
    if (!absolute) {
        return path.lexically_normal();
    }
    return FromUtf8(absolute.value(), path).lexically_normal();
}

bool IsPathWithin(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root) {
    if (candidate.empty() || root.empty()) {
        return false;
    }
    const std::filesystem::path normalizedCandidate =
        AbsoluteNormalized(candidate);
    const std::filesystem::path normalizedRoot = AbsoluteNormalized(root);

    auto candidatePart = normalizedCandidate.begin();
    auto rootPart = normalizedRoot.begin();
    for (; rootPart != normalizedRoot.end(); ++rootPart, ++candidatePart) {
        if (candidatePart == normalizedCandidate.end() ||
            !ComponentEqual(*candidatePart, *rootPart)) {
            return false;
        }
    }
    return true;
}

} // namespace Gyo::Tools::UiEditor
