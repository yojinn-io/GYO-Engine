#include "doctest/doctest.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/fs/Vfs.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/path/Uri.hpp"

using namespace Engine::IO;

// engine/io's file:// backend over engine/os. Files are set up with the
// standard library; the backend is reached through file URIs only.

namespace {

class TempTree final {
public:
    TempTree() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() / ("gyo-native-fs-" + std::to_string(stamp));
        std::filesystem::create_directories(root_);
    }
    TempTree(const TempTree&) = delete;
    TempTree& operator=(const TempTree&) = delete;
    ~TempTree() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    void Write(const std::string& relative, const std::string& bytes) const {
        const auto file = root_ / relative;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    // The native path in UTF-8 '/' form.
    std::string Native(const std::string& relative = {}) const {
        const auto path = relative.empty() ? root_ : root_ / relative;
        const std::u8string text = path.generic_u8string();
        return std::string(text.begin(), text.end());
    }

    Path::Uri Uri(const std::string& relative = {}) const {
        auto uri = Path::FileUriFromNativePath(Native(relative));
        REQUIRE(uri);
        return uri.value();
    }

private:
    std::filesystem::path root_;
};

IoErrorCode OpenError(FS::NativeFileSystem& fs, const Path::Uri& uri, const Stream::FileOpenMode mode) {
    auto opened = fs.Open(uri, mode);
    REQUIRE_MESSAGE(!opened, "NativeFileSystem opened mode " << static_cast<unsigned>(mode));
    return opened.error().code;
}

std::string AsText(const std::vector<std::byte>& bytes) {
    std::string out;
    for (const std::byte b : bytes) out.push_back(static_cast<char>(b));
    return out;
}

} // namespace

TEST_CASE("NativeFileSystem reads a file named by a file URI") {
    const TempTree tree;
    tree.Write("textures/a.bin", "hello world");
    FS::NativeFileSystem fs;
    const auto bytes = Helpers::ReadAllBytes(fs, tree.Uri("textures/a.bin"));
    REQUIRE(bytes);
    CHECK(AsText(bytes.value()) == "hello world");
}

// A '..' after a symbolic link reaches the OS as written (docs/architecture/io.md
// Invariant 3, D54), so the backend opens the file the OS itself opens for the
// path: on POSIX the one beside the link's target ("through the link"), on
// Windows the lexical one, as GetFullPathNameW removes the '..' first. A
// lexical rule in engine/io would read "lexical" on POSIX too.
TEST_CASE("NativeFileSystem opens a '..' after a symbolic link as the OS does") {
    const TempTree tree;
    tree.Write("real/target.txt", "through the link");
    tree.Write("real/sub/keep.txt", "");
    tree.Write("target.txt", "lexical");
    const auto native = [&tree](const std::string& relative) {
        const std::string text = tree.Native(relative);
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    };
    std::error_code error;
    std::filesystem::create_directory_symlink(native("real/sub"), native("link"), error);
    if (error) {
        // Windows needs Developer Mode or a privilege to create links (as in tests/common/os/FilesTests.cpp).
        MESSAGE("symbolic links unavailable here: " << error.message());
        return;
    }
    const std::string path = tree.Native("link/../target.txt");
    std::ifstream reference(native("link/../target.txt"), std::ios::binary);
    REQUIRE(reference);
    const std::string expected{std::istreambuf_iterator<char>(reference), std::istreambuf_iterator<char>()};
    MESSAGE("the OS reads \"" << expected << "\"");

    auto uri = FS::NativeFileUri(path);
    REQUIRE(uri);
    CHECK(uri.value().ToString().find("/link/../target.txt") != std::string::npos);
    FS::NativeFileSystem fs;
    const auto bytes = Helpers::ReadAllBytes(fs, uri.value());
    REQUIRE(bytes);
    CHECK(AsText(bytes.value()) == expected);
    const auto status = fs.Stat(uri.value());
    REQUIRE(status);
    CHECK(status.value().sizeBytes == expected.size());
}

