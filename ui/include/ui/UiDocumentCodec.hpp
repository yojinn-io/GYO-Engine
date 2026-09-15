#pragma once

#include <string>
#include <string_view>

#include "ui/UiDocument.hpp"

namespace Engine::Ui {

class UiDocumentCodec final {
public:
    [[nodiscard]] static UiResult<UiDocument> Parse(
        std::string_view utf8Json,
        std::string_view source = {});

    [[nodiscard]] static UiResult<void> Validate(const UiDocument& document);

    // Deterministic UTF-8 JSON: fixed field ordering, two-space indentation,
    // uppercase #RRGGBBAA colors, and one trailing newline.
    [[nodiscard]] static UiResult<std::string> Serialize(
        const UiDocument& document);
};

} // namespace Engine::Ui
