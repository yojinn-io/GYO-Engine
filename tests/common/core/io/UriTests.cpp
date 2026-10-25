#include "doctest/doctest.h"

#include <memory>
#include <string>

#include "engine/io/fs/MountTable.hpp"
#include "engine/io/path/Uri.hpp"

using namespace Engine::IO;

// The file:// rule of docs/architecture/io.md: one mapping between a file URI
// and a native absolute path, as pure strings, so every case below gives the
// same result on every platform.

namespace {
std::string FilePath(std::string_view text) {
    const auto uri = Path::ParseUri(text);
    REQUIRE_MESSAGE(uri, "ParseUri failed for " << std::string(text));
    CHECK(uri.value().scheme == Path::UriScheme::File);
    return uri.value().path.Str();
}

IoErrorCode FileError(std::string_view text) {
    const auto uri = Path::ParseUri(text);
    REQUIRE_MESSAGE(!uri, "ParseUri accepted " << std::string(text));
    return uri.error().code;
}

IoErrorCode NativeError(std::string_view native) {
    const auto uri = Path::FileUriFromNativePath(native);
    REQUIRE_MESSAGE(!uri, "FileUriFromNativePath accepted " << std::string(native));
    return uri.error().code;
}

IoErrorCode NoNativePath(const Path::Uri& uri) {
    const auto native = Path::NativePathFromFileUri(uri);
    REQUIRE_MESSAGE(!native, "NativePathFromFileUri accepted " << uri.path.Str());
    return native.error().code;
}
} // namespace

TEST_CASE("Uri: a file URI of a POSIX absolute path keeps the root") {
    CHECK(FilePath("file:///Users/x") == "/Users/x");
    CHECK(FilePath("file:///") == "/");
}

TEST_CASE("Uri: a file URI drive letter is the first path segment") {
    CHECK(FilePath("file:///C:/game/assets") == "C:/game/assets");
    CHECK(FilePath("file:///c:/game") == "c:/game");
    CHECK(FilePath("file:///C:") == "C:/");
}

TEST_CASE("Uri: a localhost authority is the local machine") {
    CHECK(FilePath("file://localhost/Users/x") == "/Users/x");
    CHECK(FilePath("file://LOCALHOST/C:/x") == "C:/x");
}

TEST_CASE("Uri: a drive letter in the authority is rejected") {
    CHECK(FileError("file://C:/x") == IoErrorCode::InvalidPath);
    CHECK(FileError("file://c:") == IoErrorCode::InvalidPath);
}

TEST_CASE("Uri: UNC file URIs are not supported in this round") {
    CHECK(FileError("file://server/share/x") == IoErrorCode::NotSupported);
    // The four-slash spelling names the same UNC path, not a local one.
    CHECK(FileError("file:////server/share/x") == IoErrorCode::NotSupported);
    CHECK(FileError("file:///\\\\server\\share\\x") == IoErrorCode::NotSupported);
}

TEST_CASE("Uri: long-path and device prefixes are not file paths") {
    CHECK(FileError("file:////?/C:/x") == IoErrorCode::InvalidPath);
    CHECK(FileError("file:///\\\\?\\C:\\x") == IoErrorCode::InvalidPath);
    CHECK(FileError("file:///\\\\.\\C:") == IoErrorCode::InvalidPath);
    CHECK(FileError("file:///\\??\\C:\\x") == IoErrorCode::InvalidPath);
}

TEST_CASE("Uri: a file URI needs a path") {
    CHECK(FileError("file://") == IoErrorCode::InvalidPath);
    CHECK(FileError("file://localhost") == IoErrorCode::InvalidPath);
}

TEST_CASE("Uri: backslashes in a file URI path are separators") {
    CHECK(FilePath("file:///C:\\x\\y") == "C:/x/y");
    CHECK(FileError("file:///C:\\a\\..\\..\\x") == IoErrorCode::PathEscapesRoot);
}

TEST_CASE("Uri: a POSIX path whose first segment looks like a drive has no file URI") {
    // Its URI would name the drive path instead.
    CHECK(FileError("file:///./C:/x") == IoErrorCode::InvalidPath);
    CHECK(FileError("file:///a/../C:/x") == IoErrorCode::InvalidPath);
    CHECK(FilePath("file:///C:x") == "/C:x");
}

TEST_CASE("Uri: dot segments resolve inside the root and may not escape it") {
    CHECK(FilePath("file:///a/./b/../c") == "/a/c");
    CHECK(FilePath("file:///C:/a/../b") == "C:/b");
    CHECK(FileError("file:///a/../../x") == IoErrorCode::PathEscapesRoot);
    CHECK(FileError("file:///C:/../x") == IoErrorCode::PathEscapesRoot);
}

TEST_CASE("Uri: a file path keeps UTF-8 bytes and is not percent-decoded") {
    CHECK(FilePath("file:///Users/\xE8\xB3\x87\xE6\x96\x99/\xC3\xA9%20x") ==
          "/Users/\xE8\xB3\x87\xE6\x96\x99/\xC3\xA9%20x");
}

