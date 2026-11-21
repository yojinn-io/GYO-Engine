#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/logging/Record.hpp"

// Collects the text and the fields of one record when they are composed, not
// given as views: a descriptor's Describe writes into one (section 5.3 of
// docs/architecture/target/v2/logging-declaration.zh-Hant.md). Keys and string
// values are copied, so what the caller passed may go away afterwards. Clear
// keeps the capacity, so a builder reused for the next record allocates only
// when a record is larger than every earlier one.

namespace GYO::Engine::Logging {

class RecordBuilder final {
public:
    RecordBuilder() = default;
    RecordBuilder(const RecordBuilder&) = delete;
    RecordBuilder& operator=(const RecordBuilder&) = delete;

    // Replaces the text.
    void Text(std::string_view text);
    void AppendText(std::string_view text);
    // Adds a field after the ones already added.
    void Add(std::string_view key, FieldValue value);
    void Clear() noexcept;

    [[nodiscard]] std::string_view TextView() const noexcept { return text_; }
    // Views into this builder: valid until the next call that changes it.
    [[nodiscard]] std::span<const Field> Fields() const;

private:
    struct Stored {
        std::size_t keyOffset;
        std::size_t keySize;
        FieldValue value;
        std::size_t textOffset; // String values only
        std::size_t textSize;
    };

    std::string text_;
    // Keys and string values, back to back; offsets survive a reallocation.
    std::string arena_;
    std::vector<Stored> stored_;
    mutable std::vector<Field> resolved_;
};

} // namespace GYO::Engine::Logging
