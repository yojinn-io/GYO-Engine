#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/os/Files.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>

// The write primitives (docs/architecture/os.md, Files). The reference for the
// bytes is a C++ file stream: a text-mode std::ofstream for Native line endings
// (the C runtime decides them per platform), a binary one otherwise, which is
// what the callers wrote with before engine/os had a writer (Design Invariant 9).

using namespace Engine::Os;
using namespace Engine::Os::Files;

namespace {

std::string Utf8(const std::filesystem::path& path) {
    auto text = PathToUtf8(path);
    REQUIRE_MESSAGE(text, "PathToUtf8 failed");
    return text.value();
}

class TempDirectory final {
public:
    explicit TempDirectory(const char* name) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() / (std::string(name) + "-" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    ~TempDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    const std::filesystem::path& Path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

std::string ReadBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::span<const std::byte> Bytes(const std::string_view text) {
    return std::as_bytes(std::span(text.data(), text.size()));
}

void WriteAll(const std::filesystem::path& path, const WriteMode mode, const LineEndings endings,
              const std::string_view text) {
    auto file = FileWriter::Open(Utf8(path), mode, endings);
    REQUIRE_MESSAGE(file, (file ? std::string{} : Engine::Base::Describe(file.error())));
    REQUIRE(file->Write(Bytes(text)));
    REQUIRE(file->Close());
}

// The bytes a C++ file stream writes for text in this mode.
std::string Reference(const std::filesystem::path& path, const WriteMode mode, const LineEndings endings,
                      const std::string_view text) {
    std::ios::openmode flags = mode == WriteMode::Append ? std::ios::app : std::ios::trunc;
    if (endings == LineEndings::AsWritten) flags |= std::ios::binary;
    {
        std::ofstream out(path, flags | std::ios::out);
        out << text;
    }
    return ReadBytes(path);
}

constexpr std::string_view kLines = "first\nsecond\r\nthird\n\nlast";

// The form the test uses to remove a deep tree: on Windows the standard
// library has no long-path handling of its own (as in FilesTests.cpp).
std::filesystem::path SetupPath(const std::filesystem::path& path) {
#if defined(_WIN32)
    return std::filesystem::path(L"\\\\?\\" + std::filesystem::absolute(path).make_preferred().native());
#else
    return path;
#endif
}

} // namespace

TEST_CASE("Os files write a new file and truncate an existing one") {
    TempDirectory dir("gyo-os-write-truncate");
    const auto path = dir.Path() / "trace.jsonl";
    WriteAll(path, WriteMode::Truncate, LineEndings::AsWritten, "0123456789");
    CHECK(ReadBytes(path) == "0123456789");
    WriteAll(path, WriteMode::Truncate, LineEndings::AsWritten, "abc");
    CHECK(ReadBytes(path) == "abc");
}

TEST_CASE("Os files append to a file, creating it when missing") {
    TempDirectory dir("gyo-os-write-append");
    const auto path = dir.Path() / "log.txt";
    WriteAll(path, WriteMode::Append, LineEndings::AsWritten, "one\n");
    WriteAll(path, WriteMode::Append, LineEndings::AsWritten, "two\n");
    CHECK(ReadBytes(path) == "one\ntwo\n");
}

TEST_CASE("Os files write the same bytes as a C++ file stream in each mode") {
    TempDirectory dir("gyo-os-write-reference");
    for (const auto mode : {WriteMode::Truncate, WriteMode::Append}) {
        for (const auto endings : {LineEndings::AsWritten, LineEndings::Native}) {
            CAPTURE(static_cast<int>(mode));
            CAPTURE(static_cast<int>(endings));
            const auto ours = dir.Path() / "ours.txt";
            const auto theirs = dir.Path() / "theirs.txt";
            // Two rounds, so that append and truncate both meet an existing file.
            for (int round = 0; round < 2; ++round) {
                WriteAll(ours, mode, endings, kLines);
                Reference(theirs, mode, endings, kLines);
            }
            CHECK(ReadBytes(ours) == ReadBytes(theirs));
            std::filesystem::remove(ours);
            std::filesystem::remove(theirs);
        }
    }
}

TEST_CASE("Os files write native line endings in pieces as in one write") {
    // A text-mode stream translates every '\n' wherever a write splits the text.
    TempDirectory dir("gyo-os-write-pieces");
    const auto pieces = dir.Path() / "pieces.txt";
    {
        auto file = FileWriter::Open(Utf8(pieces), WriteMode::Truncate, LineEndings::Native);
        REQUIRE(file);
        for (const char c : kLines) REQUIRE(file->Write(Bytes(std::string_view(&c, 1))));
        REQUIRE(file->Close());
    }
    const auto whole = dir.Path() / "whole.txt";
    WriteAll(whole, WriteMode::Truncate, LineEndings::Native, kLines);
    CHECK(ReadBytes(pieces) == ReadBytes(whole));
}

TEST_CASE("Os files write a path outside the ANSI code page") {
    TempDirectory dir("gyo-os-write-unicode");
    const auto path = dir.Path() / std::filesystem::path(u8"日本語-한글-é.jsonl");
    WriteAll(path, WriteMode::Truncate, LineEndings::AsWritten, "{}\n");
    CHECK(ReadBytes(path) == "{}\n");
}

TEST_CASE("Os files let a reader open a file a writer holds") {
    // The C++ stream shared its file with readers and writers, not with
    // deleters (_SH_DENYNO on Windows); so does the writer.
    TempDirectory dir("gyo-os-write-share");
    const auto path = dir.Path() / "shared.txt";
    auto writer = FileWriter::Open(Utf8(path), WriteMode::Truncate, LineEndings::AsWritten);
    REQUIRE(writer);
    REQUIRE(writer->Write(Bytes("seen")));
    auto reader = FileReader::Open(Utf8(path));
    REQUIRE_MESSAGE(reader, (reader ? std::string{} : Engine::Base::Describe(reader.error())));
    auto size = reader->Size();
    REQUIRE(size);
    CHECK(*size == 4);
    auto second = FileWriter::Open(Utf8(path), WriteMode::Append, LineEndings::AsWritten);
    REQUIRE_MESSAGE(second, (second ? std::string{} : Engine::Base::Describe(second.error())));
    CHECK(second->Close());
#if defined(_WIN32)
    std::error_code removing;
    CHECK_FALSE(std::filesystem::remove(path, removing));
    CHECK(removing);
#endif
    CHECK(writer->Close());
}

TEST_CASE("Os files append every write at the end, whoever wrote last") {
    // Two appending writers on one file: each write goes after the other's.
    TempDirectory dir("gyo-os-write-interleave");
    const auto path = dir.Path() / "shared.log";
    auto first = FileWriter::Open(Utf8(path), WriteMode::Append, LineEndings::AsWritten);
    auto second = FileWriter::Open(Utf8(path), WriteMode::Append, LineEndings::AsWritten);
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(first->Write(Bytes("a")));
    REQUIRE(second->Write(Bytes("b")));
    REQUIRE(first->Write(Bytes("c")));
    REQUIRE(first->Close());
    REQUIRE(second->Close());
    CHECK(ReadBytes(path) == "abc");
}

TEST_CASE("Os files report write failures with the native code") {
    TempDirectory dir("gyo-os-write-errors");
    const auto missing = FileWriter::Open(Utf8(dir.Path() / "absent" / "x.txt"), WriteMode::Truncate,
                                          LineEndings::AsWritten);
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == OsErrorCode::NotFound);
    CHECK((missing.error().detail.rfind("errno=", 0) == 0 || missing.error().detail.rfind("win32=", 0) == 0));
    CHECK(missing.error().detail.find("; path_bytes=") != std::string::npos);

