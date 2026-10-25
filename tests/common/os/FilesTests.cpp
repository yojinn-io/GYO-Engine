#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/os/Files.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <regex>
#include <string>
#include <utility>
#include <vector>

// The one common test directory allowed to look at the OS: these cases check
// per-platform behaviour and set up FIFOs, devices and links.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

using namespace Engine::Os;
using namespace Engine::Os::Files;

namespace {

std::string Utf8(const std::filesystem::path& path) {
    auto text = PathToUtf8(path);
    REQUIRE_MESSAGE(text, "PathToUtf8 failed");
    return text.value();
}

std::filesystem::path Native(const std::string& utf8) {
    auto path = PathFromUtf8(utf8);
    REQUIRE_MESSAGE(path, "PathFromUtf8 failed for " << utf8);
    return path.value();
}

// A fresh directory under the temporary directory, removed afterwards. Tests
// may use std::filesystem and fstream to set up and check files.
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

    std::filesystem::path Write(const std::filesystem::path& relative, const std::string& bytes) const {
        const auto file = path_ / relative;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return file;
    }

private:
    std::filesystem::path path_;
};

std::string ReadAll(ReadFile& file) {
    std::string out;
    std::vector<std::byte> buffer(4);
    while (true) {
        auto got = file.Read(buffer);
        REQUIRE(got);
        if (got.value() == 0) return out;
        for (std::size_t i = 0; i < got.value(); ++i) out.push_back(static_cast<char>(buffer[i]));
    }
}

OsErrorCode OpenError(const std::string& path) {
    auto file = ReadFile::Open(path);
    REQUIRE_MESSAGE(!file, "ReadFile::Open accepted " << path);
    return file.error().code;
}

} // namespace

TEST_CASE("Os files read a whole file through its UTF-8 path") {
    const TempDirectory temp("gyo-os-files");
    const std::string bytes("hello\0world", 11);
    const auto path = temp.Write("a.bin", bytes);
    auto file = ReadFile::Open(Utf8(path));
    REQUIRE(file);
    CHECK(file.value().Size().value() == 11);
    CHECK(ReadAll(file.value()) == bytes);
    // At the end of the file a read returns 0 bytes; an empty span returns 0 without a read.
    std::vector<std::byte> more(8);
    CHECK(file.value().Read(more).value() == 0);
    CHECK(file.value().Read(std::span<std::byte>{}).value() == 0);
}

TEST_CASE("Os files open a path outside the ANSI code page") {
    // Japanese and Traditional Chinese characters, which a Western code page cannot represent.
    const TempDirectory temp("gyo-os-files");
    const auto path =
        temp.Write(Native("\xE8\xB3\x87\xE6\x96\x99\xE5\xA4\xBE-\xC3\xA9\xE6\x97\xA5\xE6\x9C\xAC/b.txt"), "UTF-8 path");
    const std::string utf8 = Utf8(path);
    CHECK(utf8.find("\xE8\xB3\x87\xE6\x96\x99\xE5\xA4\xBE") != std::string::npos);
    auto file = ReadFile::Open(utf8);
    REQUIRE_MESSAGE(file, "cannot open " << utf8);
    CHECK(ReadAll(file.value()) == "UTF-8 path");
    CHECK(Exists(utf8).value());
}

TEST_CASE("Os files seek and tell") {
    const TempDirectory temp("gyo-os-files");
    auto file = ReadFile::Open(Utf8(temp.Write("c.bin", "hello world")));
    REQUIRE(file);
    REQUIRE(file.value().Seek(6));
    CHECK(file.value().Tell().value() == 6);
    CHECK(ReadAll(file.value()) == "world");
    CHECK(file.value().Tell().value() == 11);
    REQUIRE(file.value().Seek(0));
    CHECK(ReadAll(file.value()) == "hello world");
    // Past the end is allowed and reads nothing; beyond the platform's offset range is refused.
    REQUIRE(file.value().Seek(100));
    CHECK(ReadAll(file.value()).empty());
    const auto huge = file.value().Seek((std::numeric_limits<std::uint64_t>::max)());
    REQUIRE_FALSE(huge);
    CHECK(huge.error().code == OsErrorCode::TooLarge);
}

TEST_CASE("Os files move an open file and refuse a moved-from one") {
    const TempDirectory temp("gyo-os-files");
    auto opened = ReadFile::Open(Utf8(temp.Write("m.bin", "moved")));
    REQUIRE(opened);
    ReadFile first = std::move(opened.value());
    ReadFile second = std::move(first);
    CHECK(ReadAll(second) == "moved");
    auto other = ReadFile::Open(Utf8(temp.Write("n.bin", "assigned")));
    REQUIRE(other);
    second = std::move(other.value());
    REQUIRE(second.Seek(0));
    CHECK(ReadAll(second) == "assigned");
    std::vector<std::byte> buffer(1);
    GYO_CHECK_ASSERTS(static_cast<void>(first.Read(buffer)));
    GYO_CHECK_ASSERTS(static_cast<void>(first.Size()));
}

