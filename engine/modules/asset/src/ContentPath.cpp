#include "engine/asset/ContentPath.hpp"

#include <vector>

namespace GYO::Engine::Asset {
namespace {

// Splits on '/', keeping empty segments.
std::vector<std::string_view> Segments(const std::string_view path) {
    std::vector<std::string_view> segments;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = path.find('/', start);
        if (end == std::string_view::npos) {
            segments.push_back(path.substr(start));
            return segments;
        }
        segments.push_back(path.substr(start, end - start));
        start = end + 1;
    }
}

} // namespace

bool IsRelativeContentPath(const std::string_view path) noexcept {
    if (path.empty()) return false;
    for (const char character : path) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20 || character == '\\' || character == ':') return false;
    }
    // Every segment between separators is a name: a leading, trailing or
    // repeated '/' makes an empty one.
    std::size_t start = 0;
    while (true) {
        const std::size_t end = path.find('/', start);
        const std::string_view segment =
            path.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        if (segment.empty() || segment == "." || segment == "..") return false;
        if (end == std::string_view::npos) return true;
        start = end + 1;
    }
}

std::string PathExtension(const std::string_view path) {
    const std::size_t slash = path.rfind('/');
    const std::string_view last = slash == std::string_view::npos ? path : path.substr(slash + 1);
    const std::size_t dot = last.rfind('.');
    if (dot == std::string_view::npos) return {};
    std::string extension{last.substr(dot + 1)};
    for (char& character : extension) {
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    }
    return extension;
}

std::optional<std::string> ResolveReferencePath(const std::string_view parentPath, const std::string_view reference) {
    std::vector<std::string_view> resolved = Segments(parentPath);
    resolved.pop_back(); // the parent's own file name
    for (const std::string_view segment : Segments(reference)) {
        // An absolute path (a leading '/'), an empty reference and a repeated or
        // trailing '/' leave an empty segment.
        if (segment.empty()) return std::nullopt;
        if (segment == ".") continue;
        if (segment == "..") {
            if (resolved.empty()) return std::nullopt; // above the content root
            resolved.pop_back();
            continue;
        }
        resolved.push_back(segment);
    }
    std::string path;
    for (std::size_t index = 0; index < resolved.size(); ++index) {
        if (index != 0) path += '/';
        path += resolved[index];
    }
    // A '\\', a ':' or a control character, and a reference that leaves nothing.
    if (!IsRelativeContentPath(path)) return std::nullopt;
    return path;
}

} // namespace GYO::Engine::Asset