TEST_CASE("Uri: '#' and '?' belong to a file path") {
    const auto uri = Path::ParseUri("file:///tmp/a#b?c");
    REQUIRE(uri);
    CHECK(uri.value().path.Str() == "/tmp/a#b?c");
    CHECK(uri.value().query.empty());
    CHECK(uri.value().fragment.empty());
}

TEST_CASE("Uri: a file URI prints its canonical form, which parses to the same path") {
    for (const char* text : {"file:///Users/x", "file:///C:/game/assets", "file:///c:/x", "file:///"}) {
        const auto uri = Path::ParseUri(text);
        REQUIRE(uri);
        CHECK(uri.value().ToString() == text);
    }
    CHECK(Path::ParseUri("file://localhost/Users/x").value().ToString() == "file:///Users/x");
    CHECK(Path::ParseUri("file:///C:").value().ToString() == "file:///C:/");
    CHECK(Path::ParseUri("file:///a//b/").value().ToString() == "file:///a/b");
    // Parsing the canonical form is a fixed point, whatever spelling it came from.
    for (const char* text : {"file:///Users/x", "file://LocalHost/C:/x", "file:///C:\\x\\y", "file:///a/./b/../c",
                             "file:///a//b/", "file:///tmp/a#b?c", "file:///C:x", "file:///C:"}) {
        const auto first = Path::ParseUri(text);
        REQUIRE_MESSAGE(first, text);
        const auto second = Path::ParseUri(first.value().ToString());
        REQUIRE_MESSAGE(second, text);
        CHECK_MESSAGE(second.value().path == first.value().path, text);
        CHECK_MESSAGE(second.value().ToString() == first.value().ToString(), text);
    }
}

TEST_CASE("Uri: the loose parser applies the file rule and keeps other schemes as they were") {
    CHECK(Path::ParseUriLoose("file:///Users/x").path.Str() == "/Users/x");
    CHECK(Path::ParseUriLoose("file:///C:/x").path.Str() == "C:/x");
    // The scheme name is case-insensitive for file, as in ParseUri.
    CHECK(Path::ParseUriLoose("FILE:///Users/x").scheme == Path::UriScheme::File);
    CHECK(Path::ParseUriLoose("File:///C:/x").path.Str() == "C:/x");
    // Other schemes keep the loose meaning: leading slashes are dropped, the rest is relative.
    CHECK(Path::ParseUriLoose("res:///a/b").path.Str() == "a/b");
    CHECK(Path::ParseUriLoose("asset://textures/x.png").path.Str() == "textures/x.png");
    CHECK(Path::ParseUriLoose("textures/x.png").path.Str() == "textures/x.png");
}

TEST_CASE("Uri: a file URI that ParseUri rejects is no file URI for the loose parser either") {
    for (const char* text : {"file://server/share", "file://C:/x", "file:///C:/../x", "file://", "file:////server/x"}) {
        const auto uri = Path::ParseUriLoose(text);
        CHECK_MESSAGE(uri.scheme != Path::UriScheme::File, text);
        CHECK_MESSAGE(!Path::NativePathFromFileUri(uri), text);
    }
}

TEST_CASE("Uri: schemes other than file keep their parse") {
    const auto asset = Path::ParseUri("asset://textures/x.png?v=1#f");
    REQUIRE(asset);
    CHECK(asset.value().scheme == Path::UriScheme::Asset);
    CHECK(asset.value().path.Str() == "x.png");
    CHECK(asset.value().authority == "textures");
    CHECK(asset.value().query == "v=1");
    CHECK(asset.value().fragment == "f");
    CHECK(Path::ParseUri("textures/x.png").value().path.Str() == "textures/x.png");
    CHECK_FALSE(Path::ParseUri("/abs"));
    CHECK_FALSE(Path::ParseUri(""));
}

TEST_CASE("Uri: a mount on a file root resolves to an absolute native path") {
    FS::MountTable table;
    FS::MountPoint posix;
    posix.mountUri = Path::ParseUriLoose("asset://");
    posix.rootUri = Path::ParseUriLoose("file:///Users/x/assets");
    const auto request = Path::ParseUriLoose("asset://textures/a.png");
    const auto resolved = table.Resolve(posix, request);
    REQUIRE(resolved);
    CHECK(resolved.value().nativeUri.scheme == Path::UriScheme::File);
    CHECK(resolved.value().nativeUri.path.Str() == "/Users/x/assets/textures/a.png");

    FS::MountPoint drive = posix;
    drive.rootUri = Path::ParseUriLoose("file:///C:/game/assets");
    const auto onDrive = table.Resolve(drive, request);
    REQUIRE(onDrive);
    CHECK(onDrive.value().nativeUri.path.Str() == "C:/game/assets/textures/a.png");

    // A root that is not a file URI keeps its loose meaning.
    FS::MountPoint logical = posix;
    logical.rootUri = Path::ParseUriLoose("asset://root");
    const auto onLogical = table.Resolve(logical, request);
    REQUIRE(onLogical);
    CHECK(onLogical.value().nativeUri.path.Str() == "root/textures/a.png");
}

