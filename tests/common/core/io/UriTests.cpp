#include "doctest/doctest.h"

#include <memory>
#include <string>

#include "engine/io/fs/MountTable.hpp"
#include "engine/io/path/Uri.hpp"

using namespace Engine::IO;

// The file:// rule of docs/architecture/io.md: one mapping between a file URI
// and a native absolute path, as pure strings, so every case below gives the
// same result on every platform. The path is kept as written (io-native-paths,
// D54): dot segments, repeated and trailing slashes reach the OS, which applies
// its own rules (symbolic links on POSIX); a "//host/..." path is a UNC path on
// Windows and the root-relative path the OS makes of it on POSIX.

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

// The successful forms, each REQUIREd first, so that a regression fails its case instead of aborting the run.
std::string Printed(std::string_view text) {
    const auto uri = Path::ParseUri(text);
    REQUIRE_MESSAGE(uri, "ParseUri failed for " << std::string(text));
    return uri.value().ToString();
}

Path::Uri NativeUri(std::string_view native) {
    const auto uri = Path::FileUriFromNativePath(native);
    REQUIRE_MESSAGE(uri, "FileUriFromNativePath failed for " << std::string(native));
    return uri.value();
}

std::string HandNative(const Path::Uri& uri) {
    const auto native = Path::NativePathFromFileUri(uri);
    REQUIRE_MESSAGE(native, "NativePathFromFileUri failed for " << uri.path.Str());
    return native.value();
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

TEST_CASE("Uri: a UNC file URI names the native path //host/share") {
    CHECK(FilePath("file://server/share/x") == "//server/share/x");
    // The four-slash and backslash spellings name the same UNC path.
    CHECK(FilePath("file:////server/share/x") == "//server/share/x");
    CHECK(FilePath("file:///\\\\server\\share\\x") == "//server/share/x");
    // A host alone, or a host and share: the OS decides what it names.
    CHECK(FilePath("file://server") == "//server");
    CHECK(FilePath("file://server/share") == "//server/share");
    // A UNC host named localhost is spelled with four slashes: file://localhost/ is the local machine.
    CHECK(FilePath("file:////localhost/C$/x") == "//localhost/C$/x");
    CHECK(FilePath("file:////LocalHost/C$") == "//LocalHost/C$");
    // No Windows server has a '?' or ':' in its name: such a host would read as a namespace prefix, a drive or
    // a port, so it is refused in every spelling.
    for (const char* text : {"file:////C:/x", "file://///C:/x", "file:////C:", "file:////C:foo/x", "file://\?\?/C:/x",
                             "file:////\?\?/C:/x", "file://///\?\?/C:/x", "file://server:8080/x", "file:////server:8080/x"}) {
        CAPTURE(text);
        CHECK(FileError(text) == IoErrorCode::InvalidPath);
    }
    CHECK(FileError("file://server\\share\\x") == IoErrorCode::InvalidPath);
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
    CHECK(FilePath("file:///C:\\a\\..\\..\\x") == "C:/a/../../x");
}

TEST_CASE("Uri: a POSIX path whose first segment looks like a drive has no file URI") {
    // Its URI would name the drive path instead. The check is lexical, so it also sees through dot segments
    // and extra leading slashes; it only refuses, the path stays as written.
    CHECK(FileError("file:///./C:/x") == IoErrorCode::InvalidPath);
    CHECK(FileError("file:///a/../C:/x") == IoErrorCode::InvalidPath);
    CHECK(FilePath("file:///C:x") == "/C:x");
}

TEST_CASE("Uri: dot segments, repeated and trailing slashes are kept for the OS") {
    // The OS resolves them: on POSIX "link/.." is the parent of the link's target.
    CHECK(FilePath("file:///a/./b/../c") == "/a/./b/../c");
    CHECK(FilePath("file:///C:/a/../b") == "C:/a/../b");
    CHECK(FilePath("file:///a/../../x") == "/a/../../x");
    CHECK(FilePath("file:///C:/../x") == "C:/../x");
    CHECK(FilePath("file:///a//b/") == "/a//b/");
    // After an empty authority, two or more slashes start a UNC path (file:////host and
    // file://///host, RFC 8089 appendix E); a local path has one.
    CHECK(FilePath("file://///a") == "//a");
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
    CHECK(Printed("file://localhost/Users/x") == "file:///Users/x");
    CHECK(Printed("file:///C:") == "file:///C:/");
    CHECK(Printed("file:///a//b/") == "file:///a//b/");
    CHECK(Printed("file:///a/./b/../c") == "file:///a/./b/../c");
    CHECK(Printed("file:////server/share/x") == "file://server/share/x");
    CHECK(Printed("file://server") == "file://server");
    CHECK(Printed("file:////localhost/C$/x") == "file:////localhost/C$/x");
    // Parsing the canonical form is a fixed point, whatever spelling it came from.
    for (const char* text : {"file:///Users/x", "file://LocalHost/C:/x", "file:///C:\\x\\y", "file:///a/./b/../c",
                             "file:///a//b/", "file:///tmp/a#b?c", "file:///C:x", "file:///C:", "file://server/share/x",
                             "file:////server/share", "file:///\\\\server\\share", "file://server",
                             "file:////localhost/C$/x", "file://///a", "file:///a/../../x"}) {
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
    for (const char* text : {"file://C:/x", "file://", "file://localhost", "file:////?/C:/x"}) {
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

// Every spelling built from these pieces is either refused or keeps the rules: a native path that gets a URI
// comes back from it and its text parses to the same path; a URI that parses has a native path, and its
// canonical form parses to the same path and prints the same text (io.md Invariant 3).
TEST_CASE("Uri: generated spellings keep the round trip and the fixed point") {
    const char* const leads[] = {"", "/", "//", "///", "////", "/////", "\\", "\\\\", "\\\\\\", "/\\", "\\/"};
    const char* const firsts[] = {"", "C:", "c:", "C:x", "server", "localhost", "LocalHost", "??", "?", ".", "..",
                                  "a", "s:1", "C$"};
    const char* const rests[] = {"", "/", "/x", "/./x", "/../x", "/../C:/x", "/./C:/x", "/C:/x", "//x", "/x/"};
    int natives = 0;
    int uris = 0;
    for (const char* lead : leads) {
        for (const char* first : firsts) {
            for (const char* rest : rests) {
                const std::string text = std::string(lead) + first + rest;
                CAPTURE(text);
                if (const auto uri = Path::FileUriFromNativePath(text)) {
                    ++natives;
                    const auto back = Path::NativePathFromFileUri(uri.value());
                    REQUIRE_MESSAGE(back, "no native path back for " << text);
                    CHECK(back.value() == uri.value().path.Str());
                    const auto reparsed = Path::ParseUri(uri.value().ToString());
                    REQUIRE_MESSAGE(reparsed, "the URI text " << uri.value().ToString() << " does not parse");
                    CHECK(reparsed.value().path == uri.value().path);
                }
                const std::string spelled = "file:" + text;
                const auto uri = Path::ParseUri(spelled);
                if (uri && uri.value().scheme == Path::UriScheme::File) {
                    ++uris;
                    const auto second = Path::ParseUri(uri.value().ToString());
                    REQUIRE_MESSAGE(second, "the canonical form " << uri.value().ToString() << " does not parse");
                    CHECK(second.value().path == uri.value().path);
                    CHECK(second.value().ToString() == uri.value().ToString());
                    const auto native = Path::NativePathFromFileUri(uri.value());
                    REQUIRE_MESSAGE(native, "no native path for " << spelled);
                    CHECK(native.value() == uri.value().path.Str());
                }
            }
        }
    }
    MESSAGE(natives << " native spellings and " << uris << " URI spellings mapped");
    CHECK(natives > 100);
    CHECK(uris > 100);
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
    // A UNC root works as any other file root, and still keeps its requests inside.
    mount.rootUri = Path::ParseUriLoose("file://server/share/assets");
    const auto unc = table.Resolve(mount, Path::ParseUriLoose("asset://a.png"));
    REQUIRE(unc);
    const auto native = Path::NativePathFromFileUri(unc.value().nativeUri);
    REQUIRE(native);
    CHECK(native.value() == "//server/share/assets/a.png");
    CHECK(table.Resolve(mount, Path::ParseUriLoose("asset://../secret")).error().code == IoErrorCode::PathEscapesRoot);
}

TEST_CASE("Uri: a native absolute path becomes a file URI and comes back unchanged") {
    for (const char* native : {"/Users/x", "/", "C:/game/assets", "c:/x", "C:/",
                               "/Users/\xE8\xB3\x87\xE6\x96\x99/a#b?c", "/a/link/../x", "/a//b/", "C:/a/../../x",
                               "//server/share/x", "//server", "//localhost/C$/x", "//a/b"}) {
        const auto uri = Path::FileUriFromNativePath(native);
        REQUIRE_MESSAGE(uri, "FileUriFromNativePath failed for " << native);
        const auto back = Path::NativePathFromFileUri(uri.value());
        REQUIRE(back);
        CHECK(back.value() == native);
        const auto reparsed = Path::ParseUri(uri.value().ToString());
        REQUIRE(reparsed);
        CHECK(reparsed.value().path.Str() == native);
    }
    CHECK(NativeUri("/Users/x").ToString() == "file:///Users/x");
    CHECK(NativeUri("C:/x").ToString() == "file:///C:/x");
}

TEST_CASE("Uri: a native path keeps its segments and takes the '/' form") {
    CHECK(NativeUri("C:\\game\\assets").path.Str() == "C:/game/assets");
    CHECK(NativeUri("/a//b/./c/../d").path.Str() == "/a//b/./c/../d");
    CHECK(NativeUri("\\\\server\\share\\x").path.Str() == "//server/share/x");
    CHECK(NativeUri("\\\\server\\share\\x").ToString() == "file://server/share/x");
    CHECK(NativeUri("\\\\localhost\\C$").ToString() == "file:////localhost/C$");
    // Three or more leading slashes are the root, as the OS reads them.
    CHECK(NativeUri("///a").path.Str() == "/a");
}

TEST_CASE("Uri: only native absolute paths become file URIs") {
    CHECK(NativeError("") == IoErrorCode::InvalidPath);
    CHECK(NativeError("a/b") == IoErrorCode::InvalidPath);
    // Drive-relative paths name the current directory of a drive, or the root of the current drive.
    CHECK(NativeError("C:x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("C:") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\x") == IoErrorCode::InvalidPath);
    CHECK(NativeError(std::string("/a\0b", 4)) == IoErrorCode::InvalidPath);
    // A POSIX path whose first segment looks like a drive: its URI would name the drive path.
    CHECK(NativeError("/C:/x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("/C:") == IoErrorCode::InvalidPath);
    // Long-path, device and NT prefixes are made inside engine/os only.
    CHECK(NativeError("///C:/x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\\\\\C:\\x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("///C:") == IoErrorCode::InvalidPath);
    CHECK(NativeError("/./C:/x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("/a/../C:/x") == IoErrorCode::InvalidPath);
    // A UNC host with a '?' or ':' (a drive, the NT prefix written with two slashes, a port).
    CHECK(NativeError("//C:/x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\\\C:\\x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("//\?\?/C:/x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\\\??\\C:\\x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("//server:8080/x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\\\?\\C:\\x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("//?/C:/x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\\\?\\UNC\\server\\share") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\\\.\\C:") == IoErrorCode::InvalidPath);
    CHECK(NativeError("\\??\\C:\\x") == IoErrorCode::InvalidPath);
    CHECK(NativeError("//./NUL") == IoErrorCode::InvalidPath);
    // A UNC path with no host is no path.
    CHECK(NativeError("//") == IoErrorCode::InvalidPath);
}

TEST_CASE("Uri: only an absolute file URI has a native path") {
    CHECK(NoNativePath(Path::ParseUriLoose("asset://x.png")) == IoErrorCode::InvalidPath);
    Path::Uri relative;
    relative.scheme = Path::UriScheme::File;
    relative.path = Path::Path::FromNormalized("Users/x");
    CHECK(NoNativePath(relative) == IoErrorCode::InvalidPath);
}

TEST_CASE("Uri: a file Uri built by hand names a path only in the mapping's canonical form") {
    const auto make = [](std::string path) {
        Path::Uri uri;
        uri.scheme = Path::UriScheme::File;
        uri.path = Path::Path::FromNormalized(std::move(path));
        return uri;
    };
    const auto plain = Path::NativePathFromFileUri(make("/a/b"));
    REQUIRE(plain);
    CHECK(plain.value() == "/a/b");
    // Dot segments are kept, so a path with them is canonical; a backslash is not.
    CHECK(HandNative(make("/a/../b")) == "/a/../b");
    CHECK(NoNativePath(make("C:/x\\..\\y")) == IoErrorCode::InvalidPath);
    CHECK(NoNativePath(make("/C:/x")) == IoErrorCode::InvalidPath);
    CHECK(NoNativePath(make(std::string("/a\0b", 4))) == IoErrorCode::InvalidPath);
    // Not the mapping's form either: three slashes are the root ("/a"), a bare drive is "C:/".
    CHECK(NoNativePath(make("///a")) == IoErrorCode::InvalidPath);
    CHECK(NoNativePath(make("C:")) == IoErrorCode::InvalidPath);
    // A host in the authority names the UNC path //host + path.
    auto unc = make("/share/x");
    unc.authority = "server";
    CHECK(HandNative(unc) == "//server/share/x");
    CHECK(unc.ToString() == "file://server/share/x");
    CHECK(HandNative(make("//server/share/x")) == "//server/share/x");
    auto prefixed = make("/C:/x");
    prefixed.authority = "?";
    CHECK(NoNativePath(prefixed) == IoErrorCode::InvalidPath);
    // A host with a relative path, a drive for a host, a drive-shaped UNC host: no UNC path.
    auto relativeHost = make("share");
    relativeHost.authority = "server";
    CHECK(NoNativePath(relativeHost) == IoErrorCode::InvalidPath);
    CHECK(relativeHost.ToString() == "file://server/share");
    auto driveHost = make("/x");
    driveHost.authority = "C:";
    CHECK(NoNativePath(driveHost) == IoErrorCode::InvalidPath);
    CHECK(NoNativePath(make("//C:/x")) == IoErrorCode::InvalidPath);
    auto local = make("/x");
    local.authority = "localhost";
    const auto localPath = Path::NativePathFromFileUri(local);
    REQUIRE(localPath);
    CHECK(localPath.value() == "/x");
    auto withFragment = make("/x");
    withFragment.fragment = "f";
    CHECK(NoNativePath(withFragment) == IoErrorCode::InvalidPath);
}
