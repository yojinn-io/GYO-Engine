#include "engine/io/path/Uri.hpp"

#include <algorithm>
#include <cctype>

namespace Engine::IO::Path {

    static std::string ToLower(std::string_view sv) {
        std::string out(sv);
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    static UriScheme SchemeFrom(std::string_view schemeLower) {
        if (schemeLower == "asset") return UriScheme::Asset;
        if (schemeLower == "file")  return UriScheme::File;
        if (schemeLower == "http")  return UriScheme::Http;
        if (schemeLower == "https") return UriScheme::Https;
        return UriScheme::Unknown;
    }

    // ASCII only, so the mapping does not depend on the C locale.
    static bool IsAsciiAlpha(const char c) noexcept {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    static bool IsDriveLetter(std::string_view s) noexcept {
        return s.size() >= 2 && IsAsciiAlpha(s[0]) && s[1] == ':';
    }

    // "X:" followed by '/' or the end: the root of a drive.
    static bool StartsWithDrive(std::string_view s) noexcept {
        return IsDriveLetter(s) && (s.size() == 2 || s[2] == '/');
    }

    static bool IsLocalAuthority(std::string_view authority) {
        return authority.empty() || ToLower(authority) == "localhost";
    }

    static std::string ToForwardSlashes(std::string_view s) {
        std::string out(s);
        std::replace(out.begin(), out.end(), '\\', '/');
        return out;
    }

    // The Windows long-path, device and NT namespace prefixes ("\\?\",
    // "\\.\", "\??\") in '/' form. They are made inside engine/os only.
    static bool HasNamespacePrefix(std::string_view s) noexcept {
        // "/\?\?/" is "/??/" written so that it is not read as a trigraph.
        return s.starts_with("//?/") || s.starts_with("//./") || s.starts_with("/\?\?/") || s == "//?" ||
               s == "//.";
    }

    // A native absolute path in '/' form, as the mapping keeps it: segments as written (dot segments,
    // repeated and trailing slashes reach the OS, which applies its own rules; io-native-paths, D54).
    // "/..." is a local path, "X:/..." a drive path ("X:" is its root "X:/"), "//host..." a UNC path;
    // three or more leading slashes are the root "/", as POSIX reads them. A NUL, a namespace prefix,
    // a UNC path with no host and a relative path are refused.
    static Base::Result<std::string, IoError> CanonicalFilePath(std::string path) {
        if (path.find('\0') != std::string::npos) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "file path contains a NUL byte"));
        }
        if (HasNamespacePrefix(path)) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath,
                                           "long-path and device prefixes never appear in engine/io paths", path));
        }
        const std::size_t slashes = path.find_first_not_of('/') == std::string::npos ? path.size()
                                                                                     : path.find_first_not_of('/');
        if (slashes >= 3) return "/" + path.substr(slashes);
        if (slashes == 2) {
            if (path.size() == 2) {
                return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "UNC path has no host", path));
            }
            return path;
        }
        if (slashes == 1) return path;
        if (StartsWithDrive(path)) {
            if (path.size() == 2) path.push_back('/');
            return path;
        }
        return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "file path is not absolute", path));
    }

    // "/X:..." with a drive as its first segment: as a native POSIX path it has no file URI, since its URI
    // would name the drive path.
    static bool LooksLikeDriveUnderRoot(std::string_view path) noexcept {
        return path.size() >= 3 && path[0] == '/' && path[1] != '/' && StartsWithDrive(path.substr(1));
    }

    // The native path of a file URI from what follows "file://". An empty or localhost authority is the
    // local machine: one leading slash starts a local path, two or more a UNC path (file:////host,
    // file://///host; RFC 8089 appendix E). Any other authority is a UNC host. Backslashes in the path are
    // separators; the query and fragment separators are part of a file path.
    static Base::Result<std::string, IoError> FilePathFromRest(std::string_view rest) {
        const auto slash = rest.find('/');
        const std::string_view authority = rest.substr(0, slash);
        const std::string path = slash == std::string_view::npos ? std::string{} : ToForwardSlashes(rest.substr(slash));
        if (IsDriveLetter(authority)) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath,
                                           "file URI has a drive letter in its authority; write file:///C:/...",
                                           std::string(rest)));
        }
        if (authority.find('\\') != std::string_view::npos) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "file URI host has a backslash",
                                           std::string(rest)));
        }
        if (!IsLocalAuthority(authority)) return CanonicalFilePath("//" + std::string(authority) + path);
        if (path.empty()) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "file URI has no path", std::string(rest)));
        }
        if (HasNamespacePrefix(path) || HasNamespacePrefix(std::string_view(path).substr(1))) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath,
                                           "long-path and device prefixes never appear in engine/io paths",
                                           std::string(rest)));
        }
        if (path.starts_with("//")) return CanonicalFilePath("//" + path.substr(path.find_first_not_of('/') ==
                                                                                       std::string::npos
                                                                                   ? path.size()
                                                                                   : path.find_first_not_of('/')));
        // "/C:/x" names the drive path "C:/x".
        std::string_view native = path;
        if (StartsWithDrive(native.substr(1))) native.remove_prefix(1);
        return CanonicalFilePath(std::string(native));
    }

    std::string Uri::ToString() const {
        std::string out;

        auto appendPath = [&]() {
            out += path.Str();
        };

        if (scheme == UriScheme::File) {
            // The canonical form; parsing it gives the same Uri. A file URI has no query or fragment.
            // A local path is "file:///<path>"; a UNC path "//host/..." is "file://host/...", or
            // "file:////localhost/..." for a host named localhost, since file://localhost/ is the local
            // machine. A host set by hand in the authority is printed there.
            const std::string& text = path.Str();
            if (!IsLocalAuthority(authority)) {
                out = "file://" + authority;
                if (!text.empty() && text.front() != '/') out += "/";
                out += text;
                return out;
            }
            if (text.starts_with("//")) {
                const std::string_view host = std::string_view(text).substr(2, text.find('/', 2) - 2);
                return (IsLocalAuthority(host) ? "file://" : "file:") + text;
            }
            out = "file://";
            if (text.empty() || text.front() != '/') out += "/";
            appendPath();
            return out;
        }

        if (scheme == UriScheme::None) {
            appendPath();
        } else {
            // scheme text
            if (scheme == UriScheme::Unknown) out += schemeText;
            else {
                switch (scheme) {
                case UriScheme::Asset: out += "asset"; break;
                case UriScheme::File:  out += "file";  break;
                case UriScheme::Http:  out += "http";  break;
                case UriScheme::Https: out += "https"; break;
                default: out += "unknown"; break;
                }
            }
            out += "://";
            out += authority;
            if (!authority.empty() && !path.Empty() && path.Str().front() != '/') out += "/";
            appendPath();
        }

        if (!query.empty()) {
            out += "?";
            out += query;
        }
        if (!fragment.empty()) {
            out += "#";
            out += fragment;
        }
        return out;
    }

    Base::Result<Uri, IoError> ParseUri(std::string_view s) {
        if (s.empty()) {
            return Base::Err(IoError::Make(Engine::IO::IoErrorCode::InvalidPath,
                                        "uri is empty"));
        }

        if (const auto schemeEnd = s.find("://");
            schemeEnd != std::string_view::npos && SchemeFrom(ToLower(s.substr(0, schemeEnd))) == UriScheme::File) {
            auto filePath = FilePathFromRest(s.substr(schemeEnd + 3));
            if (!filePath) return Base::Err(std::move(filePath.error()));
            Uri uri;
            uri.scheme = UriScheme::File;
            uri.path = Path::FromNormalized(std::move(filePath.value()));
            return uri;
        }

        // split fragment
        std::string_view base = s;
        std::string_view frag;
        if (auto pos = base.find('#'); pos != std::string_view::npos) {
            frag = base.substr(pos + 1);
            base = base.substr(0, pos);
        }

        // split query
        std::string_view query;
        if (auto pos = base.find('?'); pos != std::string_view::npos) {
            query = base.substr(pos + 1);
            base = base.substr(0, pos);
        }

        // scheme?
        Uri uri;
        uri.query = std::string(query);
        uri.fragment = std::string(frag);

        const auto schemePos = base.find("://");
        if (schemePos == std::string_view::npos) {
            // スキーム無し：相対パスとして扱う（厳格）
            NormalizeOptions opt{};
            opt.rejectAbsoluteLike = true;
            opt.rejectTraversal    = true;

            auto p = Path::Parse(base, opt);
            if (!p) {
                return Base::Err(p.error());
            }

            uri.scheme = UriScheme::None;
            uri.path = std::move(p.value());
            return uri;
        }

        // scheme part
        const std::string schemeLower = ToLower(base.substr(0, schemePos));
        uri.scheme = SchemeFrom(schemeLower);
        if (uri.scheme == UriScheme::Unknown) {
            uri.schemeText = std::string(base.substr(0, schemePos));
        }

        std::string_view rest = base.substr(schemePos + 3); // after "://"

        // authority + path
        // authority は次の "/" まで（なければ全体が authority とみなされる）
        std::string_view auth;
        std::string_view pathPart;

        if (auto slash = rest.find('/'); slash == std::string_view::npos) {
            auth = rest;
            pathPart = std::string_view{};
        } else {
            auth = rest.substr(0, slash);
            pathPart = rest.substr(slash + 1); // "/" を除いて論理パスへ
        }

        uri.authority = std::string(auth);

        // scheme ごとにパス制約（ここは“安全側”に倒す）
        NormalizeOptions opt{};
        opt.rejectNullByte = true;

        // asset/http/https/unknown は基本「相対論理パス」を想定（file:// は上で処理済み）
        opt.rejectAbsoluteLike = true;
        opt.rejectTraversal    = true;

        // http/https の場合、ここで path を “エンジンの Path” にするかは用途次第。
        // 今回は設計維持のため、同じ Path として扱う（必要なら別設計に分離可能）。
        if (!pathPart.empty()) {
            auto p = Path::Parse(pathPart, opt);
            if (!p) {
                return Base::Err(p.error());
            }
            uri.path = std::move(p.value());
        } else {
            uri.path = Path::FromNormalized(std::string{});
        }

        // unknown scheme は「未対応」として弾きたいならここで返す
        //（今回は“パースは通す”方針。運用側で禁止してOK）
        return uri;;
    }

    Uri ParseUriLoose(std::string_view s) {
        Uri u;

        auto pos = s.find("://");
        if (pos == std::string_view::npos) {
            u.path = Path::FromNormalized(std::string(s));
            return u;
        }

        // "://"" の直後から
        std::string_view rest = s.substr(pos + 3);

        if (SchemeFrom(ToLower(s.substr(0, pos))) == UriScheme::File) {
            if (auto filePath = FilePathFromRest(rest)) {
                u.scheme = UriScheme::File;
                u.path = Path::FromNormalized(std::move(filePath.value()));
                return u;
            }
            // A file URI that ParseUri rejects is no file URI here either: it keeps no scheme, so it
            // never names a native path (docs/architecture/io.md).
        } else {
            u.scheme = SchemeFrom(std::string(s.substr(0, pos)));
        }

        // "res:///a" などは先頭スラッシュを剥がして相対寄りに
        while (!rest.empty() && (rest.front() == '/' || rest.front() == '\\')) {
            rest.remove_prefix(1);
        }

        u.path =  Path::FromNormalized(std::string(rest));
        return u;
    }

    Base::Result<Uri, IoError> FileUriFromNativePath(std::string_view utf8AbsolutePath) {
        const std::string path = ToForwardSlashes(utf8AbsolutePath);
        if (HasNamespacePrefix(path)) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath,
                                           "long-path and device prefixes never appear in engine/io paths",
                                           std::string(utf8AbsolutePath)));
        }
        // "\x" and "C:" are relative on Windows: the root, or the current directory, of the current drive.
        if ((utf8AbsolutePath.starts_with('\\') && !(path.size() >= 2 && path[1] == '/')) ||
            (path.size() == 2 && IsDriveLetter(path))) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "path is relative to the current drive",
                                           std::string(utf8AbsolutePath)));
        }
        if (LooksLikeDriveUnderRoot(path)) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath,
                                           "a POSIX path whose first segment looks like a drive has no file URI",
                                           std::string(utf8AbsolutePath)));
        }
        auto canonical = CanonicalFilePath(path);
        if (!canonical) return Base::Err(std::move(canonical.error()));
        Uri uri;
        uri.scheme = UriScheme::File;
        uri.path = Path::FromNormalized(std::move(canonical.value()));
        return uri;
    }

    Base::Result<std::string, IoError> NativePathFromFileUri(const Uri& uri) {
        if (uri.scheme != UriScheme::File) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "not a file URI", uri.ToString()));
        }
        if (!uri.query.empty() || !uri.fragment.empty()) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "a file URI has no query or fragment",
                                           uri.path.Str()));
        }
        std::string native = uri.path.Str();
        if (!IsLocalAuthority(uri.authority)) {
            // A host set by hand: the UNC path //host + path.
            if (IsDriveLetter(uri.authority) || uri.authority.find_first_of("/\\") != std::string::npos ||
                (!native.empty() && native.front() != '/')) {
                return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "file URI host is not a UNC host",
                                               uri.authority));
            }
            native = "//" + uri.authority + native;
        }
        // Built by hand, a Uri may hold any text: only the mapping's canonical form names a path.
        if (native.find('\\') != std::string::npos || LooksLikeDriveUnderRoot(native)) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "file URI path is not in canonical form",
                                           native));
        }
        auto canonical = CanonicalFilePath(native);
        if (!canonical) return Base::Err(std::move(canonical.error()));
        if (canonical.value() != native) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "file URI path is not in canonical form",
                                           native));
        }
        return native;
    }

} // namespace Engine::IO::Path
