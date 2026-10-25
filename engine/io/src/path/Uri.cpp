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

    // Normalizes a native absolute path in '/' form ("/a/b", "C:/a"): dot
    // segments resolve inside the root, escaping it is an error, the root
    // stays as written. A POSIX path whose first segment looks like a drive
    // ("/C:/x") has no file URI: its URI would name the drive path.
    static Base::Result<std::string, IoError> NormalizeFilePath(std::string_view path) {
        std::string_view root;
        if (!path.empty() && path.front() == '/') root = path.substr(0, 1);
        else if (StartsWithDrive(path)) root = path.substr(0, 2);
        else {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "file path is not absolute",
                                           std::string(path)));
        }
        NormalizeOptions opt{};
        opt.rejectAbsoluteLike = false;
        opt.rejectTraversal = true;
        auto rest = Normalize(path.substr(root.size()), opt);
        if (!rest) return Base::Err(std::move(rest.error()));
        std::string out(root);
        if (out.back() != '/') out.push_back('/');
        out += rest.value();
        if (root == "/" && StartsWithDrive(std::string_view(out).substr(1))) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath,
                                           "a POSIX path whose first segment looks like a drive has no file URI",
                                           out));
        }
        return out;
    }

    // The path of a file URI from what follows "file://": an empty or
    // localhost authority, then the path with its leading '/'. Backslashes in
    // the path are separators; the query and fragment separators are part of
    // a file path.
    static Base::Result<std::string, IoError> FilePathFromRest(std::string_view rest) {
        const auto slash = rest.find('/');
        const std::string_view authority = rest.substr(0, slash);
        const std::string path = slash == std::string_view::npos ? std::string{} : ToForwardSlashes(rest.substr(slash));
        if (IsDriveLetter(authority)) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath,
                                           "file URI has a drive letter in its authority; write file:///C:/...",
                                           std::string(rest)));
        }
        if (!IsLocalAuthority(authority)) {
            return Base::Err(IoError::Make(IoErrorCode::NotSupported, "UNC file URIs are not supported",
                                           std::string(rest)));
        }
        if (path.empty()) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "file URI has no path", std::string(rest)));
        }
        if (HasNamespacePrefix(path) || HasNamespacePrefix(std::string_view(path).substr(1))) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath,
                                           "long-path and device prefixes never appear in engine/io paths",
                                           std::string(rest)));
        }
        // "file:////server/share" is the other spelling of a UNC URI.
        if (path.starts_with("//")) {
            return Base::Err(IoError::Make(IoErrorCode::NotSupported, "UNC file URIs are not supported",
                                           std::string(rest)));
        }
        // "/C:/x" names the drive path "C:/x".
        std::string_view native = path;
        if (StartsWithDrive(native.substr(1))) native.remove_prefix(1);
        return NormalizeFilePath(native);
    }

    std::string Uri::ToString() const {
        std::string out;

        auto appendPath = [&]() {
            out += path.Str();
        };

        if (scheme == UriScheme::File) {
            // The canonical "file:///<absolute path>"; parsing it gives the same Uri. A file URI has no
            // query or fragment. A UNC authority is printed, so parsing the text reports it.
            out = "file://";
            if (!IsLocalAuthority(authority)) out += authority;
            if (path.Empty() || path.Str().front() != '/') out += "/";
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
        if (path.starts_with("//")) {
            return Base::Err(IoError::Make(IoErrorCode::NotSupported, "UNC paths are not supported",
                                           std::string(utf8AbsolutePath)));
        }
        // "\x" and "C:" are relative on Windows: the root, or the current directory, of the current drive.
        if (utf8AbsolutePath.starts_with('\\') || (path.size() == 2 && IsDriveLetter(path))) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "path is relative to the current drive",
                                           std::string(utf8AbsolutePath)));
        }
        auto normalized = NormalizeFilePath(path);
        if (!normalized) return Base::Err(std::move(normalized.error()));
        Uri uri;
        uri.scheme = UriScheme::File;
        uri.path = Path::FromNormalized(std::move(normalized.value()));
        return uri;
    }

    Base::Result<std::string, IoError> NativePathFromFileUri(const Uri& uri) {
        if (uri.scheme != UriScheme::File) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "not a file URI", uri.ToString()));
        }
        if (!IsLocalAuthority(uri.authority)) {
            return Base::Err(IoError::Make(IoErrorCode::NotSupported, "UNC file URIs are not supported",
                                           uri.authority));
        }
        if (!uri.query.empty() || !uri.fragment.empty()) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "a file URI has no query or fragment",
                                           uri.path.Str()));
        }
        // Built by hand, a Uri may hold any text: only the normalized form of the mapping names a path.
        auto normalized = NormalizeFilePath(uri.path.Str());
        if (!normalized) return Base::Err(std::move(normalized.error()));
        if (normalized.value() != uri.path.Str()) {
            return Base::Err(IoError::Make(IoErrorCode::InvalidPath, "file URI path is not normalized",
                                           uri.path.Str()));
        }
        return uri.path.Str();
    }

} // namespace Engine::IO::Path
