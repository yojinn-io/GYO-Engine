#include "engine/logging/MemorySink.hpp"

#include <new>
#include <string_view>

namespace GYO::Engine::Logging {

const StoredField* StoredRecord::Find(const std::string_view key) const noexcept {
    for (const StoredField& field : fields) {
        if (field.key == key) return &field;
    }
    return nullptr;
}

StoredRecord Store(const Record& record) {
    StoredRecord stored;
    stored.level = record.level;
    stored.category.assign(record.category);
    stored.text.assign(record.text);
    stored.wall = record.wall;
    stored.monoNs = record.monoNs;
    stored.world.assign(record.world);
    stored.worldInterrupted = record.worldInterrupted;
    stored.fields.reserve(record.fields.size());
    for (const Field& field : record.fields) {
        StoredField out;
        out.key.assign(field.key);
        out.type = field.value.GetType();
        switch (out.type) {
        case FieldValue::Type::Int: out.intValue = field.value.AsInt(); break;
        case FieldValue::Type::Uint: out.uintValue = field.value.AsUint(); break;
        case FieldValue::Type::Double: out.doubleValue = field.value.AsDouble(); break;
        case FieldValue::Type::Bool: out.boolValue = field.value.AsBool(); break;
        case FieldValue::Type::String: out.text.assign(field.value.AsString()); break;
        }
        stored.fields.push_back(std::move(out));
    }
    return stored;
}

void MemorySink::Write(const Record& record) noexcept {
    try {
        StoredRecord stored = Store(record);
        std::scoped_lock lock(mutex_);
        records_.push_back(std::move(stored));
    } catch (...) {
        std::scoped_lock lock(mutex_);
        ++lost_;
    }
}

bool MemorySink::WriteFatal(const Record& record, const std::string_view jsonLine,
                            const std::string_view textLine) noexcept {
    try {
        StoredRecord stored = Store(record);
        stored.fatal = true;
        std::string json(jsonLine);
        std::string text(textLine);
        std::scoped_lock lock(mutex_);
        records_.push_back(std::move(stored));
        fatalJson_.push_back(std::move(json));
        fatalText_.push_back(std::move(text));
        return true;
    } catch (...) {
        std::scoped_lock lock(mutex_);
        ++lost_;
        return false;
    }
}

std::vector<StoredRecord> MemorySink::Records() const {
    std::scoped_lock lock(mutex_);
    return records_;
}

std::vector<std::string> MemorySink::FatalJsonLines() const {
    std::scoped_lock lock(mutex_);
    return fatalJson_;
}

std::vector<std::string> MemorySink::FatalTextLines() const {
    std::scoped_lock lock(mutex_);
    return fatalText_;
}

std::uint64_t MemorySink::Lost() const {
    std::scoped_lock lock(mutex_);
    return lost_;
}

void MemorySink::Clear() {
    std::scoped_lock lock(mutex_);
    records_.clear();
    fatalJson_.clear();
    fatalText_.clear();
    lost_ = 0;
}

} // namespace GYO::Engine::Logging
