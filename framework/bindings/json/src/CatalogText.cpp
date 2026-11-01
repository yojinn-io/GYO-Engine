#include "gyo/framework/json/CatalogText.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace Gyo::Tools::Framework {
namespace {

using Json = nlohmann::ordered_json;

CatalogError InvalidCatalog(std::string message, std::string detail = {}) {
    return CatalogError::Make(CatalogErrorCode::InvalidCatalog, std::move(message), std::move(detail));
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

std::string EntryText(const CatalogEntry& entry) {
    return "{ \"id\": " + Json(entry.id).dump() + ", \"type\": " + Json(entry.type).dump() +
           ", \"path\": " + Json(entry.path).dump() + " }";
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

} // namespace

Result<std::vector<CatalogEntry>, CatalogError> ReadCatalog(const std::string_view catalogText) {
    Engine::Asset::Catalog::CatalogParser parser;
    auto entries = parser.Parse(catalogText, "catalog");
    if (!entries) {
        return Engine::Base::Err(InvalidCatalog("not a catalog v1", entries.error().message));
    }
    auto checked = CheckCatalogEntries(entries.value());
    if (!checked) {
        return Engine::Base::Err(std::move(checked).error());
    }
    return std::move(entries).value();
}

Result<CatalogUpdate, CatalogError> RegisterAsset(const std::string_view catalogText, const CatalogEntry& entry,
                                                  const RegistrationPolicy& policy) {
    auto entries = ReadCatalog(catalogText);
    if (!entries) {
        return Engine::Base::Err(std::move(entries).error());
    }
    auto plan = PlanRegistration(entries.value(), entry, policy);
    if (!plan) {
        return Engine::Base::Err(std::move(plan).error());
    }
    const std::optional<AssetsLayout> layout = FindAssets(catalogText);
    if (!layout || layout->elements.size() != entries->size()) {
        return Engine::Base::Err(InvalidCatalog("the assets array could not be located in the text"));
    }
    CatalogUpdate update;
    update.added = plan->added;
    update.previousPath = plan->previousPath;
    const std::string entryText = EntryText(entry);
    std::string text(catalogText);
    if (!plan->added) {
        const auto [begin, end] = layout->elements[plan->index];
        text.replace(begin, end - begin, entryText);
    } else if (layout->elements.empty()) {
        std::size_t close = layout->arrayOpen + 1;
        while (close < text.size() && IsSpace(text[close])) ++close;
        const std::string outer = LeadingSpace(text, layout->arrayOpen);
        text.replace(layout->arrayOpen + 1, close - layout->arrayOpen - 1, "\n" + outer + "  " + entryText + "\n" + outer);
    } else {
        const auto [begin, end] = layout->elements.back();
        text.insert(end, ",\n" + Indentation(text, begin) + entryText);
    }

    // The edit must change exactly that entry: check the result as data.
    Json expected = Json::parse(catalogText, nullptr, false);
    const Json reparsed = Json::parse(text, nullptr, false);
    const Json newEntry = {{"id", entry.id}, {"type", entry.type}, {"path", entry.path}};
    if (plan->added) {
        expected["assets"].push_back(newEntry);
    } else {
        expected["assets"][plan->index] = newEntry;
    }
    if (reparsed.is_discarded() || reparsed != expected || !ReadCatalog(text)) {
        return Engine::Base::Err(InvalidCatalog("editing the catalog text changed more than the entry"));
    }
    update.text = std::move(text);
    return update;
}

} // namespace Gyo::Tools::Framework
