#include <doctest/doctest.h>

#include "engine/os/Files.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <optional>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#else
#include <cstdlib>

#include <sys/stat.h>
#endif

// The primitives the editor's file service moved onto (docs/architecture/os.md,
// Files; os-subsystem os-tool-files, D56): replacing a file by renaming,
// removing a file, resolving links in a path, comparing path components and
// the font directories. Also the user data directory
// (v8-arch-gyo2-os-user-data). The reference for ResolvedPath is the standard
// library's weakly_canonical, which the editor called before (Design
// Invariant 9).

using namespace GYO::Engine::OS;
using namespace GYO::Engine::OS::Files;

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

TEST_CASE("OS files rename a file over an existing one in one step") {
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

TEST_CASE("OS files keep the target when renaming over it fails") {
    TempDirectory dir("gyo-os-rename-fail");
    const auto to = dir.Path() / "export.json";
    WriteBytes(to, "kept");
    const auto missing = RenameReplacing(Utf8(dir.Path() / "absent.tmp"), Utf8(to));
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == OSErrorCode::NotFound);
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

TEST_CASE("OS files rename and remove under a path longer than 300 characters") {
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

TEST_CASE("OS files remove a file, not a directory") {
    TempDirectory dir("gyo-os-remove");
    const auto file = dir.Path() / "a.tmp";
    WriteBytes(file, "x");
    REQUIRE(RemoveFile(Utf8(file)));
    CHECK_FALSE(std::filesystem::exists(file));

    const auto missing = RemoveFile(Utf8(file));
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == OSErrorCode::NotFound);

    const auto folder = dir.Path() / "folder";
    std::filesystem::create_directories(folder);
    const auto onDirectory = RemoveFile(Utf8(folder));
    REQUIRE_FALSE(onDirectory);
    CHECK(onDirectory.error().code == OSErrorCode::InvalidPath);
    CHECK(std::filesystem::is_directory(folder));
}

TEST_CASE("OS files resolve a path as std::filesystem::weakly_canonical does") {
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
        REQUIRE_MESSAGE(resolved, (resolved ? std::string{} : GYO::Engine::Base::Describe(resolved.error())));
        CHECK(resolved.value() == Utf8(std::filesystem::weakly_canonical(input)));
        CHECK(resolved.value().find('\\') == std::string::npos);
        CHECK(IsAbsolutePath(resolved.value()));
    }
}