TEST_CASE("NativeFileSystem streams are readable and seekable, not writable") {
    const TempTree tree;
    tree.Write("b.bin", "0123456789");
    FS::NativeFileSystem fs;
    auto opened = fs.Open(tree.Uri("b.bin"), Stream::OpenReadBinary());
    REQUIRE(opened);
    Stream::IStream& stream = *opened.value();
    CHECK(stream.Caps().readable);
    CHECK(stream.Caps().seekable);
    CHECK_FALSE(stream.Caps().writable);
    CHECK(stream.Size().value() == 10);
    CHECK(stream.Seek(-3, Stream::SeekWhence::End).value() == 7);
    char tail[8]{};
    CHECK(stream.Read(tail, sizeof tail).value() == 3);
    CHECK(std::string(tail, 3) == "789");
    CHECK(stream.Read(tail, sizeof tail).value() == 0);
    CHECK(stream.IsEof());
    CHECK(stream.Seek(2, Stream::SeekWhence::Begin).value() == 2);
    CHECK(stream.Seek(1, Stream::SeekWhence::Current).value() == 3);
    CHECK(stream.Tell().value() == 3);
    CHECK_FALSE(stream.IsEof());
    // Like the other read-only streams, a target outside [0, Size()] fails, INT64_MIN included.
    CHECK(stream.Seek(-4, Stream::SeekWhence::Current).error().code == IoErrorCode::SeekFailed);
    CHECK(stream.Seek(11, Stream::SeekWhence::Begin).error().code == IoErrorCode::SeekFailed);
    CHECK(stream.Seek(1, Stream::SeekWhence::End).error().code == IoErrorCode::SeekFailed);
    for (const auto whence : {Stream::SeekWhence::Begin, Stream::SeekWhence::Current, Stream::SeekWhence::End}) {
        CHECK(stream.Seek((std::numeric_limits<std::int64_t>::min)(), whence).error().code ==
              IoErrorCode::SeekFailed);
    }
    CHECK(stream.Tell().value() == 3);
    CHECK(stream.Seek(0, Stream::SeekWhence::End).value() == 10);
    // A read of zero bytes is not the end of the file.
    CHECK(stream.Seek(0, Stream::SeekWhence::Begin).value() == 0);
    CHECK(stream.Read(tail, 0).value() == 0);
    CHECK_FALSE(stream.IsEof());
    CHECK(stream.Write("x", 1).error().code == IoErrorCode::NotSupported);
    REQUIRE(stream.Close());
    CHECK_FALSE(stream.IsOpen());
    CHECK(stream.Read(tail, 1).error().code == IoErrorCode::ReadFailed);
    CHECK(stream.Tell().error().code == IoErrorCode::SeekFailed);
    CHECK(stream.Seek(0, Stream::SeekWhence::Begin).error().code == IoErrorCode::SeekFailed);
}

TEST_CASE("NativeFileSystem answers Exists and Stat") {
    const TempTree tree;
    tree.Write("c/d.bin", "12345");
    FS::NativeFileSystem fs;
    CHECK(fs.Exists(tree.Uri("c/d.bin")).value());
    CHECK_FALSE(fs.Exists(tree.Uri("c/missing.bin")).value());
    const auto info = fs.Stat(tree.Uri("c/d.bin"));
    REQUIRE(info);
    CHECK(info.value().IsFile());
    CHECK(info.value().sizeBytes == 5);
    CHECK(info.value().mtimeNs > 0);
    CHECK(info.value().backend == "file");
    CHECK(fs.Stat(tree.Uri("c")).value().IsDirectory());
}

TEST_CASE("NativeFileSystem keeps the engine/os cause of a failure") {
    const TempTree tree;
    FS::NativeFileSystem fs;
    const std::string missing = tree.Native("missing.bin");
    auto opened = fs.Open(tree.Uri("missing.bin"), Stream::OpenReadBinary());
    REQUIRE_FALSE(opened);
    CHECK(opened.error().code == IoErrorCode::NotFound);
    CHECK(opened.error().message == "cannot open file");
    // "<native path>; NotFound: errno=<n>; path_bytes=<n>" (win32= on Windows).
    const std::string expectedStart = missing + "; NotFound: ";
    CHECK(opened.error().detail.starts_with(expectedStart));
    CHECK(opened.error().detail.ends_with("; path_bytes=" + std::to_string(missing.size())));
    CHECK(fs.Stat(tree.Uri("missing.bin")).error().code == IoErrorCode::NotFound);
}

