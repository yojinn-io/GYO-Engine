#include "doctest/doctest.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "engine/base/Fnv1a.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/io/helpers/UserFiles.hpp"
#include "engine/io/helpers/WriteFileAtomically.hpp"
#include "engine/io/path/Uri.hpp"
#include "engine/os/Files.hpp"

using namespace Engine::IO;

// engine/io's helpers for files a person names (docs/architecture/io.md, "User files"). Files are set up with
// the standard library; the helpers are reached through file URIs and UTF-8 native paths.

namespace {

class UserTree final {
public:
    UserTree() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() / ("gyo-user-files-" + std::to_string(stamp));
        std::filesystem::create_directories(root_);
    }
    UserTree(const UserTree&) = delete;
    UserTree& operator=(const UserTree&) = delete;
    ~UserTree() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    void Write(const std::string& relative, const std::string& bytes) const {
        const auto file = Path(relative);
        std::filesystem::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    std::filesystem::path Path(const std::string& relative = {}) const {
        return relative.empty() ? root_ : root_ / std::filesystem::path(std::u8string(relative.begin(), relative.end()));
    }
    std::string Native(const std::string& relative = {}) const {
        const std::u8string text = Path(relative).generic_u8string();
        return std::string(text.begin(), text.end());
    }
    Path::Uri Uri(const std::string& relative = {}) const {
        auto uri = FS::NativeFileUri(Native(relative));
        REQUIRE(uri);
        return uri.value();
    }

private:
    std::filesystem::path root_;
};

Helpers::ReadAllOptions AnySize() {
    Helpers::ReadAllOptions options;
    options.maxBytes = (std::numeric_limits<std::size_t>::max)();
    return options;
}

bool Within(const std::string& candidate, const std::string& root) {
    auto within = FS::IsNativePathWithin(candidate, root);
    REQUIRE(within);
    return within.value();
}

} // namespace

TEST_CASE("ProbeFileStamp reports a missing file, a file and what is not a file") {
    const UserTree tree;
    FS::NativeFileSystem files;
    auto missing = Helpers::ProbeFileStamp(files, tree.Uri("none.json"), AnySize());
    REQUIRE(missing);
    CHECK_FALSE(missing.value().exists);

    tree.Write("名前/doc.json", "{\"a\":1}");
    auto stamp = Helpers::ProbeFileStamp(files, tree.Uri("名前/doc.json"), AnySize());
    REQUIRE(stamp);
    CHECK(stamp.value().exists);
    CHECK(stamp.value().sizeBytes == 7);
    CHECK(stamp.value().contentHash == Engine::Base::Fnv1a64("{\"a\":1}"));
    CHECK(stamp.value().mtimeNs != 0);

    tree.Write("名前/doc.json", "{\"a\":2}");
    auto changed = Helpers::ProbeFileStamp(files, tree.Uri("名前/doc.json"), AnySize());
    REQUIRE(changed);
    CHECK(changed.value().contentHash != stamp.value().contentHash);

    auto directory = Helpers::ProbeFileStamp(files, tree.Uri("名前"), AnySize());
    REQUIRE_FALSE(directory);
    CHECK(directory.error().code == IoErrorCode::InvalidPath);
    // Refused from its status, before anything is opened.
    CHECK(std::string_view(directory.error().message).find("not a regular file") != std::string_view::npos);
}

TEST_CASE("ProbeFileStamp reads no more than the caller allows") {
    const UserTree tree;
    tree.Write("big.bin", std::string(64, 'x'));
    FS::NativeFileSystem files;
    Helpers::ReadAllOptions small;
    small.maxBytes = 16;
    CHECK_FALSE(Helpers::ProbeFileStamp(files, tree.Uri("big.bin"), small));
    CHECK(Helpers::ProbeFileStamp(files, tree.Uri("big.bin"), AnySize()));
}

TEST_CASE("WriteFileAtomically creates missing parents only when asked") {
    const UserTree tree;
    FS::NativeFileSystem files;
    const std::string text = "saved";
    const Engine::Base::ConstSpan<std::byte> bytes(reinterpret_cast<const std::byte*>(text.data()), text.size());
    CHECK_FALSE(Helpers::WriteFileAtomically(files, tree.Uri("a/b/out.txt"), bytes));
    CHECK_FALSE(std::filesystem::exists(tree.Path("a")));

    Helpers::WriteAtomicallyOptions options;
    options.createParentDirectories = true;
    REQUIRE(Helpers::WriteFileAtomically(files, tree.Uri("a/b/out.txt"), bytes, options));
    auto read = Helpers::ReadAllBytes(files, tree.Uri("a/b/out.txt"));
    REQUIRE(read);
    CHECK(std::string(reinterpret_cast<const char*>(read.value().data()), read.value().size()) == "saved");
    // An existing parent is fine, as is writing over the file.
    REQUIRE(Helpers::WriteFileAtomically(files, tree.Uri("a/b/out.txt"), bytes, options));
}