TEST_CASE("Os files report a missing file as NotFound with the native code") {
    const TempDirectory temp("gyo-os-files");
    const std::string missing = Utf8(temp.Path() / "missing.bin");
    auto file = ReadFile::Open(missing);
    REQUIRE_FALSE(file);
    CHECK(file.error().code == OsErrorCode::NotFound);
#if defined(_WIN32)
    const std::regex detail(R"(win32=\d+; path_bytes=(\d+))");
#else
    const std::regex detail(R"(errno=\d+; path_bytes=(\d+))");
#endif
    std::smatch match;
    REQUIRE_MESSAGE(std::regex_match(file.error().detail, match, detail), file.error().detail);
    CHECK(match[1].str() == std::to_string(missing.size()));
    CHECK(OpenError(Utf8(temp.Path() / "no-such-directory" / "x.bin")) == OsErrorCode::NotFound);
    // A parent that is a file, not a directory.
    const std::string under = Utf8(temp.Write("plain.bin", "x")) + "/x";
    CHECK(OpenError(under) == OsErrorCode::NotFound);
    CHECK_FALSE(Exists(under).value());
    CHECK(Stat(under).error().code == OsErrorCode::NotFound);
}

TEST_CASE("Os files open regular files only") {
    const TempDirectory temp("gyo-os-files");
    CHECK(OpenError(Utf8(temp.Path())) == OsErrorCode::InvalidPath);
#if defined(_WIN32)
    // A device opens through CreateFileW but is not a file.
    CHECK(OpenError("NUL") == OsErrorCode::InvalidPath);
#else
    // A FIFO without a writer: refused at once, not waited on.
    const std::string fifo = Utf8(temp.Path() / "fifo");
    REQUIRE(::mkfifo(fifo.c_str(), 0600) == 0);
    CHECK(OpenError(fifo) == OsErrorCode::InvalidPath);
#endif
}

TEST_CASE("Os files refuse invalid arguments before the OS sees them") {
    CHECK(OpenError("") == OsErrorCode::InvalidPath);
    CHECK(OpenError(std::string("a\0b", 3)) == OsErrorCode::InvalidPath);
    // A truncated sequence, overlong two-, three- and four-byte forms, a surrogate, a code point beyond
    // U+10FFFF, a stray continuation byte.
    for (const char* invalid : {"x\xE8\xB3", "\xC0\xAF", "\xE0\x80\xAF", "\xF0\x80\x80\xAF", "\xED\xA0\x80",
                                "\xF4\x90\x80\x80", "\x80x"}) {
        CHECK(OpenError(invalid) == OsErrorCode::InvalidEncoding);
        CHECK(Stat(invalid).error().code == OsErrorCode::InvalidEncoding);
        CHECK(Exists(invalid).error().code == OsErrorCode::InvalidEncoding);
        CHECK(MakeAbsolute(invalid).error().code == OsErrorCode::InvalidEncoding);
        CHECK(PathFromUtf8(invalid).error().code == OsErrorCode::InvalidEncoding);
    }
    const auto refused = ReadFile::Open("\xC0\xAF");
    REQUIRE_FALSE(refused);
    CHECK(refused.error().detail == "path_bytes=2");
}

TEST_CASE("Os files refuse a path that has no UTF-8 form") {
#if defined(_WIN32)
    // An unpaired surrogate, which NTFS allows in a name.
    const std::filesystem::path unpaired(std::wstring(L"a\xD800" L"b"));
#else
    // Bytes that are not UTF-8, which POSIX allows in a name.
    const std::filesystem::path unpaired(std::string("a\xFF" "b"));
#endif
    const auto text = PathToUtf8(unpaired);
    REQUIRE_FALSE(text);
    CHECK(text.error().code == OsErrorCode::InvalidEncoding);
}

TEST_CASE("Os files stat and check existence") {
    const TempDirectory temp("gyo-os-files");
    const auto before = std::chrono::system_clock::now().time_since_epoch();
    const auto file = temp.Write("d.bin", "12345");
    const auto after = std::chrono::system_clock::now().time_since_epoch();
    const auto status = Stat(Utf8(file));
    REQUIRE(status);
    CHECK(status.value().kind == FileKind::Regular);
    CHECK(status.value().sizeBytes == 5);
    // The write time against the clock around the write, with room for coarse file system timestamps.
    CHECK(status.value().modifiedSinceUnixEpoch >= before - std::chrono::seconds(2));
    CHECK(status.value().modifiedSinceUnixEpoch <= after + std::chrono::seconds(2));

    const auto directory = Stat(Utf8(temp.Path()));
    REQUIRE(directory);
    CHECK(directory.value().kind == FileKind::Directory);
    CHECK(directory.value().sizeBytes == 0);
    CHECK(Stat(Utf8(temp.Path() / "missing")).error().code == OsErrorCode::NotFound);

    CHECK(Exists(Utf8(file)).value());
    CHECK(Exists(Utf8(temp.Path())).value());
    CHECK_FALSE(Exists(Utf8(temp.Path() / "missing")).value());
    CHECK_FALSE(Exists(Utf8(temp.Path() / "missing" / "x")).value());
}

