#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/os/Files.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
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

std::string ReadAll(FileReader& file) {
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
    auto file = FileReader::Open(path);
    REQUIRE_MESSAGE(!file, "FileReader::Open accepted " << path);
    return file.error().code;
}

} // namespace

TEST_CASE("Os files read a whole file through its UTF-8 path") {
    const TempDirectory temp("gyo-os-files");
    const std::string bytes("hello\0world", 11);
    const auto path = temp.Write("a.bin", bytes);
    auto file = FileReader::Open(Utf8(path));
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
    auto file = FileReader::Open(utf8);
    REQUIRE_MESSAGE(file, "cannot open " << utf8);
    CHECK(ReadAll(file.value()) == "UTF-8 path");
    CHECK(Exists(utf8).value());
}

TEST_CASE("Os files seek and tell") {
    const TempDirectory temp("gyo-os-files");
    auto file = FileReader::Open(Utf8(temp.Write("c.bin", "hello world")));
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
    auto opened = FileReader::Open(Utf8(temp.Write("m.bin", "moved")));
    REQUIRE(opened);
    FileReader first = std::move(opened.value());
    FileReader second = std::move(first);
    CHECK(ReadAll(second) == "moved");
    auto other = FileReader::Open(Utf8(temp.Write("n.bin", "assigned")));
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
    auto file = FileReader::Open(missing);
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
    const auto refused = FileReader::Open("\xC0\xAF");
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
    auto opened = FileReader::Open(same.value());
    REQUIRE(opened);
    CHECK(ReadAll(opened.value()) == "abc");
}

