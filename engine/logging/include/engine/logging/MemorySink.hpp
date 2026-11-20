#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "engine/logging/Record.hpp"
#include "engine/logging/Sink.hpp"

// A sink that keeps copies of its records in memory, for tests (section 5.7 of
// docs/architecture/target/v2/logging-declaration.zh-Hant.md, Feature F5).
// Any thread may write; Records, FatalLines and Clear take the same lock.

namespace GYO::Engine::Logging {

struct StoredField final {
    std::string key;
    FieldValue::Type type{FieldValue::Type::Int};
    std::int64_t intValue{};
    std::uint64_t uintValue{};
    double doubleValue{};
    bool boolValue{};
    std::string text;
};

struct StoredRecord final {
    Level level{Level::Info};
    std::string category;
    std::string text;
    WallTime wall;
    std::int64_t monoNs{};
    std::string world;
    bool worldInterrupted{false};
    std::vector<StoredField> fields;
    // Reached the sink through WriteFatal rather than Write.
    bool fatal{false};

    // The field named key, or nullptr.
    [[nodiscard]] const StoredField* Find(std::string_view key) const noexcept;
};

[[nodiscard]] StoredRecord Store(const Record& record);

class MemorySink final : public Sink {
public:
    void Write(const Record& record) noexcept override;
    bool WriteFatal(const Record& record, std::string_view jsonLine, std::string_view textLine) noexcept override;

    [[nodiscard]] std::vector<StoredRecord> Records() const;
    // The lines WriteFatal received, JSON first then text, in call order.
    [[nodiscard]] std::vector<std::string> FatalJsonLines() const;
    [[nodiscard]] std::vector<std::string> FatalTextLines() const;
    // Records that could not be stored (allocation failed).
    [[nodiscard]] std::uint64_t Lost() const;
    void Clear();

private:
    mutable std::mutex mutex_;
    std::vector<StoredRecord> records_;
    std::vector<std::string> fatalJson_;
    std::vector<std::string> fatalText_;
    std::uint64_t lost_ = 0;
};

} // namespace GYO::Engine::Logging