TEST_CASE("WriteFileAtomically with createParentDirectories writes a file at a volume's root") {
    // A file right under "/" or "C:/" has a parent with no name; that root exists and is left alone. The write
    // may still be refused by the OS (no permission), but never as an invalid path.
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path file = std::filesystem::temp_directory_path().root_path() /
                                       ("gyo-io-root-" + std::to_string(stamp) + ".txt");
    const std::u8string text = file.generic_u8string();
    auto uri = FS::NativeFileUri(std::string(text.begin(), text.end()));
    REQUIRE(uri);
    FS::NativeFileSystem files;
    const std::string data = "root";
    Helpers::WriteAtomicallyOptions options;
    options.createParentDirectories = true;
    auto written = Helpers::WriteFileAtomically(
        files, uri.value(), Engine::Base::ConstSpan<std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()),
        options);
    if (written) {
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
    } else {
        CHECK(written.error().code != IoErrorCode::InvalidPath);
    }
}

TEST_CASE("IsNativePathWithin compares whole components of absolute paths") {
    const UserTree tree;
    tree.Write("root/inside/doc.json", "");
    tree.Write("rootx/doc.json", "");
    CHECK(Within(tree.Native("root"), tree.Native("root")));
    CHECK(Within(tree.Native("root/inside/doc.json"), tree.Native("root")));
    CHECK(Within(tree.Native("root/not-yet/new.json"), tree.Native("root")));
    CHECK_FALSE(Within(tree.Native("rootx/doc.json"), tree.Native("root")));
    CHECK_FALSE(Within(tree.Native("root/../rootx/doc.json"), tree.Native("root")));
    CHECK(Within(tree.Native("root/./inside/../inside/doc.json"), tree.Native("root")));
    CHECK_FALSE(Within(tree.Native("root"), tree.Native("root/inside")));
    CHECK_FALSE(Within("", tree.Native("root")));
    CHECK_FALSE(Within(tree.Native("root"), ""));
}

namespace {

// Where the OS puts a file written at path: its parent resolved by the OS (std::filesystem::weakly_canonical:
// the existing part through the OS, links before '..'), then the last name as written (an atomic save renames
// over that name, link or not). Independent of engine/io.
std::filesystem::path WhereItLands(const std::filesystem::path& path) {
    return std::filesystem::weakly_canonical(path.parent_path()) / path.filename();
}

bool Under(const std::filesystem::path& path, const std::filesystem::path& root) {
    auto part = path.begin();
    for (auto rootPart = root.begin(); rootPart != root.end(); ++rootPart, ++part) {
        if (part == path.end() || *part != *rootPart) return false;
    }
    return true;
}

} // namespace

TEST_CASE("IsNativePathWithin agrees with where the OS puts a file, through links") {
    const UserTree tree;
    tree.Write("real/root/doc.json", "");
    tree.Write("real/root/inside/doc.json", "");
    tree.Write("real/root/sub/keep.txt", "");
    tree.Write("elsewhere/target.json", "");
    tree.Write("outside/dir/keep.txt", "");
    std::error_code error;
    const auto link = [&](const std::string& target, const std::string& name, bool directory) {
        if (error) return;
        if (directory) std::filesystem::create_directory_symlink(tree.Path(target), tree.Path(name), error);
        else std::filesystem::create_symlink(tree.Path(target), tree.Path(name), error);
    };
    link("real", "alias", true);
    link("real/root", "rootlink", true);
    link("elsewhere/target.json", "real/root/link.json", false);
    link("elsewhere", "real/root/dirlink", true);
    link("real/root/sub", "outside/dir/up", true);
    link("outside", "real/root/esc", true);
    if (error) {
        // Windows needs Developer Mode or a privilege to create links (as in tests/common/os/FilesTests.cpp).
        MESSAGE("symbolic links unavailable here: " << error.message());
        return;
    }
    const std::pair<const char*, const char*> cases[] = {
        {"alias/root/doc.json", "real/root"},          // a link above the candidate
        {"real/root/doc.json", "alias/root"},          // a link above the root
        {"real/root/link.json", "real/root"},          // a link named inside the root
        {"rootlink/new.json", "rootlink"},             // the root is itself a link
        {"real/root/new.json", "rootlink"},
        {"rootlink/new.json", "real/root"},
        {"real/root/dirlink/../inside/doc.json", "real/root"},  // '..' after a link
        {"outside/dir/up/../x.json", "real/root"},
        {"real/root/esc/../secret.json", "real/root"},
    };
    for (const auto& [candidate, root] : cases) {
        const bool expected = Under(WhereItLands(tree.Path(candidate)), std::filesystem::canonical(tree.Path(root)));
        CHECK_MESSAGE(Within(tree.Native(candidate), tree.Native(root)) == expected,
                      std::string(candidate) << " within " << std::string(root) << ": the OS says " << expected);
    }
}