TEST_CASE("Os files stat follows a symbolic link") {
    const TempDirectory temp("gyo-os-files");
    const auto target = temp.Write("target.bin", "1234567");
    const auto link = temp.Path() / "link.bin";
    const auto dangling = temp.Path() / "dangling.bin";
    std::error_code error;
    std::filesystem::create_symlink(target, link, error);
    if (error) {
        // Windows needs Developer Mode or a privilege to create links.
        MESSAGE("symbolic links unavailable here: " << error.message());
        return;
    }
    std::filesystem::create_symlink(temp.Path() / "nothing.bin", dangling, error);
    REQUIRE_FALSE(error);
    const auto status = Stat(Utf8(link));
    REQUIRE(status);
    CHECK(status.value().kind == FileKind::Regular);
    CHECK(status.value().sizeBytes == 7);
    CHECK_FALSE(Exists(Utf8(dangling)).value());
}

TEST_CASE("Os files make a relative path absolute") {
    const auto absolute = MakeAbsolute("some/relative/file.txt");
    REQUIRE(absolute);
    const std::string& text = absolute.value();
    CHECK(text.find('\\') == std::string::npos);
    CHECK(text.ends_with("some/relative/file.txt"));
    CHECK(IsAbsolutePath(text));
    CHECK(Native(text).is_absolute());
    CHECK(text.starts_with(Utf8(std::filesystem::current_path())));
    // Non-ASCII stays UTF-8 on the way back from the OS.
    const auto utf8 = MakeAbsolute("\xE8\xB3\x87\xE6\x96\x99/\xC3\xA9.txt");
    REQUIRE(utf8);
    CHECK(utf8.value().ends_with("\xE8\xB3\x87\xE6\x96\x99/\xC3\xA9.txt"));

    // An absolute path stays the same file.
    const TempDirectory temp("gyo-os-files");
    const auto file = Utf8(temp.Write("e.bin", "abc"));
    const auto same = MakeAbsolute(file);
    REQUIRE(same);
    auto opened = ReadFile::Open(same.value());
    REQUIRE(opened);
    CHECK(ReadAll(opened.value()) == "abc");
}

TEST_CASE("Os files tell absolute paths per platform") {
    CHECK_FALSE(IsAbsolutePath(""));
    CHECK_FALSE(IsAbsolutePath("a/b"));
#if defined(_WIN32)
    CHECK(IsAbsolutePath("C:/x"));
    CHECK(IsAbsolutePath("c:\\x"));
    CHECK(IsAbsolutePath("\\\\server\\share"));
    CHECK_FALSE(IsAbsolutePath("/x"));
    CHECK_FALSE(IsAbsolutePath("\\x"));
    CHECK_FALSE(IsAbsolutePath("C:x"));
#else
    CHECK(IsAbsolutePath("/x"));
    CHECK_FALSE(IsAbsolutePath("C:/x"));
#endif
}

TEST_CASE("Os files convert std::filesystem paths to UTF-8 and back") {
    const std::string utf8 = "dir/\xE8\xB3\x87\xE6\x96\x99/\xC3\xA9.txt";
    const auto path = Native(utf8);
    CHECK(Utf8(path) == utf8);
    CHECK(path.filename() == Native("\xC3\xA9.txt"));
}

TEST_CASE("Os files case sensitivity default per platform") {
#if defined(_WIN32) || defined(__APPLE__)
    CHECK_FALSE(DefaultCaseSensitivePaths());
#else
    CHECK(DefaultCaseSensitivePaths());
#endif
}

#if defined(_WIN32)
// The non-ACP path case above only detects an ANSI conversion when the
// process code page is not UTF-8; record which one the runner has.
TEST_CASE("Os files record the process ANSI code page") {
    MESSAGE("GetACP() = " << ::GetACP());
    CHECK(::GetACP() != CP_UTF8);
}
#endif

// Record only: what a path longer than the Windows MAX_PATH (260) reports
// today. The long-path fix of the consumer (P1) turns the Windows case into a
// successful open; on POSIX a missing long path is NotFound.
TEST_CASE("Os files record the failure of a missing path longer than 260 characters") {
    const TempDirectory temp("gyo-os-files");
    std::filesystem::path deep = temp.Path();
    for (int i = 0; i < 8; ++i) deep /= std::string(40, static_cast<char>('a' + i));
    const std::string path = Utf8(deep / "shader.dxil");
    REQUIRE(path.size() > 300);
    const auto file = ReadFile::Open(path);
    REQUIRE_FALSE(file);
    MESSAGE("long path (" << path.size() << " bytes): " << std::string(ToString(file.error().code)) << " "
                          << file.error().detail);
    CHECK((file.error().code == OsErrorCode::NotFound || file.error().code == OsErrorCode::InvalidPath));
}
