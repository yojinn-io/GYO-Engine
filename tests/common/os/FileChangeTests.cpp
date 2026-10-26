#include <doctest/doctest.h>

#include "engine/os/Files.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdlib>
#endif

// The primitives the editor's file service moved onto (docs/architecture/os.md,
// Files; os-subsystem os-tool-files, D56): replacing a file by renaming,
// removing a file, resolving links in a path, comparing path components and
// the font directories. The reference for ResolvedPath is the standard
// library's weakly_canonical, which the editor called before (Design
// Invariant 9).

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
        std::filesystem::remove_all(SetupPath(path_), ignored);
    }
    const std::filesystem::path& Path() const noexcept { return path_; }

    // The form the test uses to set up and remove a deep tree: on Windows the
    // standard library has no long-path handling of its own.
    static std::filesystem::path SetupPath(const std::filesystem::path& path) {
#if defined(_WIN32)
        return std::filesystem::path(L"\\\\?\\" + std::filesystem::absolute(path).make_preferred().native());
#else
        return path;
#endif
    }

private:
    std::filesystem::path path_;
};

void WriteBytes(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream(TempDirectory::SetupPath(path), std::ios::binary) << bytes;
}

std::string ReadBytes(const std::filesystem::path& path) {
    std::ifstream file(TempDirectory::SetupPath(path), std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::filesystem::path DeepPath(const std::filesystem::path& root) {
    std::filesystem::path deep = root;
    for (int i = 0; i < 8; ++i) deep /= std::string(40, static_cast<char>('a' + i));
    return deep;
}

} // namespace

TEST_CASE("Os files rename a file over an existing one in one step") {
    TempDirectory dir("gyo-os-rename");
    const auto from = dir.Path() / "export.json.tmp";
    const auto to = dir.Path() / "export.json";
    WriteBytes(to, "old");
    WriteBytes(from, "new");
    REQUIRE(RenameReplacing(Utf8(from), Utf8(to)));
    CHECK(ReadBytes(to) == "new");
    CHECK_FALSE(std::filesystem::exists(from));

    // A target that does not exist yet is made.
    const auto fresh = dir.Path() / "fresh.json";
    WriteBytes(from, "fresh");
    REQUIRE(RenameReplacing(Utf8(from), Utf8(fresh)));
    CHECK(ReadBytes(fresh) == "fresh");
}

TEST_CASE("Os files keep the target when renaming over it fails") {
    TempDirectory dir("gyo-os-rename-fail");
    const auto to = dir.Path() / "export.json";
    WriteBytes(to, "kept");
    const auto missing = RenameReplacing(Utf8(dir.Path() / "absent.tmp"), Utf8(to));
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == OsErrorCode::NotFound);
    CHECK(ReadBytes(to) == "kept");

    // A directory is not replaced by a file.
    const auto from = dir.Path() / "export.json.tmp";
    WriteBytes(from, "new");
    const auto folder = dir.Path() / "folder";
    std::filesystem::create_directories(folder / "inner");
    const auto onDirectory = RenameReplacing(Utf8(from), Utf8(folder));
    REQUIRE_FALSE(onDirectory);
    CHECK(std::filesystem::is_directory(folder / "inner"));
    CHECK(ReadBytes(from) == "new");
}

TEST_CASE("Os files rename and remove under a path longer than 300 characters") {
    TempDirectory dir("gyo-os-rename-long");
    const auto deep = DeepPath(dir.Path());
    std::filesystem::create_directories(TempDirectory::SetupPath(deep));
    const auto from = deep / "menu.json.tmp";
    const auto to = deep / "menu.json";
    REQUIRE(Utf8(to).size() > 300);
    WriteBytes(to, "old");
    WriteBytes(from, "new");
    REQUIRE_MESSAGE(RenameReplacing(Utf8(from), Utf8(to)), "rename under a deep path");
    CHECK(ReadBytes(to) == "new");
    REQUIRE(RemoveFile(Utf8(to)));
    CHECK_FALSE(Exists(Utf8(to)).value());
}

TEST_CASE("Os files remove a file, not a directory") {
    TempDirectory dir("gyo-os-remove");
    const auto file = dir.Path() / "a.tmp";
    WriteBytes(file, "x");
    REQUIRE(RemoveFile(Utf8(file)));
    CHECK_FALSE(std::filesystem::exists(file));

    const auto missing = RemoveFile(Utf8(file));
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == OsErrorCode::NotFound);

    const auto folder = dir.Path() / "folder";
    std::filesystem::create_directories(folder);
    const auto onDirectory = RemoveFile(Utf8(folder));
    REQUIRE_FALSE(onDirectory);
    CHECK(onDirectory.error().code == OsErrorCode::InvalidPath);
    CHECK(std::filesystem::is_directory(folder));
}