// A relative path under a current directory with no UTF-8 form has no UTF-8
// absolute form either (D43, D54): InvalidEncoding, while an absolute path is
// unaffected.
TEST_CASE("Os files refuse to make a path absolute under a current directory with no UTF-8 form") {
    const TempDirectory temp("gyo-os-files");
#if defined(_WIN32)
    const auto odd = temp.Path() / std::filesystem::path(std::wstring(L"a\xD800" L"b"));
#else
    const auto odd = temp.Path() / std::filesystem::path(std::string("a\xFF" "b"));
#endif
    std::error_code error;
    std::filesystem::create_directory(odd, error);
#if defined(__APPLE__)
    // APFS and HFS+ keep names in UTF-8, so no such directory can exist here.
    CHECK(error);
    if (error) return;
#else
    REQUIRE_FALSE(error);
#endif
    struct RestoreCurrentPath final {
        std::filesystem::path previous = std::filesystem::current_path();
        ~RestoreCurrentPath() { std::filesystem::current_path(previous); }
    };
    std::optional<Engine::Base::Result<std::string, OsError>> relative;
    std::optional<Engine::Base::Result<std::string, OsError>> absolute;
    {
        const RestoreCurrentPath restore;
        std::filesystem::current_path(odd);
        relative = MakeAbsolute("x.bin");
        absolute = MakeAbsolute(Utf8(temp.Path() / "x.bin"));
    }
    REQUIRE_FALSE(*relative);
    CHECK(relative->error().code == OsErrorCode::InvalidEncoding);
    CHECK(relative->error().detail.find("path_bytes=5") != std::string::npos);
    CHECK(*absolute);
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

TEST_CASE("Os files print a path for a message in UTF-8 and never fail") {
    CHECK(PathForMessage({}).empty());
    CHECK(PathForMessage(Native("logs/run.txt")) == "logs/run.txt");
    // Characters outside the ANSI code page (Japanese, Korean) keep their UTF-8 form.
    const std::string named = "\xE8\xA8\x98\xE9\x8C\xB2/\xED\x95\x9C.jsonl";
    CHECK(PathForMessage(Native(named)) == named);
#if defined(_WIN32)
    // The platform's own form: the separators stay as the path holds them.
    CHECK(PathForMessage(std::filesystem::path(L"C:\\logs\\run.txt")) == "C:\\logs\\run.txt");
    CHECK(PathForMessage(std::filesystem::path(std::wstring(L"a\xD800" L"b"))) == "a\xEF\xBF\xBD" "b");
#else
    // Each byte that starts no UTF-8 sequence is one U+FFFD; what follows it is kept.
    CHECK(PathForMessage(std::filesystem::path(std::string("a\xFF" "b"))) == "a\xEF\xBF\xBD" "b");
    CHECK(PathForMessage(std::filesystem::path(std::string("x\xE8\xA8"))) == "x\xEF\xBF\xBD\xEF\xBF\xBD");
    CHECK(PathForMessage(std::filesystem::path(std::string("\xC0\xAF/\xED\x95\x9C"))) ==
          "\xEF\xBF\xBD\xEF\xBF\xBD/\xED\x95\x9C");
#endif
}

TEST_CASE("Os files case sensitivity default per platform") {
#if defined(_WIN32) || defined(__APPLE__)
    CHECK_FALSE(DefaultCaseSensitivePaths());
#else
    CHECK(DefaultCaseSensitivePaths());
#endif
}

#if defined(_WIN32)
// Every Windows executable of the repository declares UTF-8 as its ANSI code
// page in its manifest (os.md Invariant 11, os-utf8-codepage, D64): the
// process does not take the system's setting.
TEST_CASE("Os files run with UTF-8 as the process ANSI code page") {
    MESSAGE("GetACP() = " << ::GetACP());
    CHECK(::GetACP() == CP_UTF8);
    // The declaration itself, which this checks on a machine whose system code
    // page is UTF-8 too. A linker may merge it with its own manifest and prefix
    // the element, so only the element's name and value are matched.
    const HRSRC resource = ::FindResourceW(nullptr, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(24));
    REQUIRE_MESSAGE(resource != nullptr, "the executable has no manifest resource");
    const HGLOBAL loaded = ::LoadResource(nullptr, resource);
    REQUIRE(loaded != nullptr);
    const auto* bytes = static_cast<const char*>(::LockResource(loaded));
    REQUIRE(bytes != nullptr);
    const std::string manifest(bytes, ::SizeofResource(nullptr, resource));
    CHECK_MESSAGE(std::regex_search(manifest, std::regex("activeCodePage[^>]*>\\s*UTF-8\\s*<")), manifest);
}
#endif

// A directory tree deeper than the Windows MAX_PATH (260): eight levels of 40
// characters under a fresh temporary directory, 300+ bytes in all.
std::filesystem::path DeepPath(const std::filesystem::path& root) {
    std::filesystem::path deep = root;
    for (int i = 0; i < 8; ++i) deep /= std::string(40, static_cast<char>('a' + i));
    return deep;
}

// The form the test itself uses to create and remove files on a deep path: on
// Windows the standard library has no long-path handling of its own, so the
// setup spells the long-path prefix out.
std::filesystem::path SetupPath(const std::filesystem::path& path) {
#if defined(_WIN32)
    return std::filesystem::path(L"\\\\?\\" + std::filesystem::absolute(path).make_preferred().native());
#else
    return path;
#endif
}

// A missing path longer than the Windows MAX_PATH (260) is NotFound on every
// platform: on Windows the open reaches the file system (long-path form)
// instead of failing on the length. The error never shows the long-path form.
TEST_CASE("Os files report a missing path longer than 260 characters as not found") {
    const TempDirectory temp("gyo-os-files");
    const std::string path = Utf8(DeepPath(temp.Path()) / "shader.dxil");
    REQUIRE(path.size() > 300);
    const auto file = FileReader::Open(path);
    REQUIRE_FALSE(file);
    MESSAGE("long path (" << path.size() << " bytes): " << std::string(ToString(file.error().code)) << " "
                          << file.error().detail);
    CHECK(file.error().code == OsErrorCode::NotFound);
    CHECK(file.error().detail.find("path_bytes=" + std::to_string(path.size())) != std::string::npos);
    CHECK(file.error().detail.find('?') == std::string::npos);
    CHECK_FALSE(Exists(path).value());
}

// A file of a package unpacked under a folder deep enough that its path
// exceeds 260 characters opens, reads and stats through
// its plain UTF-8 path, in '/' form and, on Windows, in '\' form.
TEST_CASE("Os files read an existing file under a path longer than 300 characters") {
    const TempDirectory temp("gyo-os-files");
    const auto deep = DeepPath(temp.Path());
    const std::string bytes("long\0path", 10);
    std::filesystem::create_directories(SetupPath(deep));
    std::ofstream(SetupPath(deep / "shader.dxil"), std::ios::binary)
        .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    std::string path = Utf8(deep / "shader.dxil");
    REQUIRE(path.size() > 300);
    std::vector<std::string> forms{path};
#if defined(_WIN32)
    forms.push_back(path);
    std::replace(forms.back().begin(), forms.back().end(), '/', '\\');
    // A caller's own long-path form is passed on unchanged.
    forms.push_back("\\\\?\\" + forms.back());
#endif
    for (const auto& form : forms) {
        CAPTURE(form);
        auto file = FileReader::Open(form);
        REQUIRE_MESSAGE(file, "cannot open a " << form.size() << "-byte path: "
                                               << (file ? std::string{} : file.error().detail));
        CHECK(file.value().Size().value() == bytes.size());
        CHECK(ReadAll(file.value()) == bytes);
        const auto status = Stat(form);
        REQUIRE(status);
        CHECK(status.value().kind == FileKind::Regular);
        CHECK(status.value().sizeBytes == bytes.size());
        CHECK(Exists(form).value());
    }
    const auto folder = Stat(Utf8(deep));
    REQUIRE(folder);
    CHECK(folder.value().kind == FileKind::Directory);
    std::error_code ignored;
    std::filesystem::remove_all(SetupPath(temp.Path() / std::string(40, 'a')), ignored);
}