    const auto directory = FileWriter::Open(Utf8(dir.Path()), WriteMode::Truncate, LineEndings::AsWritten);
    REQUIRE_FALSE(directory);
    CHECK(directory.error().code == OsErrorCode::InvalidPath);

    const auto invalid = FileWriter::Open("bad-\xFF", WriteMode::Truncate, LineEndings::AsWritten);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().code == OsErrorCode::InvalidEncoding);
    CHECK(invalid.error().detail == "path_bytes=5");
}

TEST_CASE("Os files write nothing for an empty span and refuse a moved-from writer") {
    TempDirectory dir("gyo-os-write-moved");
    const auto path = dir.Path() / "moved.txt";
    auto writer = FileWriter::Open(Utf8(path), WriteMode::Truncate, LineEndings::Native);
    REQUIRE(writer);
    CHECK(writer->Write({}));
    FileWriter moved = std::move(*writer);
    REQUIRE(moved.Write(Bytes("x")));
    REQUIRE(moved.Close());
    CHECK(ReadBytes(path) == "x");
    Engine::Test::ScopedAssertionHandler throwing;
    CHECK_THROWS_AS(static_cast<void>(writer->Write(Bytes("y"))), Engine::Base::AssertionFailure);
    CHECK_THROWS_AS(static_cast<void>(moved.Write(Bytes("y"))), Engine::Base::AssertionFailure);
}

TEST_CASE("Os files create directories with their missing parents") {
    TempDirectory dir("gyo-os-mkdirs");
    const auto nested = dir.Path() / "a" / "b" / u8"日本";
    REQUIRE(CreateDirectories(Utf8(nested)));
    CHECK(std::filesystem::is_directory(nested));
    // An existing directory is fine, as with std::filesystem::create_directories.
    CHECK(CreateDirectories(Utf8(nested)));
    CHECK(CreateDirectories(Utf8(dir.Path())));

    const auto file = dir.Path() / "file.txt";
    WriteAll(file, WriteMode::Truncate, LineEndings::AsWritten, "x");
    const auto underFile = CreateDirectories(Utf8(file / "child"));
    REQUIRE_FALSE(underFile);
    const auto onFile = CreateDirectories(Utf8(file));
    REQUIRE_FALSE(onFile);
    CHECK(onFile.error().code == OsErrorCode::InvalidPath);

    const auto empty = CreateDirectories("");
    REQUIRE_FALSE(empty);
    CHECK(empty.error().code == OsErrorCode::InvalidPath);
}

TEST_CASE("Os files write and create directories under a path longer than 300 characters") {
    TempDirectory dir("gyo-os-write-long");
    const auto top = dir.Path() / "segment-of-a-deep-release-folder";
    std::filesystem::path deep = top;
    while (Utf8(deep).size() < 300) deep /= "segment-of-a-deep-release-folder";
    REQUIRE(CreateDirectories(Utf8(deep)));
    const auto path = deep / "match-ingress.jsonl";
    WriteAll(path, WriteMode::Truncate, LineEndings::Native, "{}\n");
    auto reader = FileReader::Open(Utf8(path));
    REQUIRE_MESSAGE(reader, (reader ? std::string{} : Engine::Base::Describe(reader.error())));
    auto status = Stat(Utf8(path));
    REQUIRE(status);
    CHECK(status->kind == FileKind::Regular);
    CHECK(status->sizeBytes >= 3);
    std::error_code ignored;
    std::filesystem::remove_all(SetupPath(top), ignored);
}