TEST_CASE("Os files resolve a path as std::filesystem::weakly_canonical does") {
    TempDirectory dir("gyo-os-resolve");
    std::filesystem::create_directories(dir.Path() / "real" / "sub");
    WriteBytes(dir.Path() / "real" / "file.txt", "x");
    std::vector<std::filesystem::path> inputs{
        dir.Path(),
        dir.Path() / "real",
        dir.Path() / "real" / "file.txt",
        dir.Path() / "real" / "missing" / "x.json",
        dir.Path() / "real" / "missing" / ".." / "y.json",
        dir.Path() / "real" / "sub" / ".." / "file.txt",
        dir.Path() / "real" / "file.txt" / "under-a-file",
        // realpath takes these on macOS, stat does not: the existing prefix is the file.
        dir.Path() / "real" / "file.txt" / "",
        dir.Path() / "real" / "file.txt" / ".",
        dir.Path() / "absent" / "deeper" / "z.json",
        dir.Path() / "real" / "sub" / "",
    };
    std::error_code linkError;
    std::filesystem::create_directory_symlink(dir.Path() / "real" / "sub", dir.Path() / "link", linkError);
    if (linkError) {
        // Windows needs Developer Mode or a privilege to create links (as in FilesTests.cpp).
        MESSAGE("symbolic links unavailable here: " << linkError.message());
    } else {
        inputs.push_back(dir.Path() / "link");
        inputs.push_back(dir.Path() / "link" / "missing" / "x.json");
        inputs.push_back(dir.Path() / "link" / ".." / "file.txt");
        inputs.push_back(dir.Path() / "link" / ".." / "missing" / "w.json");
    }
    for (const auto& input : inputs) {
        // CAPTURE evaluates its expression again when it reports a failure; Utf8 asserts, so it gets a plain value.
        const std::string text = Utf8(input);
        CAPTURE(text);
        const auto resolved = ResolvedPath(text);
        REQUIRE_MESSAGE(resolved, (resolved ? std::string{} : Engine::Base::Describe(resolved.error())));
        CHECK(resolved.value() == Utf8(std::filesystem::weakly_canonical(input)));
        CHECK(resolved.value().find('\\') == std::string::npos);
        CHECK(IsAbsolutePath(resolved.value()));
    }
}

// Nothing exists after the root: the root stays as written, with the rest
// appended (the walk back must stop there, not resolve or drop the root).
TEST_CASE("Os files resolve a path whose only existing part is the root") {
    const auto root = std::filesystem::temp_directory_path().root_path();
    for (const auto& input : {root / "gyo-no-such-top-directory" / "x.json", root / "gyo-no-such-top-directory"}) {
        const std::string text = Utf8(input);
        CAPTURE(text);
        const auto resolved = ResolvedPath(text);
        REQUIRE(resolved);
        CHECK(resolved.value() == Utf8(std::filesystem::weakly_canonical(input)));
    }
}

#if defined(_WIN32)
// A drive letter that names a directory (as subst makes one, here with
// DefineDosDeviceW, which needs no privilege): a resolved prefix is the
// directory's real path, while a drive with nothing existing after it stays.
TEST_CASE("Os files resolve a path on a drive letter that names a directory") {
    TempDirectory dir("gyo-os-resolve-drive");
    std::filesystem::create_directories(dir.Path() / "existing");
    const DWORD used = ::GetLogicalDrives();
    wchar_t letter = 0;
    for (wchar_t candidate = L'Z'; candidate >= L'G'; --candidate) {
        if ((used & (1u << (candidate - L'A'))) == 0) {
            letter = candidate;
            break;
        }
    }
    REQUIRE(letter != 0);
    const std::wstring drive{letter, L':'};
    const std::wstring target = std::filesystem::absolute(dir.Path()).native();
    if (!::DefineDosDeviceW(0, drive.c_str(), target.c_str())) {
        MESSAGE("DefineDosDeviceW failed here: " << ::GetLastError());
        return;
    }
    struct RemoveDrive final {
        const std::wstring& drive;
        const std::wstring& target;
        ~RemoveDrive() {
            ::DefineDosDeviceW(DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE, drive.c_str(), target.c_str());
        }
    } removeDrive{drive, target};
    const std::string driveText = Utf8(std::filesystem::path(drive + L"/"));
    for (const std::string& relative : {std::string("missing/x.json"), std::string("existing/missing/y.json"),
                                        std::string("existing"), std::string()}) {
        const std::string text = driveText + relative;
        CAPTURE(text);
        const auto resolved = ResolvedPath(text);
        REQUIRE(resolved);
        CHECK(resolved.value() == Utf8(std::filesystem::weakly_canonical(std::filesystem::path(
                                      std::u8string(text.begin(), text.end())))));
    }
}