TEST_CASE("NativeFileSystem maps the other engine/os failures") {
    const TempTree tree;
    tree.Write("dir/h.bin", "x");
    FS::NativeFileSystem fs;
    // A directory is not a file (engine/os InvalidPath).
    CHECK(OpenError(fs, tree.Uri("dir"), Stream::OpenReadBinary()) == IoErrorCode::InvalidPath);
    // A path that is not UTF-8 never reaches the OS (engine/os InvalidEncoding).
    auto invalid = Path::FileUriFromNativePath(tree.Native() + "/\xC0\xAF");
    REQUIRE(invalid);
    CHECK(OpenError(fs, invalid.value(), Stream::OpenReadBinary()) == IoErrorCode::InvalidPath);
    CHECK(fs.Stat(invalid.value()).error().code == IoErrorCode::InvalidPath);
    // A mode that contradicts itself.
    CHECK(OpenError(fs, tree.Uri("dir/h.bin"), Stream::FileOpenMode::None) == IoErrorCode::InvalidPath);
}

TEST_CASE("NativeFileSystem takes only paths absolute on this platform") {
    // "/x" is the root of the current drive on Windows, "C:/x" a relative path on POSIX: exactly one of them is
    // refused before the OS is asked, the other is simply missing.
    FS::NativeFileSystem fs;
    const auto posix = fs.Stat(Path::ParseUri("file:///gyo-no-such-directory/x.bin").value());
    const auto drive = fs.Stat(Path::ParseUri("file:///C:/gyo-no-such-directory/x.bin").value());
    REQUIRE_FALSE(posix);
    REQUIRE_FALSE(drive);
    CHECK((posix.error().code == IoErrorCode::InvalidPath) != (drive.error().code == IoErrorCode::InvalidPath));
    CHECK((posix.error().code == IoErrorCode::NotFound) != (drive.error().code == IoErrorCode::NotFound));
}

// os-files-write opened writing (a file it creates, emptied or appended to) and
// CreateDirectories; the rest stays NotSupported and Capabilities() says so.
TEST_CASE("NativeFileSystem says what it supports") {
    const TempTree tree;
    tree.Write("e.bin", "x");
    FS::NativeFileSystem fs;
    const auto uri = tree.Uri("e.bin");
    using Stream::FileOpenMode;
    CHECK(OpenError(fs, uri, FileOpenMode::Read | FileOpenMode::CreateIfMissing) == IoErrorCode::NotSupported);
    CHECK(OpenError(fs, uri, FileOpenMode::Read | FileOpenMode::Write | FileOpenMode::Truncate |
                                 FileOpenMode::CreateIfMissing) == IoErrorCode::NotSupported);
    // Overwriting in place, or writing an existing file only, is not offered.
    CHECK(OpenError(fs, uri, FileOpenMode::Write | FileOpenMode::CreateIfMissing) == IoErrorCode::NotSupported);
    CHECK(OpenError(fs, uri, Stream::OpenWriteBinaryTruncate(false)) == IoErrorCode::NotSupported);
    CHECK(fs.Remove(uri, {}).error().code == IoErrorCode::NotSupported);
    CHECK(fs.Move(uri, uri).error().code == IoErrorCode::NotSupported);
    CHECK(fs.Copy(uri, uri).error().code == IoErrorCode::NotSupported);
    CHECK(fs.List(tree.Uri(), {}).error().code == IoErrorCode::NotSupported);
    CHECK(fs.Iterate(tree.Uri(), {}).error().code == IoErrorCode::NotSupported);
    CHECK(fs.CreateWatcher().error().code == IoErrorCode::NotSupported);

    const auto caps = fs.Capabilities();
    CHECK(caps.canOpenRead);
    CHECK(caps.canStat);
    CHECK(caps.supportsToNativePath);
    CHECK(caps.supportsMtime);
    CHECK(caps.canOpenWrite);
    CHECK(caps.canCreateDirectories);
    CHECK_FALSE(caps.canList);
    CHECK_FALSE(caps.canIterate);
    CHECK_FALSE(caps.canRemove);
    CHECK_FALSE(caps.canRemoveRecursive);
    CHECK_FALSE(caps.canMove);
    CHECK_FALSE(caps.canCopy);
    CHECK_FALSE(caps.supportsWatch);
    CHECK_FALSE(caps.supportsRecursiveWatch);
}

std::filesystem::path FromUtf8(const std::string& native) {
    return std::filesystem::path(std::u8string(native.begin(), native.end()));
}

