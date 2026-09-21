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

    std::string Uri::ToString() const {
        std::string out;

        auto appendPath = [&]() {
            out += path.Str();
        };

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
            return Base::Result<Uri, IoError>::Err(
                IoError::Make(Engine::IO::IoErrorCode::InvalidPath,
                                        "uri is empty")
            );
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
                return Base::Result<Uri, IoError>::Err(p.error());
            }

            uri.scheme = UriScheme::None;
            uri.path = std::move(p.value());
            return Base::Result<Uri, IoError>::Ok(uri);
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

        if (uri.scheme == UriScheme::File) {
            // file:// は tool 用途が多いので absolute-like を許可しておく（必要なら runtime 側で禁止）
            opt.rejectAbsoluteLike = false;
            opt.rejectTraversal    = true;
        } else {
            // asset/http/https/unknown は基本「相対論理パス」を想定
            opt.rejectAbsoluteLike = true;
            opt.rejectTraversal    = true;
        }

        // http/https の場合、ここで path を “エンジンの Path” にするかは用途次第。
        // 今回は設計維持のため、同じ Path として扱う（必要なら別設計に分離可能）。
        if (!pathPart.empty()) {
            auto p = Path::Parse(pathPart, opt);
            if (!p) {
                return Base::Result<Uri, IoError>::Err(p.error());
            }
            uri.path = std::move(p.value());
        } else {
            uri.path = Path::FromNormalized(std::string{});
        }

        // unknown scheme は「未対応」として弾きたいならここで返す
        //（今回は“パースは通す”方針。運用側で禁止してOK）
        return Base::Result<Uri, IoError>::Ok(uri);;
    }

    Uri ParseUriLoose(std::string_view s) {
        Uri u;

        auto pos = s.find("://");
        if (pos == std::string_view::npos) {
            u.path = Path::FromNormalized(std::string(s));
            return u;
        }

        u.scheme = SchemeFrom(std::string(s.substr(0, pos)));

        // "://"" の直後から
        std::string_view rest = s.substr(pos + 3);

        // "res:///a" などは先頭スラッシュを剥がして相対寄りに
        while (!rest.empty() && (rest.front() == '/' || rest.front() == '\\')) {
            rest.remove_prefix(1);
        }

        u.path =  Path::FromNormalized(std::string(rest));
        return u;
    }

} // namespace Engine::IO::Path
