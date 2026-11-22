#include "engine/io/path/PathUtils.hpp"

#include <cctype>
#include <string>
#include <vector>

namespace GYO::Engine::IO::Path {

    static inline bool IsAlpha(char c) noexcept {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    static bool ContainsNullByte(std::string_view s) noexcept {
        for (char ch : s) {
            if (ch == '\0') return true;
        }
        return false;
    }

    static bool IsAbsoluteLike(std::string_view s) noexcept {
        if (s.empty()) return false;

        // POSIX absolute "/..."
        if (s.front() == '/') return true;

        // Windows UNC "\\server\share" or "//server/share"
        if (s.size() >= 2) {
            if ((s[0] == '\\' && s[1] == '\\') || (s[0] == '/' && s[1] == '/')) return true;
        }

        // Windows drive "C:" or "C:\"
        if (s.size() >= 2 && IsAlpha(s[0]) && s[1] == ':') return true;

        return false;
    }

    static bool ContainsTraversal(std::string_view s) noexcept {
        // セグメント単位で ".." を検出（"/../" や 末尾 ".." など）
        // ここは簡易：Normalize 内で確実に処理するので、早期判定用途。
        size_t i = 0;
        while (i < s.size()) {
            // skip separators
            while (i < s.size() && (s[i] == '/' || s[i] == '\\')) ++i;
            if (i >= s.size()) break;

            size_t j = i;
            while (j < s.size() && s[j] != '/' && s[j] != '\\') ++j;
            std::string_view seg = s.substr(i, j - i);
            if (seg == "..") return true;
            i = j;
        }
        return false;
    }

    Base::Result<std::string, IOError> Normalize(std::string_view raw, const NormalizeOptions& opt) {
        if (opt.rejectNullByte && ContainsNullByte(raw)) {
            return Base::Err(IOError::Make(IOErrorCode::InvalidPath,
                                        "path contains null byte",
                                        std::string(raw)));
        }

        if (opt.rejectAbsoluteLike && IsAbsoluteLike(raw)) {
            return Base::Err(IOError::Make(IOErrorCode::InvalidPath,
                                        "absolute-like path is not allowed",
                                        std::string(raw)));
        }

        // まずはスラッシュ統一（必要なら）
        std::string s;
        s.reserve(raw.size());
        for (char ch : raw) {
            if (opt.convertBackslash && ch == '\\') s.push_back('/');
            else s.push_back(ch);
        }

        // 末尾スラッシュ保持判定
        const bool hadTrailingSlash = (!s.empty() && s.back() == '/');

        // collapse slashes（連続/を1つに）
        if (opt.collapseSlashes) {
            std::string t;
            t.reserve(s.size());
            bool prevSlash = false;
            for (char ch : s) {
                if (ch == '/') {
                    if (!prevSlash) t.push_back(ch);
                    prevSlash = true;
                } else {
                    t.push_back(ch);
                    prevSlash = false;
                }
            }
            s.swap(t);
        }

        // セグメント分解＆スタック処理
        std::vector<std::string_view> stack;
        stack.reserve(16);

        size_t i = 0;
        while (i < s.size()) {
            while (i < s.size() && s[i] == '/') ++i; // skip '/'
            if (i >= s.size()) break;

            size_t j = i;
            while (j < s.size() && s[j] != '/') ++j;

            std::string_view seg = std::string_view(s).substr(i, j - i);
            i = j;

            if (opt.removeDot && seg == ".") {
                continue;
            }

            if (seg == "..") {
                if (opt.resolveDotDot) {
                    if (!stack.empty()) {
                        stack.pop_back();
                        continue;
                    }
                    // root越え（相対パスで上に出る）
                    return Base::Err(IOError::Make(IOErrorCode::PathEscapesRoot,
                                                "path escapes root by '..'",
                                                std::string(raw)));
                } else if (opt.rejectTraversal) {
                    return Base::Err(IOError::Make(IOErrorCode::InvalidPath,
                                                "path traversal '..' is not allowed",
                                                std::string(raw)));
                }
                // resolveDotDot=false かつ rejectTraversal=false のときは ".." を残す
                stack.push_back(seg);
                continue;
            }

            // 通常セグメント
            stack.push_back(seg);
        }

        // 組み立て
        std::string out;
        // 大雑把に予約
        out.reserve(s.size());

        for (size_t k = 0; k < stack.size(); ++k) {
            if (k != 0) out.push_back('/');
            out.append(stack[k].data(), stack[k].size());
        }

        if (opt.keepTrailingSlash && hadTrailingSlash && !out.empty()) {
            out.push_back('/');
        }

        // 追加の traversal 判定（resolveDotDot=false時の保険）
        if (opt.rejectTraversal && ContainsTraversal(out)) {
            return Base::Err(IOError::Make(IOErrorCode::InvalidPath,
                                        "path traversal '..' is not allowed",
                                        out));
        }

        return out;
    }

    std::string Parent(std::string_view s) {
        if (s.empty()) return {};
        // 末尾スラッシュ除去
        while (!s.empty() && s.back() == '/') s.remove_suffix(1);
        if (s.empty()) return {};

        const size_t pos = s.find_last_of('/');
        if (pos == std::string_view::npos) return {};
        return std::string(s.substr(0, pos));
    }

    std::string RemoveDotSegments(std::string_view path, bool& escapedAboveRoot) {
        escapedAboveRoot = false;

        // "/" 区切り想定（NormalizeSlashes で揃っている前提）
        // ただし Windows drive/UNC も来るので prefix を扱う
        std::string prefix;
        std::string_view p = path;

        // drive prefix "C:/"
        if (p.size() >= 3 &&
            std::isalpha(static_cast<unsigned char>(p[0])) &&
            p[1] == ':' &&
            p[2] == '/') {
            prefix = std::string(p.substr(0, 3)); // "C:/"
            p.remove_prefix(3);
        }
        // UNC prefix "//server/share/" は簡易対応：先頭 "//" を prefix として保持
        else if (p.size() >= 2 && p[0] == '/' && p[1] == '/') {
            prefix = "//";
            p.remove_prefix(2);
        }
        // unix absolute "/" を prefix として保持
        else if (!p.empty() && p[0] == '/') {
            prefix = "/";
            p.remove_prefix(1);
        }

        std::vector<std::string> stack;
        stack.reserve(16);

        size_t i = 0;
        while (i <= p.size()) {
            size_t j = p.find('/', i);
            if (j == std::string_view::npos) j = p.size();

            std::string_view seg = p.substr(i, j - i);
            i = j + 1;

            if (seg.empty() || seg == ".") {
                continue;
            }
            if (seg == "..") {
                if (!stack.empty()) {
                    stack.pop_back();
                } else {
                    // ルートより上に出ようとした
                    escapedAboveRoot = true;
                }
                continue;
            }

            stack.emplace_back(seg);
        }

        std::string out = prefix;
        for (size_t k = 0; k < stack.size(); ++k) {
            if (!out.empty() && out.back() != '/') out.push_back('/');
            out += stack[k];
        }

        // prefix が "/" で stack が空のときは "/" を返す（絶対パスのルート）
        if (!prefix.empty() && stack.empty()) {
            return out.empty() ? prefix : out;
        }

        // 相対で空になった場合は空文字
        return out;
    }

} // namespace GYO::Engine::IO::Path