TEST_CASE("Uri: a mount never reaches outside its root") {
    FS::MountTable table;
    FS::MountPoint mount;
    mount.mountUri = Path::ParseUriLoose("asset://");
    mount.rootUri = Path::ParseUriLoose("file:///Users/x/assets");
    for (const char* request : {"asset://../secret", "asset://a/../../secret", "asset://..\\..\\x"}) {
        const auto resolved = table.Resolve(mount, Path::ParseUriLoose(request));
        REQUIRE_FALSE_MESSAGE(resolved, request);
        CHECK(resolved.error().code == IoErrorCode::PathEscapesRoot);
    }
    // A rejected file root never becomes a native path.
    mount.rootUri = Path::ParseUriLoose("file://server/share");
    const auto unc = table.Resolve(mount, Path::ParseUriLoose("asset://a.png"));
    REQUIRE(unc);
    CHECK_FALSE(Path::NativePathFromFileUri(unc.value().nativeUri));
}

TEST_CASE("Uri: a native absolute path becomes a file URI and comes back unchanged") {
    for (const char* native : {"/Users/x", "/", "C:/game/assets", "c:/x", "C:/",
                               "/Users/\xE8\xB3\x87\xE6\x96\x99/a#b?c"}) {
        const auto uri = Path::FileUriFromNativePath(native);
        REQUIRE_MESSAGE(uri, "FileUriFromNativePath failed for " << native);
        const auto back = Path::NativePathFromFileUri(uri.value());
        REQUIRE(back);
        CHECK(back.value() == native);
        const auto reparsed = Path::ParseUri(uri.value().ToString());
        REQUIRE(reparsed);
        CHECK(reparsed.value().path.Str() == native);
    }
    CHECK(Path::FileUriFromNativePath("/Users/x").value().ToString() == "file:///Users/x");
    CHECK(Path::FileUriFromNativePath("C:/x").value().ToString() == "file:///C:/x");
}

TEST_CASE("Uri: a native path is normalized to the '/' form") {
    CHECK(Path::FileUriFromNativePath("C:\\game\\assets").value().path.Str() == "C:/game/assets");
    CHECK(Path::FileUriFromNativePath("/a//b/./c/../d").value().path.Str() == "/a/b/d");
}

TEST_CASE("Uri: only native absolute paths become file URIs") {
    CHECK(NativeError("") == IoErrorCode::InvalidPath);
    CHECK(NativeError("a/b") == IoErrorCode::InvalidPath);
    // Drive-relative paths name the current directory of a drive, or the root of the current drive.
    CHECK(NativeError("C:x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("C:") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("/a/../../x") == IoErrorCode::PathEscapesRoot);
    // A POSIX path whose first segment looks like a drive: its URI would name the drive path.
    CHECK(NativeError("/C:/x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("/C:") == IoErrorCode::InvalidPath);
    // Long-path, device and NT prefixes are made inside engine/os only.
    CHECK(NativeError("\\\\?\\C:\\x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("//?/C:/x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\\\?\\UNC\\server\\share") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\\\.\\C:") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\??\\C:\\x") == IoErrorCode::InvalidPath);
    // UNC roots are not supported in this round (os-subsystem D46).
    CHECK(NativeError("//server/share/x") == IoErrorCode::NotSupported);
    CHECK(NativeError("\\\\server\\share\\x") == IoErrorCode::NotSupported);
}

TEST_CASE("Uri: only an absolute file URI has a native path") {
    CHECK(NoNativePath(Path::ParseUriLoose("asset://x.png")) == IoErrorCode::InvalidPath);
    Path::Uri relative;
    relative.scheme = Path::UriScheme::File;
    relative.path = Path::Path::FromNormalized("Users/x");
    CHECK(NoNativePath(relative) == IoErrorCode::InvalidPath);
}

TEST_CASE("Uri: a file Uri built by hand names a path only in the mapping's normalized form") {
    const auto make = [](std::string path) {
        Path::Uri uri;
        uri.scheme = Path::UriScheme::File;
        uri.path = Path::Path::FromNormalized(std::move(path));
        return uri;
    };
    const auto plain = Path::NativePathFromFileUri(make("/a/b"));
    REQUIRE(plain);
    CHECK(plain.value() == "/a/b");
    CHECK(NoNativePath(make("/a/../b")) == IoErrorCode::InvalidPath);
    CHECK(NoNativePath(make("C:/x\\..\\y")) == IoErrorCode::InvalidPath);
    CHECK(NoNativePath(make("/C:/x")) == IoErrorCode::InvalidPath);
    CHECK(NoNativePath(make(std::string("/a\0b", 4))) == IoErrorCode::InvalidPath);
    auto unc = make("/share/x");
    unc.authority = "server";
    CHECK(NoNativePath(unc) == IoErrorCode::NotSupported);
    CHECK(unc.ToString() == "file://server/share/x");
    auto local = make("/x");
    local.authority = "localhost";
    const auto localPath = Path::NativePathFromFileUri(local);
    REQUIRE(localPath);
    CHECK(localPath.value() == "/x");
    auto withFragment = make("/x");
    withFragment.fragment = "f";
    CHECK(NoNativePath(withFragment) == IoErrorCode::InvalidPath);
}
