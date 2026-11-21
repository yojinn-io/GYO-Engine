#include "engine/logging/RecordBuilder.hpp"

namespace GYO::Engine::Logging {

void RecordBuilder::Text(const std::string_view text) {
    text_.assign(text);
}

void RecordBuilder::AppendText(const std::string_view text) {
    text_.append(text);
}

void RecordBuilder::Add(const std::string_view key, const FieldValue value) {
    Stored stored{.keyOffset = arena_.size(), .keySize = key.size(), .value = value, .textOffset = 0, .textSize = 0};
    arena_.append(key);
    if (value.GetType() == FieldValue::Type::String) {
        stored.textOffset = arena_.size();
        stored.textSize = value.AsString().size();
        arena_.append(value.AsString());
    }
    stored_.push_back(stored);
}

void RecordBuilder::Clear() noexcept {
    text_.clear();
    arena_.clear();
    stored_.clear();
    resolved_.clear();
}

std::span<const Field> RecordBuilder::Fields() const {
    resolved_.clear();
    resolved_.reserve(stored_.size());
    const std::string_view arena = arena_;
    for (const Stored& stored : stored_) {
        const std::string_view key = arena.substr(stored.keyOffset, stored.keySize);
        if (stored.value.GetType() == FieldValue::Type::String) {
            resolved_.push_back(Field{key, FieldValue(arena.substr(stored.textOffset, stored.textSize))});
        } else {
            resolved_.push_back(Field{key, stored.value});
        }
    }
    return resolved_;
}

} // namespace GYO::Engine::Logging