TEST_CASE("IsNativePathWithin follows the volume's own rule for names that differ in case") {
    // Where the volume ignores case, "root" is the directory "Root" and the file lands inside it; where it
    // does not, "root" is another, missing directory.
    const UserTree tree;
    tree.Write("Root/doc.json", "");
    const bool expected = Under(WhereItLands(tree.Path("Root/doc.json")), std::filesystem::weakly_canonical(tree.Path("root")));
    CHECK(Within(tree.Native("Root/doc.json"), tree.Native("root")) == expected);
}

TEST_CASE("IsNativePathWithin keeps a root written as the disk does not spell it") {
    // On a volume that ignores case the root resolves to the disk's spelling, and so do the candidate's parents.
    const UserTree tree;
    tree.Write("Assets/ui/doc.json", "");
    CHECK(Within(tree.Native("ASSETS/ui/new.json"), tree.Native("ASSETS")));
    CHECK(Within(tree.Native("ASSETS/new.json"), tree.Native("ASSETS")));
}

TEST_CASE("IsNativePathWithin resolves a relative path against the current directory") {
    const std::u8string current = std::filesystem::current_path().generic_u8string();
    const std::string here(current.begin(), current.end());
    CHECK(Within("some/relative/file.json", here));
    CHECK(Within("x.json", here));
    CHECK(Within("x.json", "."));
    CHECK(Within("ui/x.json", "./"));
    CHECK_FALSE(Within("../x.json", "."));
}

TEST_CASE("IsNativePathWithin reads a root that ends in a separator as the directory it names") {
    const UserTree tree;
    tree.Write("root/doc.json", "");
    CHECK(Within(tree.Native("root/doc.json"), tree.Native("root") + "/"));
    CHECK(Within(tree.Native("root") + "/", tree.Native("root")));
    CHECK_FALSE(Within(tree.Native("rootx/doc.json"), tree.Native("root") + "/"));
}

TEST_CASE("IsNativePathWithin reads a trailing separator the same way when the OS cannot resolve the root") {
    // A directory nobody may read makes Os::Files::ResolvedPath fail (not a missing component), so both paths
    // are compared as written. Where permissions do not stop this process (Windows, a superuser) the paths
    // resolve and the answer is the same.
    const UserTree tree;
    tree.Write("locked/root/doc.json", "");
    std::error_code error;
    std::filesystem::permissions(tree.Path("locked"), std::filesystem::perms::none, error);
    const bool inside = Within(tree.Native("locked/root/doc.json"), tree.Native("locked/root") + "/");
    std::filesystem::permissions(tree.Path("locked"), std::filesystem::perms::owner_all, error);
    CHECK(inside);
}

TEST_CASE("IsNativePathWithin refuses a namespace prefix as the file mapping does") {
    for (const char* prefixed : {"//?/C:/x/doc.json", "//./C:/x/doc.json", "\\\\?\\C:\\x\\doc.json"}) {
        auto within = FS::IsNativePathWithin(prefixed, "//?/C:/x");
        REQUIRE_FALSE(within);
        CHECK(within.error().code == IoErrorCode::InvalidPath);
    }
}

TEST_CASE("IsNativePathWithin refuses a path that is not UTF-8") {
    const std::string invalid = "bad\xff/name";
    for (const auto& [candidate, root] : {std::pair<std::string, std::string>{invalid, "/"}, {"/", invalid}}) {
        auto within = FS::IsNativePathWithin(candidate, root);
        REQUIRE_FALSE(within);
        CHECK(within.error().code == IoErrorCode::InvalidPath);
    }
}
