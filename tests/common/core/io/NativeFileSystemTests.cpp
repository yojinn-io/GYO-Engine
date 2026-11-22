#include "doctest/doctest.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <fstream>
#include <iterator>
#include <memory>
#include <utility>
#include <string>
#include <thread>
#include <vector>

#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/helpers/WriteFileAtomically.hpp"
#include "engine/io/path/Uri.hpp"

using namespace GYO::Engine::IO;

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

IOErrorCode OpenError(FS::NativeFileSystem& fs, const Path::Uri& uri, const Stream::FileOpenMode mode) {
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
    CHECK(stream.Seek(-4, Stream::SeekWhence::Current).error().code == IOErrorCode::SeekFailed);
    CHECK(stream.Seek(11, Stream::SeekWhence::Begin).error().code == IOErrorCode::SeekFailed);
    CHECK(stream.Seek(1, Stream::SeekWhence::End).error().code == IOErrorCode::SeekFailed);
    for (const auto whence : {Stream::SeekWhence::Begin, Stream::SeekWhence::Current, Stream::SeekWhence::End}) {
        CHECK(stream.Seek((std::numeric_limits<std::int64_t>::min)(), whence).error().code ==
              IOErrorCode::SeekFailed);
    }
    CHECK(stream.Tell().value() == 3);
    CHECK(stream.Seek(0, Stream::SeekWhence::End).value() == 10);
    // A read of zero bytes is not the end of the file.
    CHECK(stream.Seek(0, Stream::SeekWhence::Begin).value() == 0);
    CHECK(stream.Read(tail, 0).value() == 0);
    CHECK_FALSE(stream.IsEof());
    CHECK(stream.Write("x", 1).error().code == IOErrorCode::NotSupported);
    REQUIRE(stream.Close());
    CHECK_FALSE(stream.IsOpen());
    CHECK(stream.Read(tail, 1).error().code == IOErrorCode::ReadFailed);
    CHECK(stream.Tell().error().code == IOErrorCode::SeekFailed);
    CHECK(stream.Seek(0, Stream::SeekWhence::Begin).error().code == IOErrorCode::SeekFailed);
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
    CHECK(opened.error().code == IOErrorCode::NotFound);
    CHECK(opened.error().message == "cannot open file");
    // "<native path>; NotFound: errno=<n>; path_bytes=<n>" (win32= on Windows).
    const std::string expectedStart = missing + "; NotFound: ";
    CHECK(opened.error().detail.starts_with(expectedStart));
    CHECK(opened.error().detail.ends_with("; path_bytes=" + std::to_string(missing.size())));
    CHECK(fs.Stat(tree.Uri("missing.bin")).error().code == IOErrorCode::NotFound);
}

TEST_CASE("NativeFileSystem maps the other engine/os failures") {
    const TempTree tree;
    tree.Write("dir/h.bin", "x");
    FS::NativeFileSystem fs;
    // A directory is not a file (engine/os InvalidPath).
    CHECK(OpenError(fs, tree.Uri("dir"), Stream::OpenReadBinary()) == IOErrorCode::InvalidPath);
    // A path that is not UTF-8 never reaches the OS (engine/os InvalidEncoding).
    auto invalid = Path::FileUriFromNativePath(tree.Native() + "/\xC0\xAF");
    REQUIRE(invalid);
    CHECK(OpenError(fs, invalid.value(), Stream::OpenReadBinary()) == IOErrorCode::InvalidPath);
    CHECK(fs.Stat(invalid.value()).error().code == IOErrorCode::InvalidPath);
    // A mode that contradicts itself.
    CHECK(OpenError(fs, tree.Uri("dir/h.bin"), Stream::FileOpenMode::None) == IOErrorCode::InvalidPath);
}

TEST_CASE("NativeFileSystem takes only paths absolute on this platform") {
    // "/x" is the root of the current drive on Windows, "C:/x" a relative path on POSIX: exactly one of them is
    // refused before the OS is asked, the other is simply missing.
    FS::NativeFileSystem fs;
    const auto posix = fs.Stat(Path::ParseUri("file:///gyo-no-such-directory/x.bin").value());
    const auto drive = fs.Stat(Path::ParseUri("file:///C:/gyo-no-such-directory/x.bin").value());
    REQUIRE_FALSE(posix);
    REQUIRE_FALSE(drive);
    CHECK((posix.error().code == IOErrorCode::InvalidPath) != (drive.error().code == IOErrorCode::InvalidPath));
    CHECK((posix.error().code == IOErrorCode::NotFound) != (drive.error().code == IOErrorCode::NotFound));
}

