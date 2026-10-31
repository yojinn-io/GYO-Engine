#pragma once

#include <nlohmann/json.hpp>

#include <span>
#include <string>
#include <string_view>

namespace Gyo::Tools::UiEditor {

// What the editor knows about each gyo.ui element kind, in one place: adding
// an element to the editor means adding one entry here. The authoritative
// schema is GYO::Ui's codec; tests keep this table in line with it.

// A color reference that only this kind has (text and background styles are
// shared and handled by the Inspector for every kind that carries them).
struct ElementColorField final {
    const char* label; // Inspector label
    const char* key;   // element field
    bool source;       // true: a color source object {"color": name}; false: a color name
};

// An integer or text field that only this kind has, edited in the Inspector.
struct ElementIntegerField final {
    const char* label;
    const char* key;
    int minimum;
    int maximum;
};

struct ElementStringField final {
    const char* label;
    const char* key;
};

struct ElementKind final {
    std::string_view type;
    bool focusable{};             // takes part in the canvas focus order
    std::string_view actionPayload; // payload of the action it emits; empty when it has none
    std::string_view bindingType;   // type of the binding it reads; empty when it has none
    std::span<const ElementColorField> colors;
    bool textureAsset{};          // references a texture asset
    bool styledText{};            // has the text style keys without a text source
    std::span<const ElementIntegerField> integers;
    std::span<const ElementStringField> strings;
};

// Every kind, in the order the editor offers them.
[[nodiscard]] std::span<const ElementKind> ElementKinds() noexcept;
[[nodiscard]] const ElementKind* FindElementKind(std::string_view type) noexcept;

// A new element of kind with its default fields. Declares in document the
// action and binding it needs, under ids not yet used there.
[[nodiscard]] nlohmann::json MakeDefaultElement(nlohmann::json& document, const ElementKind& kind);

// A new empty canvas under an unused id, its backdrop declared in document.
[[nodiscard]] nlohmann::json MakeDefaultCanvas(nlohmann::json& document);

// Whether node or one of its descendants (children and list templates) has id.
[[nodiscard]] bool ContainsNodeId(const nlohmann::json& node, std::string_view id);
// "<prefix>_<n>" for the first n that names no element in the document.
[[nodiscard]] std::string UniqueNodeId(const nlohmann::json& document, std::string_view prefix);
// "<prefix>.<n>" for the first n that no declaration in the array uses as id.
[[nodiscard]] std::string UniqueDeclarationId(const nlohmann::json& declarations, std::string_view prefix);

} // namespace Gyo::Tools::UiEditor