// A UNC path through the administrative share of the temporary directory's
// drive, when this machine reaches it (as tests/common/core/io/UncProbe.cpp).
TEST_CASE("Os files resolve a UNC path as weakly_canonical does") {
    TempDirectory dir("gyo-os-resolve-unc");
    std::filesystem::create_directories(dir.Path() / "existing");
    const std::string local = Utf8(std::filesystem::absolute(dir.Path()));
    REQUIRE(local.size() > 3);
    const std::string share = "//localhost/" + local.substr(0, 1) + "$";
    std::error_code error;
    if (!std::filesystem::is_directory(std::filesystem::path(std::u8string(share.begin(), share.end())) / "", error)) {
        MESSAGE(share << " is not reachable here");
        return;
    }
    for (const std::string& relative : {std::string("/existing"), std::string("/existing/missing/x.json")}) {
        const std::string text = share + local.substr(2) + relative;
        CAPTURE(text);
        const auto resolved = ResolvedPath(text);
        REQUIRE(resolved);
        CHECK(resolved.value() == Utf8(std::filesystem::weakly_canonical(std::filesystem::path(
                                      std::u8string(text.begin(), text.end())))));
    }
}
#endif

TEST_CASE("Os files resolve a relative path against the current directory") {
    const auto resolved = ResolvedPath("gyo-no-such-directory/x.json");
    REQUIRE(resolved);
    CHECK(resolved.value() ==
          Utf8(std::filesystem::weakly_canonical(std::filesystem::current_path() / "gyo-no-such-directory/x.json")));
}

TEST_CASE("Os files compare path components as the platform names entries") {
    CHECK(SamePathComponent("assets", "assets").value());
    CHECK_FALSE(SamePathComponent("assets", "asset").value());
    CHECK(SamePathComponent("", "").value());
    CHECK_FALSE(SamePathComponent("", "a").value());
#if defined(_WIN32)
    // As NTFS compares names: case ignored by the OS's uppercase table, also
    // beyond ASCII (Greek, Latin-1).
    CHECK(SamePathComponent("Assets", "aSSETS").value());
    CHECK(SamePathComponent("\xCE\xA9", "\xCF\x89").value());   // Ω and ω
    CHECK(SamePathComponent("\xC3\x89t\xC3\xA9", "\xC3\xA9T\xC3\x89").value());   // Été and éTÉ
    CHECK_FALSE(SamePathComponent("\xCE\xA9", "O").value());
#else
    // Byte for byte, macOS included (D56).
    CHECK_FALSE(SamePathComponent("Assets", "aSSETS").value());
    CHECK_FALSE(SamePathComponent("\xCE\xA9", "\xCF\x89").value());
#endif
    const auto invalid = SamePathComponent("a\xFF", "a");
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().code == OsErrorCode::InvalidEncoding);
}

TEST_CASE("Os files list the font directories that exist, system first") {
    const auto directories = FontDirectories();
    REQUIRE(directories);
    for (const auto& directory : directories.value()) {
        CAPTURE(directory);
        CHECK(IsAbsolutePath(directory));
        const auto status = Stat(directory);
        REQUIRE(status);
        CHECK(status.value().kind == FileKind::Directory);
    }
    std::vector<std::string> expected;
#if defined(_WIN32)
    // The system Windows directory's Fonts first; it always exists.
    wchar_t windows[MAX_PATH + 1] = {};
    const UINT length = ::GetSystemWindowsDirectoryW(windows, MAX_PATH + 1);
    REQUIRE(length > 0);
    expected.push_back(Utf8(std::filesystem::path(std::wstring(windows, length)) / "Fonts"));
    REQUIRE_FALSE(directories.value().empty());
    CHECK(directories.value().front() == expected.front());
#elif defined(__APPLE__)
    expected = {"/System/Library/Fonts", "/Library/Fonts"};
    if (const char* home = std::getenv("HOME")) expected.push_back(std::string(home) + "/Library/Fonts");
    REQUIRE_FALSE(directories.value().empty());
    CHECK(directories.value().front() == "/System/Library/Fonts");
#else
    expected = {"/usr/share/fonts", "/usr/local/share/fonts"};
    if (const char* data = std::getenv("XDG_DATA_HOME"); data != nullptr && data[0] == '/') {
        expected.push_back(std::string(data) + "/fonts");
    } else if (const char* home = std::getenv("HOME")) {
        expected.push_back(std::string(home) + "/.local/share/fonts");
    }
#endif
    // Every directory listed is one of the documented ones, in their order (the user's after the system's).
    std::size_t at = 0;
    for (const auto& directory : directories.value()) {
        while (at < expected.size() && expected[at] != directory) ++at;
        CHECK_MESSAGE(at < expected.size(), directory << " is not a documented font directory in its place");
    }
}