// os-files-write opened writing (a file it creates, emptied or appended to) and
// CreateDirectories; os-tool-files opened Move (replacing) and Remove (files).
// The other open modes stay NotSupported.
TEST_CASE("NativeFileSystem says what it supports") {
    const TempTree tree;
    tree.Write("e.bin", "x");
    FS::NativeFileSystem fs;
    const auto uri = tree.Uri("e.bin");
    using Stream::FileOpenMode;
    CHECK(OpenError(fs, uri, FileOpenMode::Read | FileOpenMode::CreateIfMissing) == IOErrorCode::NotSupported);
    CHECK(OpenError(fs, uri, FileOpenMode::Read | FileOpenMode::Write | FileOpenMode::Truncate |
                                 FileOpenMode::CreateIfMissing) == IOErrorCode::NotSupported);
    // Overwriting in place, or writing an existing file only, is not offered.
    CHECK(OpenError(fs, uri, FileOpenMode::Write | FileOpenMode::CreateIfMissing) == IOErrorCode::NotSupported);
    CHECK(OpenError(fs, uri, Stream::OpenWriteBinaryTruncate(false)) == IOErrorCode::NotSupported);
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
    CHECK(stream.value()->Write(text.data(), text.size()).error().code == IOErrorCode::WriteFailed);
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
    CHECK(missing.error().code == IOErrorCode::NotFound);
    CHECK(missing.error().detail.find("NotFound: ") != std::string::npos);
    CHECK(missing.error().detail.find("path_bytes=") != std::string::npos);

    auto onFile = fs.CreateDirectories(tree.Uri("logs/2026/10/client.log"));
    REQUIRE_FALSE(onFile);
    CHECK(onFile.error().code == IOErrorCode::InvalidPath);
}

TEST_CASE("NativeFileSystem moves a file over another and removes files") {
    const TempTree tree;
    tree.Write("menu.json", "old");
    tree.Write("menu.json.tmp", "new");
    tree.Write("folder/inner.txt", "inner");
    FS::NativeFileSystem fs;
    REQUIRE(fs.Move(tree.Uri("menu.json.tmp"), tree.Uri("menu.json")));
    CHECK(ReadBack(tree.Native("menu.json")) == "new");
    CHECK_FALSE(fs.Exists(tree.Uri("menu.json.tmp")).value());

    const auto missing = fs.Move(tree.Uri("absent.tmp"), tree.Uri("menu.json"));
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == IOErrorCode::NotFound);
    CHECK(missing.error().detail.find("NotFound: ") != std::string::npos);
    CHECK(ReadBack(tree.Native("menu.json")) == "new");

    REQUIRE(fs.Remove(tree.Uri("menu.json")));
    CHECK_FALSE(fs.Exists(tree.Uri("menu.json")).value());
    CHECK(fs.Remove(tree.Uri("menu.json")).error().code == IOErrorCode::NotFound);
    // A directory stays, with its contents, and is InvalidPath on every platform.
    const auto folder = fs.Remove(tree.Uri("folder"));
    REQUIRE_FALSE(folder);
    CHECK(folder.error().code == IOErrorCode::InvalidPath);
    CHECK(ReadBack(tree.Native("folder/inner.txt")) == "inner");
}

// The editor's atomic save (os-tool-files, D56): a reader sees the old file or
// all of the new bytes; nothing is left beside the file.
TEST_CASE("NativeFileSystem writes a file atomically through a temporary file") {
    const TempTree tree;
    FS::NativeFileSystem fs;
    const auto bytes = [](const std::string& text) {
        return GYO::Engine::Base::ConstSpan<std::byte>(reinterpret_cast<const std::byte*>(text.data()), text.size());
    };
    const std::string first = "first\n";
    const std::string second = "second\r\n";
    REQUIRE(Helpers::WriteFileAtomically(fs, tree.Uri("export.json"), bytes(first)));
    CHECK(ReadBack(tree.Native("export.json")) == first);
    REQUIRE(Helpers::WriteFileAtomically(fs, tree.Uri("export.json"), bytes(second)));
    CHECK(ReadBack(tree.Native("export.json")) == second);
    const auto names = [&tree] {
        std::vector<std::string> out;
        for (const auto& entry : std::filesystem::directory_iterator(FromUtf8(tree.Native()))) {
            const std::u8string name = entry.path().filename().u8string();
            out.emplace_back(name.begin(), name.end());
        }
        return out;
    };
    CHECK(names() == std::vector<std::string>{"export.json"});

    // A directory in the way: the move fails, the directory is kept and the temporary file removed.
    tree.Write("taken/inner.txt", "inner");
    const auto blocked = Helpers::WriteFileAtomically(fs, tree.Uri("taken"), bytes(first));
    REQUIRE_FALSE(blocked);
    CHECK(ReadBack(tree.Native("taken/inner.txt")) == "inner");
    auto left = names();
    std::sort(left.begin(), left.end());
    CHECK(left == std::vector<std::string>{"export.json", "taken"});

    // A missing parent directory is not made.
    const auto noParent = Helpers::WriteFileAtomically(fs, tree.Uri("absent/export.json"), bytes(first));
    REQUIRE_FALSE(noParent);
    CHECK(noParent.error().code == IOErrorCode::NotFound);
}