std::string ReadBack(const std::string& native) {
    std::ifstream file(FromUtf8(native), std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void WriteText(FS::NativeFileSystem& fs, const Path::Uri& uri, const Stream::FileOpenMode mode,
               const std::string& text) {
    auto stream = fs.Open(uri, mode);
    REQUIRE_MESSAGE(stream, (stream ? std::string{} : stream.error().message + " " + stream.error().detail));
    const auto caps = stream.value()->Caps();
    CHECK(caps.writable);
    CHECK_FALSE(caps.readable);
    CHECK_FALSE(caps.seekable);
    auto wrote = stream.value()->Write(text.data(), text.size());
    REQUIRE(wrote);
    CHECK(wrote.value() == text.size());
    CHECK(stream.value()->Flush());
    CHECK(stream.value()->Close());
    CHECK_FALSE(stream.value()->IsOpen());
    CHECK(stream.value()->Write(text.data(), text.size()).error().code == IoErrorCode::WriteFailed);
}

TEST_CASE("NativeFileSystem writes files: truncate, append and the platform's line endings") {
    const TempTree tree;
    FS::NativeFileSystem fs;
    const auto uri = tree.Uri("trace.jsonl");
    WriteText(fs, uri, Stream::OpenWriteBinaryTruncate(), "0123456789");
    WriteText(fs, uri, Stream::OpenWriteBinaryTruncate(), "abc\n");
    CHECK(ReadBack(tree.Native("trace.jsonl")) == "abc\n");
    WriteText(fs, uri, Stream::OpenWriteBinaryAppend(), "def\n");
    CHECK(ReadBack(tree.Native("trace.jsonl")) == "abc\ndef\n");

    // Text: the bytes a text-mode C++ stream writes ("\r\n" on Windows).
    const auto text = tree.Uri("text.jsonl");
    using Stream::FileOpenMode;
    WriteText(fs, text, FileOpenMode::Write | FileOpenMode::Truncate | FileOpenMode::CreateIfMissing | FileOpenMode::Text,
              "a\nb\n");
    { std::ofstream(FromUtf8(tree.Native("reference.jsonl"))) << "a\nb\n"; }
    CHECK(ReadBack(tree.Native("text.jsonl")) == ReadBack(tree.Native("reference.jsonl")));
}

TEST_CASE("NativeFileSystem creates directories and keeps the engine/os cause of a write failure") {
    const TempTree tree;
    FS::NativeFileSystem fs;
    REQUIRE(fs.CreateDirectories(tree.Uri("logs/2026/10")));
    CHECK(std::filesystem::is_directory(FromUtf8(tree.Native("logs/2026/10"))));
    WriteText(fs, tree.Uri("logs/2026/10/client.log"), Stream::OpenWriteBinaryAppend(), "line\n");

    auto missing = fs.Open(tree.Uri("absent/x.log"), Stream::OpenWriteBinaryTruncate());
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == IoErrorCode::NotFound);
    CHECK(missing.error().detail.find("NotFound: ") != std::string::npos);
    CHECK(missing.error().detail.find("path_bytes=") != std::string::npos);

    auto onFile = fs.CreateDirectories(tree.Uri("logs/2026/10/client.log"));
    REQUIRE_FALSE(onFile);
    CHECK(onFile.error().code == IoErrorCode::InvalidPath);
}

TEST_CASE("NativeFileSystem takes file URIs only") {
    FS::NativeFileSystem fs;
    const auto logical = Path::ParseUriLoose("asset://textures/a.png");
    CHECK(fs.Open(logical, Stream::OpenReadBinary()).error().code == IoErrorCode::InvalidPath);
    CHECK(fs.Exists(logical).error().code == IoErrorCode::InvalidPath);
    CHECK(fs.Stat(logical).error().code == IoErrorCode::InvalidPath);
    const TempTree tree;
    CHECK(fs.ToNativePathString(tree.Uri("f.bin")).value() == tree.Native("f.bin"));
}

TEST_CASE("NativeFileSystem serves a VFS mount on a file root") {
    const TempTree tree;
    tree.Write("assets/textures/g.bin", "mounted");
    FS::Vfs vfs;
    FS::MountPoint mount;
    mount.name = "native";
    mount.readOnly = true;
    mount.mountUri = Path::ParseUriLoose("asset://");
    mount.rootUri = tree.Uri("assets");
    mount.fs = std::make_shared<FS::NativeFileSystem>();
    REQUIRE(vfs.Mount(std::move(mount)));
    const auto bytes = Helpers::ReadAllBytes(vfs, Path::ParseUriLoose("asset://textures/g.bin"));
    REQUIRE(bytes);
    CHECK(AsText(bytes.value()) == "mounted");
    CHECK(vfs.Stat(Path::ParseUriLoose("asset://textures/missing.bin")).error().code == IoErrorCode::NotFound);
}
