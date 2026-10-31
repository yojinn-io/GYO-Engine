#include "gyo/audio_editor/CatalogRegistration.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <set>
#include <utility>
#include <vector>

namespace Gyo::Tools::AudioEditor {
namespace {

using Json = nlohmann::ordered_json;

constexpr std::string_view kSoundType = "sound";

AudioEditorError CatalogError(std::string message, std::string detail = {}) {
    return AudioEditorError::Make(AudioEditorErrorCode::InvalidCatalog, std::move(message), std::move(detail));
}

AudioEditorError RegistrationError(std::string message, std::string detail = {}) {
    return AudioEditorError::Make(AudioEditorErrorCode::InvalidRegistration, std::move(message), std::move(detail));
}

bool IsSpace(const char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Where the assets array and its elements are in a text that already parsed
// as JSON, so only strings and nesting need tracking.
struct AssetsLayout final {
    std::size_t arrayOpen{};
    std::vector<std::pair<std::size_t, std::size_t>> elements; // [begin, end)
};

std::optional<AssetsLayout> FindAssets(const std::string_view text) {
    AssetsLayout layout;
    int depth = 0;
    bool inAssets = false;
    int assetsDepth = 0;
    std::size_t elementBegin = 0;
    for (std::size_t at = 0; at < text.size(); ++at) {
        const char c = text[at];
        if (c == '"') {
            const std::size_t begin = at;
            for (++at; at < text.size() && text[at] != '"'; ++at) {
                if (text[at] == '\\') ++at;
            }
            if (depth == 1 && !inAssets) {
                std::size_t next = at + 1;
                while (next < text.size() && IsSpace(text[next])) ++next;
                if (next < text.size() && text[next] == ':' && text.substr(begin, at + 1 - begin) == "\"assets\"") {
                    ++next;
                    while (next < text.size() && IsSpace(text[next])) ++next;
                    if (next >= text.size() || text[next] != '[') return std::nullopt;
                    layout.arrayOpen = next;
                    inAssets = true;
                    assetsDepth = depth + 1;
                    depth = assetsDepth;
                    at = next;
                }
            }
            continue;
        }
        if (c == '{' || c == '[') {
            if (inAssets && depth == assetsDepth) elementBegin = at;
            ++depth;
        } else if (c == '}' || c == ']') {
            --depth;
            if (inAssets && depth == assetsDepth) {
                layout.elements.emplace_back(elementBegin, at + 1);
            } else if (inAssets && depth == assetsDepth - 1) {
                return layout; // the assets array closed
            }
        }
    }
    return std::nullopt;
}

std::string EntryText(const SoundRegistration& registration) {
    return "{ \"id\": " + Json(registration.id).dump() + ", \"type\": \"sound\", \"path\": " +
           Json(registration.path).dump() + " }";
}

std::size_t LineStart(const std::string_view text, std::size_t at) {
    while (at > 0 && text[at - 1] != '\n') --at;
    return at;
}

// The whitespace that starts the line holding at.
std::string LeadingSpace(const std::string_view text, const std::size_t at) {
    std::size_t end = LineStart(text, at);
    const std::size_t begin = end;
    while (end < text.size() && (text[end] == ' ' || text[end] == '\t')) ++end;
    return std::string(text.substr(begin, end - begin));
}

// The whitespace before begin on its line, or the line's own indentation and
// two spaces when something else precedes it.
std::string Indentation(const std::string_view text, const std::size_t begin) {
    const std::size_t lineStart = LineStart(text, begin);
    for (const char c : text.substr(lineStart, begin - lineStart)) {
        if (c != ' ' && c != '\t') return LeadingSpace(text, begin) + "  ";
    }
    return std::string(text.substr(lineStart, begin - lineStart));
}

Result<Json, AudioEditorError> ParseCatalog(const std::string_view text) {
    Json catalog = Json::parse(text, nullptr, false);
    if (catalog.is_discarded() || !catalog.is_object()) {
        return Engine::Base::Err(CatalogError("the catalog is not a JSON object"));
    }
    const auto version = catalog.find("version");
    if (version == catalog.end() || !version->is_number_integer() || *version != 1) {
        return Engine::Base::Err(CatalogError("expected integer version 1"));
    }
    const auto assets = catalog.find("assets");
    if (assets == catalog.end() || !assets->is_array()) {
        return Engine::Base::Err(CatalogError("expected an assets array"));
    }
    std::set<std::string> ids;
    for (const Json& entry : *assets) {
        for (const char* field : {"id", "type", "path"}) {
            if (!entry.is_object() || !entry.contains(field) || !entry[field].is_string() ||
                entry[field].get_ref<const std::string&>().empty()) {
                return Engine::Base::Err(CatalogError("asset entries require nonempty string id, type and path"));
            }
        }
        if (!ids.insert(entry["id"].get<std::string>()).second) {
            return Engine::Base::Err(CatalogError("duplicate asset id", entry["id"].get<std::string>()));
        }
    }
    return catalog;
}

} // namespace

bool IsValidAssetPath(const std::string_view path) noexcept {
    if (path.empty() || path == "content.json" || path.front() == '/' || path.find('\\') != std::string_view::npos ||
        path.find(':') != std::string_view::npos) {
        return false;
    }
    std::size_t begin = 0;
    while (true) {
        const std::size_t end = path.find('/', begin);
        const std::string_view segment = path.substr(begin, end == std::string_view::npos ? end : end - begin);
        if (segment.empty() || segment == "." || segment == "..") return false;
        if (end == std::string_view::npos) return true;
        begin = end + 1;
    }
}

Result<CatalogUpdate, AudioEditorError> RegisterSound(const std::string_view catalogText,
                                                      const SoundRegistration& registration) {
    auto parsed = ParseCatalog(catalogText);
    if (!parsed) {
        return Engine::Base::Err(std::move(parsed).error());
    }
    if (registration.id.empty()) {
        return Engine::Base::Err(RegistrationError("the asset id is empty"));
    }
    if (!IsValidAssetPath(registration.path)) {
        return Engine::Base::Err(RegistrationError("not a relative asset path", registration.path));
    }
    const Json& assets = (*parsed)["assets"];
    std::optional<std::size_t> existing;
    for (std::size_t index = 0; index < assets.size(); ++index) {
        const Json& entry = assets[index];
        if (entry["id"] == registration.id) {
            // ParseCatalog checked the field is a string; comparing the json
            // with a string_view directly is ambiguous on MSVC.
            if (entry["type"].get_ref<const std::string&>() != kSoundType) {
                return Engine::Base::Err(RegistrationError("the id is registered with another type",
                                                           registration.id + " is " + entry["type"].get<std::string>()));
            }
            existing = index;
        } else if (entry["path"] == registration.path) {
            return Engine::Base::Err(RegistrationError("the path is registered under another id",
                                                       registration.path + " is " + entry["id"].get<std::string>()));
        }
    }

    const std::optional<AssetsLayout> layout = FindAssets(catalogText);
    if (!layout || layout->elements.size() != assets.size()) {
        return Engine::Base::Err(CatalogError("the assets array could not be located in the text"));
    }
    CatalogUpdate update;
    const std::string entry = EntryText(registration);
    std::string text(catalogText);
    if (existing) {
        const auto [begin, end] = layout->elements[*existing];
        const std::string& oldPath = assets[*existing]["path"].get_ref<const std::string&>();
        if (oldPath != registration.path) update.previousPath = oldPath;
        text.replace(begin, end - begin, entry);
    } else if (layout->elements.empty()) {
        std::size_t close = layout->arrayOpen + 1;
        while (close < text.size() && IsSpace(text[close])) ++close;
        const std::string outer = LeadingSpace(text, layout->arrayOpen);
        text.replace(layout->arrayOpen + 1, close - layout->arrayOpen - 1, "\n" + outer + "  " + entry + "\n" + outer);
        update.added = true;
    } else {
        const auto [begin, end] = layout->elements.back();
        text.insert(end, ",\n" + Indentation(text, begin) + entry);
        update.added = true;
    }

    // The edit must change exactly that entry: check the result as data.
    auto reparsed = ParseCatalog(text);
    Json expected = *parsed;
    const Json newEntry = {{"id", registration.id}, {"type", kSoundType}, {"path", registration.path}};
    if (existing) {
        expected["assets"][*existing] = newEntry;
    } else {
        expected["assets"].push_back(newEntry);
    }
    if (!reparsed || *reparsed != expected) {
        return Engine::Base::Err(CatalogError("editing the catalog text changed more than the entry"));
    }
    update.text = std::move(text);
    return update;
}

} // namespace Gyo::Tools::AudioEditor
