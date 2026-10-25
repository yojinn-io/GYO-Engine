#include <doctest/doctest.h>

#include "engine/os/Files.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

using namespace Engine::Os;
using namespace Engine::Os::Files;

namespace {

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
    auto file = ReadFile::Open(PathToUtf8(path));
    REQUIRE(file);
    CHECK(file.value().Size().value() == 11);
    CHECK(ReadAll(file.value()) == bytes);
    // At the end of the file a read returns 0 bytes.
    std::vector<std::byte> more(8);
    CHECK(file.value().Read(more).value() == 0);
}

TEST_CASE("Os files open a path outside the ANSI code page") {
    // Japanese and Traditional Chinese characters, which a Western code page cannot represent.
    const TempDirectory temp("gyo-os-files");
    const auto path = temp.Write(PathFromUtf8("\xE8\xB3\x87\xE6\x96\x99\xE5\xA4\xBE-\xC3\xA9\xE6\x97\xA5\xE6\x9C\xAC/b.txt"),
                                 "UTF-8 path");
    const std::string utf8 = PathToUtf8(path);
    CHECK(utf8.find("\xE8\xB3\x87\xE6\x96\x99\xE5\xA4\xBE") != std::string::npos);
    auto file = ReadFile::Open(utf8);
    REQUIRE_MESSAGE(file, "cannot open " << utf8);
    CHECK(ReadAll(file.value()) == "UTF-8 path");
    CHECK(Exists(utf8).value());
}

TEST_CASE("Os files seek and tell") {
    const TempDirectory temp("gyo-os-files");
    auto file = ReadFile::Open(PathToUtf8(temp.Write("c.bin", "hello world")));
    REQUIRE(file);
    REQUIRE(file.value().Seek(6));
    CHECK(file.value().Tell().value() == 6);
    CHECK(ReadAll(file.value()) == "world");
    CHECK(file.value().Tell().value() == 11);
    REQUIRE(file.value().Seek(0));
    CHECK(ReadAll(file.value()) == "hello world");
}

TEST_CASE("Os files report a missing file as NotFound with the native code") {
    const TempDirectory temp("gyo-os-files");
    const std::string missing = PathToUtf8(temp.Path() / "missing.bin");
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
    CHECK(OpenError(PathToUtf8(temp.Path() / "no-such-directory" / "x.bin")) == OsErrorCode::NotFound);
}

TEST_CASE("Os files refuse to open a directory") {
    const TempDirectory temp("gyo-os-files");
    CHECK(OpenError(PathToUtf8(temp.Path())) == OsErrorCode::InvalidPath);
}

TEST_CASE("Os files refuse invalid arguments before the OS sees them") {
    CHECK(OpenError("") == OsErrorCode::InvalidPath);
    CHECK(OpenError(std::string("a\0b", 3)) == OsErrorCode::InvalidPath);
    // A truncated sequence, an overlong '/', a surrogate, a code point beyond U+10FFFF, a stray continuation byte.
    for (const char* invalid : {"x\xE8\xB3", "\xC0\xAF", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\x80x"}) {
        CHECK(OpenError(invalid) == OsErrorCode::InvalidEncoding);
        CHECK(Stat(invalid).error().code == OsErrorCode::InvalidEncoding);
        CHECK(Exists(invalid).error().code == OsErrorCode::InvalidEncoding);
    }
    const auto refused = ReadFile::Open("\xC0\xAF");
    REQUIRE_FALSE(refused);
    CHECK(refused.error().detail == "path_bytes=2");
}

TEST_CASE("Os files stat and check existence") {
    const TempDirectory temp("gyo-os-files");
    const auto file = temp.Write("d.bin", "12345");
    const auto status = Stat(PathToUtf8(file));
    REQUIRE(status);
    CHECK(status.value().kind == FileKind::Regular);
    CHECK(status.value().sizeBytes == 5);
    // Written a moment ago: after 2020-01-01 and not in the far future.
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    CHECK(status.value().modifiedSinceUnixEpoch > std::chrono::seconds(1577836800));
    CHECK(status.value().modifiedSinceUnixEpoch < now + std::chrono::hours(24));

    const auto directory = Stat(PathToUtf8(temp.Path()));
    REQUIRE(directory);
    CHECK(directory.value().kind == FileKind::Directory);
    CHECK(Stat(PathToUtf8(temp.Path() / "missing")).error().code == OsErrorCode::NotFound);

    CHECK(Exists(PathToUtf8(file)).value());
    CHECK(Exists(PathToUtf8(temp.Path())).value());
    CHECK_FALSE(Exists(PathToUtf8(temp.Path() / "missing")).value());
    CHECK_FALSE(Exists(PathToUtf8(temp.Path() / "missing" / "x")).value());
}

TEST_CASE("Os files make a relative path absolute") {
    const auto absolute = MakeAbsolute("some/relative/file.txt");
    REQUIRE(absolute);
    const std::string& text = absolute.value();
    CHECK(text.find('\\') == std::string::npos);
    CHECK(text.ends_with("some/relative/file.txt"));
    CHECK(std::filesystem::path(PathFromUtf8(text)).is_absolute());
    CHECK(text.starts_with(PathToUtf8(std::filesystem::current_path())));

    // An absolute path stays the same file.
    const TempDirectory temp("gyo-os-files");
    const auto file = PathToUtf8(temp.Write("e.bin", "abc"));
    const auto same = MakeAbsolute(file);
    REQUIRE(same);
    auto opened = ReadFile::Open(same.value());
    REQUIRE(opened);
    CHECK(ReadAll(opened.value()) == "abc");
}

TEST_CASE("Os files convert std::filesystem paths to UTF-8 and back") {
    const std::string utf8 = "dir/\xE8\xB3\x87\xE6\x96\x99/\xC3\xA9.txt";
    const auto path = PathFromUtf8(utf8);
    CHECK(PathToUtf8(path) == utf8);
    CHECK(path.filename() == PathFromUtf8("\xC3\xA9.txt"));
}

TEST_CASE("Os files case sensitivity default per platform") {
#if defined(_WIN32) || defined(__APPLE__)
    CHECK_FALSE(DefaultCaseSensitivePaths());
#else
    CHECK(DefaultCaseSensitivePaths());
#endif
}

// Record only: what a path longer than the Windows MAX_PATH (260) reports
// today. The long-path fix of the consumer (P1) turns the Windows case into a
// successful open; on POSIX a missing long path is NotFound.
TEST_CASE("Os files record the failure of a missing path longer than 260 characters") {
    const TempDirectory temp("gyo-os-files");
    std::filesystem::path deep = temp.Path();
    for (int i = 0; i < 8; ++i) deep /= std::string(40, static_cast<char>('a' + i));
    const std::string path = PathToUtf8(deep / "shader.dxil");
    REQUIRE(path.size() > 300);
    const auto file = ReadFile::Open(path);
    REQUIRE_FALSE(file);
    MESSAGE("long path (" << path.size() << " bytes): " << std::string(ToString(file.error().code)) << " "
                          << file.error().detail);
    CHECK((file.error().code == OsErrorCode::NotFound || file.error().code == OsErrorCode::InvalidPath));
}