namespace {

// A write stream whose writes fail, around a real one (which it still closes).
class FailingWriteStream final : public Stream::IStream {
public:
    explicit FailingWriteStream(std::unique_ptr<Stream::IStream> inner) : inner_(std::move(inner)) {}
    Stream::StreamCaps Caps() const noexcept override { return inner_->Caps(); }
    bool IsOpen() const noexcept override { return inner_->IsOpen(); }
    bool IsEof() const noexcept override { return inner_->IsEof(); }
    GYO::Engine::Base::Result<std::size_t, IOError> Read(void* dst, const std::size_t bytes) override {
        return inner_->Read(dst, bytes);
    }
    GYO::Engine::Base::Result<std::size_t, IOError> Write(const void*, std::size_t) override {
        return GYO::Engine::Base::Err(IOError::Make(IOErrorCode::WriteFailed, "injected write failure"));
    }
    GYO::Engine::Base::Result<std::uint64_t, IOError> Tell() const override { return inner_->Tell(); }
    GYO::Engine::Base::Result<std::uint64_t, IOError> Seek(const std::int64_t offset, const Stream::SeekWhence whence) override {
        return inner_->Seek(offset, whence);
    }
    GYO::Engine::Base::Result<std::uint64_t, IOError> Size() const override { return inner_->Size(); }
    GYO::Engine::Base::Result<void, IOError> Flush() override { return inner_->Flush(); }
    GYO::Engine::Base::Result<void, IOError> Close() override { return inner_->Close(); }

private:
    std::unique_ptr<Stream::IStream> inner_;
};

// The native backend with every call that changes files recorded; writes can be made to fail.
class RecordingFileSystem final : public FS::IFileSystem {
public:
    std::vector<std::string> opened;
    std::vector<std::pair<std::string, std::string>> moved;
    std::vector<std::string> removed;
    bool failWrites = false;

    const char* Name() const noexcept override { return "Recording"; }
    IOResult<std::unique_ptr<Stream::IStream>> Open(const Path::Uri& uri, const Stream::FileOpenMode mode) override {
        opened.push_back(uri.path.Str());
        auto stream = native_.Open(uri, mode);
        if (!stream || !failWrites) return stream;
        return std::unique_ptr<Stream::IStream>(std::make_unique<FailingWriteStream>(std::move(stream.value())));
    }
    IOResult<bool> Exists(const Path::Uri& uri) override { return native_.Exists(uri); }
    IOResult<FS::FileInfo> Stat(const Path::Uri& uri) override { return native_.Stat(uri); }
    IOResult<void> CreateDirectories(const Path::Uri& uri) override { return native_.CreateDirectories(uri); }
    IOResult<void> Remove(const Path::Uri& uri) override {
        removed.push_back(uri.path.Str());
        return native_.Remove(uri);
    }
    IOResult<void> Move(const Path::Uri& from, const Path::Uri& to) override {
        moved.emplace_back(from.path.Str(), to.path.Str());
        return native_.Move(from, to);
    }
    IOResult<std::string> ToNativePathString(const Path::Uri& uri) override { return native_.ToNativePathString(uri); }

private:
    FS::NativeFileSystem native_;
};

} // namespace