// Nothing exists after the root: the root stays as written, with the rest
// appended (the walk back must stop there, not resolve or drop the root).
TEST_CASE("OS files resolve a path whose only existing part is the root") {
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
TEST_CASE("OS files resolve a path on a drive letter that names a directory") {
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
TEST_CASE("OS files resolve a UNC path as weakly_canonical does") {
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

TEST_CASE("OS files resolve a relative path against the current directory") {
    const auto resolved = ResolvedPath("gyo-no-such-directory/x.json");
    REQUIRE(resolved);
    CHECK(resolved.value() ==
          Utf8(std::filesystem::weakly_canonical(std::filesystem::current_path() / "gyo-no-such-directory/x.json")));
}

TEST_CASE("OS files compare path components as the platform names entries") {
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
    CHECK(invalid.error().code == OSErrorCode::InvalidEncoding);
}

TEST_CASE("OS files list the font directories that exist, system first") {
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
    // Then the user's, as the contract lists them (the D201 run had user fonts installed).
    if (const DWORD size = ::GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0); size > 0) {
        std::wstring local(size, L'\0');
        const DWORD got = ::GetEnvironmentVariableW(L"LOCALAPPDATA", local.data(), size);
        if (got > 0 && got < size) {
            local.resize(got);
            expected.push_back(Utf8(std::filesystem::path(local) / "Microsoft" / "Windows" / "Fonts"));
        }
    }
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

// ---- The user data directory (v8-arch-gyo2-os-user-data; declaration
// os-user-data-declaration.zh-Hant.md, C1-C10). On POSIX every case points HOME
// (and XDG_DATA_HOME) at a temporary directory and runs in a temporary current
// directory, so no case, and no mutant of it, touches the developer's own
// directories or the repository; on Windows the base cannot be redirected, so
// the cases use names of their own under %APPDATA% and remove them.

namespace {

#if !defined(_WIN32)
// Sets or unsets an environment variable and restores the previous value afterwards.
class ScopedEnvironment final {
public:
    ScopedEnvironment(const char* name, const char* value) : name_(name) {
        if (const char* previous = std::getenv(name)) previous_ = previous;
        Set(value);
    }
    ScopedEnvironment(const ScopedEnvironment&) = delete;
    ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;
    ~ScopedEnvironment() { Set(previous_ ? previous_->c_str() : nullptr); }

private:
    void Set(const char* value) const {
        if (value) setenv(name_, value, 1);
        else unsetenv(name_);
    }
    const char* name_;
    std::optional<std::string> previous_;
};

// Runs in a temporary current directory and goes back afterwards, also when a
// REQUIRE ends the case; anything made relative to it is checked by the case.
class ScopedCurrentPath final {
public:
    ScopedCurrentPath() : directory_("gyo-os-user-data-cwd"), previous_(std::filesystem::current_path()) {
        std::filesystem::current_path(directory_.Path());
    }
    ScopedCurrentPath(const ScopedCurrentPath&) = delete;
    ScopedCurrentPath& operator=(const ScopedCurrentPath&) = delete;
    ~ScopedCurrentPath() {
        std::error_code ignored;
        std::filesystem::current_path(previous_, ignored);
    }
    bool Empty() const { return std::filesystem::is_empty(directory_.Path()); }

private:
    TempDirectory directory_;
    std::filesystem::path previous_;
};

// A temporary HOME with XDG_DATA_HOME unset, in a temporary current directory.
class ScopedHome final {
public:
    explicit ScopedHome(const char* name)
        : home_(name), text_(Utf8(home_.Path())), setHome_("HOME", text_.c_str()), noData_("XDG_DATA_HOME", nullptr) {}
    const std::filesystem::path& Path() const noexcept { return home_.Path(); }
    const std::string& Text() const noexcept { return text_; }
    bool CurrentDirectoryEmpty() const { return cwd_.Empty(); }

private:
    ScopedCurrentPath cwd_;
    TempDirectory home_;
    std::string text_;
    ScopedEnvironment setHome_;
    ScopedEnvironment noData_;
};

// The base the contract names for a HOME of home, with XDG_DATA_HOME unset.
std::string BaseForHome(const std::string& home) {
#if defined(__APPLE__)
    return home + "/Library/Application Support";
#else
    return home + "/.local/share";
#endif
}
#else
std::optional<std::string> AppDataBaseOrNothing() {
    wchar_t path[MAX_PATH] = {};
    if (FAILED(::SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) return std::nullopt;
    auto text = PathToUtf8(std::filesystem::path(path));
    if (!text) return std::nullopt;
    std::string base = std::move(text.value());
    std::replace(base.begin(), base.end(), '\\', '/');
    return base;
}

std::string AppDataBase() {
    auto base = AppDataBaseOrNothing();
    REQUIRE_MESSAGE(base, "SHGetFolderPathW failed");
    return *base;
}

// A name no other run uses, removed with everything under it afterwards.
class ScopedAppDataName final {
public:
    ScopedAppDataName() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        name_ = "gyo-os-user-data-test-" + std::to_string(stamp);
    }
    ScopedAppDataName(const ScopedAppDataName&) = delete;
    ScopedAppDataName& operator=(const ScopedAppDataName&) = delete;
    ~ScopedAppDataName() {
        const auto base = AppDataBaseOrNothing();
        if (!base) return;
        std::error_code ignored;
        if (auto path = PathFromUtf8(*base + "/" + name_)) std::filesystem::remove_all(path.value(), ignored);
    }
    const std::string& Name() const noexcept { return name_; }

private:
    std::string name_;
};
#endif

void CheckIsDirectory(const std::string& path) {
    CAPTURE(path);
    CHECK(IsAbsolutePath(path));
    CHECK(path.back() != '/');
    CHECK(path.find('\\') == std::string::npos);
    const auto status = Stat(path);
    REQUIRE(status);
    CHECK(status.value().kind == FileKind::Directory);
}

} // namespace

TEST_CASE("OS files make the user data directory under the platform base") {
#if defined(_WIN32)
    const ScopedAppDataName organization;
    const std::string base = AppDataBase();
    const std::string& org = organization.Name();
#else
    const ScopedHome home("gyo-os-user-data-home");
    const std::string base = BaseForHome(home.Text());
    const std::string org = "GYO Test Org";
#endif
    const auto made = UserDataDirectory(org, "Test App");
    REQUIRE(made);
    CHECK(made.value() == base + "/" + org + "/Test App");
    CheckIsDirectory(made.value());

    // A directory that exists already is fine, and the answer is the same.
    const auto again = UserDataDirectory(org, "Test App");
    REQUIRE(again);
    CHECK(again.value() == made.value());

    // No organization: the application's directory is right under the base.
#if defined(_WIN32)
    const auto bare = UserDataDirectory("", org);
    REQUIRE(bare);
    CHECK(bare.value() == base + "/" + org);
#else
    const auto bare = UserDataDirectory("", "Bare App");
    REQUIRE(bare);
    CHECK(bare.value() == base + "/Bare App");
    CHECK(home.CurrentDirectoryEmpty());
#endif
    CheckIsDirectory(bare.value());
}

TEST_CASE("OS files refuse user data names that are not one path component") {
#if defined(_WIN32)
    const ScopedAppDataName organization;
    const std::string org = organization.Name();
#else
    const ScopedHome home("gyo-os-user-data-names");
    const std::string org = "GYO Test Org";
#endif
    const std::vector<std::string> invalid = {".", "..", "a/b", "a\\b", "/", std::string("a\0b", 3)};
    for (const auto& name : invalid) {
        CAPTURE(name);
        const auto asApplication = UserDataDirectory(org, name);
        REQUIRE_FALSE(asApplication);
        CHECK(asApplication.error().code == OSErrorCode::InvalidPath);
        const auto asOrganization = UserDataDirectory(name, "Test App");
        REQUIRE_FALSE(asOrganization);
        CHECK(asOrganization.error().code == OSErrorCode::InvalidPath);
    }
    const auto emptyApplication = UserDataDirectory(org, "");
    REQUIRE_FALSE(emptyApplication);
    CHECK(emptyApplication.error().code == OSErrorCode::InvalidPath);
    const auto notUtf8 = UserDataDirectory(org, "a\xFF");
    REQUIRE_FALSE(notUtf8);
    CHECK(notUtf8.error().code == OSErrorCode::InvalidEncoding);
#if !defined(_WIN32)
    // Nothing was made: the names are refused before any directory is.
    CHECK(std::filesystem::is_empty(home.Path()));
    CHECK(home.CurrentDirectoryEmpty());
#endif
}

#if !defined(_WIN32)
TEST_CASE("OS files fail without a home for the user data directory") {
    const ScopedCurrentPath cwd;
    const ScopedEnvironment noData("XDG_DATA_HOME", nullptr);
    {
        const ScopedEnvironment noHome("HOME", nullptr);
        const auto withoutHome = UserDataDirectory("GYO Test Org", "Test App");
        REQUIRE_FALSE(withoutHome);
        CHECK(withoutHome.error().code == OSErrorCode::IOFailed);
    }
    {
        const ScopedEnvironment setHome("HOME", "relative-home");
        const auto relativeHome = UserDataDirectory("GYO Test Org", "Test App");
        REQUIRE_FALSE(relativeHome);
        CHECK(relativeHome.error().code == OSErrorCode::IOFailed);
    }
    // Nothing was made relative to the current directory.
    CHECK(cwd.Empty());
}

TEST_CASE("OS files drop the trailing separator of the user data base") {
    const ScopedCurrentPath cwd;
    const TempDirectory home("gyo-os-user-data-slash");
    const std::string homeText = Utf8(home.Path());
    const ScopedEnvironment noData("XDG_DATA_HOME", nullptr);
    const ScopedEnvironment setHome("HOME", (homeText + "//").c_str());
    const auto made = UserDataDirectory("GYO Test Org", "Test App");
    REQUIRE(made);
    CHECK(made.value() == BaseForHome(homeText) + "/GYO Test Org/Test App");
    CHECK(made.value().find("//") == std::string::npos);
}

TEST_CASE("OS files make new user data directories for the owner only") {
    namespace fs = std::filesystem;
    // The umask most systems use, so 0700 and the default differ whatever the runner's is.
    const mode_t previousMask = ::umask(022);
    struct RestoreMask final {
        mode_t mask;
        ~RestoreMask() { ::umask(mask); }
    } restoreMask{previousMask};
    const ScopedHome home("gyo-os-user-data-access");
    // The first level under HOME exists already, readable by others: it stays so.
#if defined(__APPLE__)
    const fs::path existing = home.Path() / "Library";
#else
    const fs::path existing = home.Path() / ".local";
#endif
    fs::create_directory(existing);
    fs::permissions(existing, fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                                  fs::perms::others_read | fs::perms::others_exec);
    const auto before = fs::status(existing).permissions();

    const auto made = UserDataDirectory("GYO Test Org", "Test App");
    REQUIRE(made);
    CHECK(fs::status(existing).permissions() == before);
    // Every level made here, from the one under the existing level to the application's.
    REQUIRE(made.value().starts_with(Utf8(existing) + "/"));
    const auto madePath = PathFromUtf8(made.value());
    REQUIRE(madePath);
    for (fs::path level = madePath.value(); level != existing; level = level.parent_path()) {
        CAPTURE(level.string());
        CHECK(fs::status(level).permissions() == fs::perms::owner_all);
    }

    // The public CreateDirectories keeps 0777 less the umask (Design Invariant 9).
    const fs::path shared = home.Path() / "shared";
    REQUIRE(CreateDirectories(Utf8(shared)));
    CHECK(static_cast<mode_t>(fs::status(shared).permissions()) == (0777 & ~mode_t{022}));
}

// The font directories read HOME (and on Linux XDG_DATA_HOME) through the same
// helpers as the user data base; the user's font directory is listed last.
TEST_CASE("OS files list the user's font directory from the environment") {
    const ScopedHome home("gyo-os-user-data-fonts");
#if defined(__APPLE__)
    const std::filesystem::path fonts = home.Path() / "Library" / "Fonts";
    std::filesystem::create_directories(fonts);
    const auto directories = FontDirectories();
    REQUIRE(directories);
    REQUIRE_FALSE(directories.value().empty());
    CHECK(directories.value().back() == Utf8(fonts));
#else
    const std::filesystem::path homeFonts = home.Path() / ".local" / "share" / "fonts";
    std::filesystem::create_directories(homeFonts);
    const TempDirectory data("gyo-os-user-data-font-data");
    std::filesystem::create_directories(data.Path() / "fonts");
    {
        const auto directories = FontDirectories();
        REQUIRE(directories);
        REQUIRE_FALSE(directories.value().empty());
        CHECK(directories.value().back() == Utf8(homeFonts));
    }
    {
        const ScopedEnvironment setData("XDG_DATA_HOME", Utf8(data.Path()).c_str());
        const auto fromData = FontDirectories();
        REQUIRE(fromData);
        REQUIRE_FALSE(fromData.value().empty());
        CHECK(fromData.value().back() == Utf8(data.Path() / "fonts"));
    }
    {
        const ScopedEnvironment setData("XDG_DATA_HOME", "relative-data");
        const auto relativeData = FontDirectories();
        REQUIRE(relativeData);
        REQUIRE_FALSE(relativeData.value().empty());
        CHECK(relativeData.value().back() == Utf8(homeFonts));
    }
#endif
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
TEST_CASE("OS files take the user data base from XDG_DATA_HOME when it is absolute") {
    const ScopedHome home("gyo-os-user-data-xdg-home");
    const TempDirectory data("gyo-os-user-data-xdg-data");
    const std::string dataText = Utf8(data.Path());
    {
        const ScopedEnvironment setData("XDG_DATA_HOME", dataText.c_str());
        const auto made = UserDataDirectory("GYO Test Org", "Test App");
        REQUIRE(made);
        CHECK(made.value() == dataText + "/GYO Test Org/Test App");
    }
    {
        const ScopedEnvironment slashData("XDG_DATA_HOME", (dataText + "/").c_str());
        const auto made = UserDataDirectory("GYO Test Org", "Test App");
        REQUIRE(made);
        CHECK(made.value() == dataText + "/GYO Test Org/Test App");
    }
    // A relative or empty XDG_DATA_HOME is ignored, as the specification says;
    // each call reads the environment again (nothing is cached).
    for (const std::string& ignored : std::vector<std::string>{"relative-data", ""}) {
        CAPTURE(ignored);
        const ScopedEnvironment setData("XDG_DATA_HOME", ignored.c_str());
        const auto made = UserDataDirectory("GYO Test Org", "Test App");
        REQUIRE(made);
        CHECK(made.value() == home.Text() + "/.local/share/GYO Test Org/Test App");
    }
    CHECK(home.CurrentDirectoryEmpty());
}
#endif
