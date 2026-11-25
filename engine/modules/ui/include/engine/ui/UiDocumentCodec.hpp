#pragma once

// Reading, validating and writing the gyo.ui Data Contract (docs/architecture/target/v2/ui.zh-Hant.md, section 4.1;
// Design Invariant UI-2). Reading and writing go through the JSON Module (strict RFC 8259 reading; one output
// format, J-5). Ported from master's engine/ui (UiDocumentCodec, UiValidation, UiSerialization).

#include <string>
#include <string_view>

#include "engine/ui/UiDocument.hpp"

namespace GYO::Engine::Ui {

class UiDocumentCodec final {
public:
    // Reads and validates a document. source names it in failures (a path or an asset id).
    [[nodiscard]] static UiResult<UiDocument> Parse(std::string_view utf8Json, std::string_view source = {});

    [[nodiscard]] static UiResult<void> Validate(const UiDocument& document);

    // UTF-8 JSON with two-space indentation (master's layout): fixed field ordering, uppercase #RRGGBBAA colours and
    // one trailing newline. A document that does not validate is not written.
    [[nodiscard]] static UiResult<std::string> Serialize(const UiDocument& document);
};

} // namespace GYO::Engine::Ui