// The steps of the atomic write: the bytes never go to the target itself, only
// to a temporary file that is moved over it, and a failed write leaves the
// target as it was and no temporary file.
TEST_CASE("NativeFileSystem atomic writes go through a temporary file and a move") {
    const TempTree tree;
    tree.Write("export.json", "old");
    const std::string target = tree.Uri("export.json").path.Str();
    const std::string text = "new";
    const GYO::Engine::Base::ConstSpan<std::byte> bytes(reinterpret_cast<const std::byte*>(text.data()), text.size());

    RecordingFileSystem recording;
    REQUIRE(Helpers::WriteFileAtomically(recording, tree.Uri("export.json"), bytes));
    REQUIRE(recording.opened.size() == 1);
    const std::string temporary = recording.opened.front();
    CHECK(temporary != target);
    CHECK(temporary.rfind(target + ".tmp.", 0) == 0);
    REQUIRE(recording.moved.size() == 1);
    CHECK(recording.moved.front() == std::pair<std::string, std::string>(temporary, target));
    CHECK(recording.removed.empty());
    CHECK(ReadBack(tree.Native("export.json")) == "new");

    RecordingFileSystem failing;
    failing.failWrites = true;
    const auto failed = Helpers::WriteFileAtomically(failing, tree.Uri("export.json"), bytes);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().code == IOErrorCode::WriteFailed);
    REQUIRE(failing.opened.size() == 1);
    CHECK(failing.moved.empty());
    CHECK(failing.removed == std::vector<std::string>{failing.opened.front()});
    CHECK(ReadBack(tree.Native("export.json")) == "new");
    CHECK_FALSE(std::filesystem::exists(FromUtf8(tree.Native()) / "export.json.tmp"));
    std::size_t entries = 0;
    for ([[maybe_unused]] const auto& entry : std::filesystem::directory_iterator(FromUtf8(tree.Native()))) ++entries;
    CHECK(entries == 1);
}

TEST_CASE("NativeFileSystem takes file URIs only") {
    FS::NativeFileSystem fs;
    const auto logical = Path::ParseUri("asset://textures/a.png").value();
    CHECK(fs.Open(logical, Stream::OpenReadBinary()).error().code == IOErrorCode::InvalidPath);
    CHECK(fs.Exists(logical).error().code == IOErrorCode::InvalidPath);
    CHECK(fs.Stat(logical).error().code == IOErrorCode::InvalidPath);
    const TempTree tree;
    CHECK(fs.ToNativePathString(tree.Uri("f.bin")).value() == tree.Native("f.bin"));
}

// A caller tells a file over its limit from one it could not read (docs/architecture/io.md): TooLarge, whether
// the size is known before reading or found while reading.
TEST_CASE("ReadAllBytes reports a file over the limit as TooLarge") {
    const TempTree tree;
    tree.Write("big.bin", std::string(64, 'x'));
    FS::NativeFileSystem fs;
    Helpers::ReadAllOptions small;
    small.maxBytes = 16;
    const auto sized = Helpers::ReadAllBytes(fs, tree.Uri("big.bin"), small);
    REQUIRE_FALSE(sized);
    CHECK(sized.error().code == IOErrorCode::TooLarge);
    small.tryUseSizeHint = false;
    small.chunkBytes = 8;
    const auto streamed = Helpers::ReadAllBytes(fs, tree.Uri("big.bin"), small);
    REQUIRE_FALSE(streamed);
    CHECK(streamed.error().code == IOErrorCode::TooLarge);
    small.maxBytes = 64;
    CHECK(Helpers::ReadAllBytes(fs, tree.Uri("big.bin"), small));
}

// Design Invariant 6: one NativeFileSystem read by many threads at once. A smoke test, not a guard: no change
// to the backend makes it fail every time (docs/architecture/io.md).
TEST_CASE("NativeFileSystem serves whole-file reads from several threads at once") {
    const TempTree tree;
    const std::string contents[] = {std::string(70000, 'a'), std::string(3, 'b'), std::string(130001, 'c')};
    for (std::size_t i = 0; i < std::size(contents); ++i) tree.Write("f" + std::to_string(i) + ".bin", contents[i]);
    FS::NativeFileSystem fs;
    std::atomic<int> wrong{0};
    std::vector<std::thread> readers;
    for (int t = 0; t < 8; ++t) {
        readers.emplace_back([&, t] {
            for (int round = 0; round < 200; ++round) {
                const std::size_t i = static_cast<std::size_t>(t + round) % std::size(contents);
                const auto bytes = Helpers::ReadAllBytes(fs, tree.Uri("f" + std::to_string(i) + ".bin"));
                if (!bytes || AsText(bytes.value()) != contents[i]) ++wrong;
            }
        });
    }
    for (auto& reader : readers) reader.join();
    CHECK(wrong.load() == 0);
}
