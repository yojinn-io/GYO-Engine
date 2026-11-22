#pragma once

#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include "engine/logging/Level.hpp"

// One record as the Logger hands it to every sink (section 5.2 and 5.12 of
// docs/architecture/target/v2/logging-declaration.zh-Hant.md). Every view in a
// Record is valid only during the sink call that receives it: a sink that keeps
// a record copies what it needs.

namespace GYO::Engine::Logging {

// The value of one field: an integer, a double (non-finite values are allowed
// here; the JSON lines format writes them as agreed strings), a bool or text.
// Character types are refused so that a char is never written as a number by
// accident, and other pointers are refused so that they never become a bool.
class FieldValue final {
public:
    enum class Type : std::uint8_t { Int = 1, Uint, Double, Bool, String };

    template <class T>
        requires std::signed_integral<T> && (!std::same_as<T, char>) && (!std::same_as<T, wchar_t>)
    constexpr FieldValue(const T value) noexcept : type_(Type::Int), int_(value) {}

    template <class T>
        requires std::unsigned_integral<T> && (!std::same_as<T, bool>) && (!std::same_as<T, char>) &&
                 (!std::same_as<T, wchar_t>) && (!std::same_as<T, char8_t>) && (!std::same_as<T, char16_t>) &&
                 (!std::same_as<T, char32_t>)
    constexpr FieldValue(const T value) noexcept : type_(Type::Uint), uint_(value) {}

    template <std::floating_point T>
    constexpr FieldValue(const T value) noexcept : type_(Type::Double), double_(static_cast<double>(value)) {}

    constexpr FieldValue(const bool value) noexcept : type_(Type::Bool), bool_(value) {}
    constexpr FieldValue(const std::string_view value) noexcept : type_(Type::String), text_(value) {}
    constexpr FieldValue(const char* value) noexcept : type_(Type::String), text_(value) {}
    constexpr FieldValue(char* value) noexcept : type_(Type::String), text_(value) {}
    FieldValue(const std::string& value) noexcept : type_(Type::String), text_(value) {}

    FieldValue(char) = delete;
    FieldValue(wchar_t) = delete;
    FieldValue(char8_t) = delete;
    FieldValue(char16_t) = delete;
    FieldValue(char32_t) = delete;
    template <class T>
        requires(!std::same_as<std::remove_cv_t<T>, char>)
    FieldValue(T*) = delete;

    [[nodiscard]] constexpr Type GetType() const noexcept { return type_; }
    // Each accessor is for its own Type only (a Programmer Error otherwise is
    // not checked here: these are read by sinks right after a switch on GetType).
    [[nodiscard]] constexpr std::int64_t AsInt() const noexcept { return int_; }
    [[nodiscard]] constexpr std::uint64_t AsUint() const noexcept { return uint_; }
    [[nodiscard]] constexpr double AsDouble() const noexcept { return double_; }
    [[nodiscard]] constexpr bool AsBool() const noexcept { return bool_; }
    [[nodiscard]] constexpr std::string_view AsString() const noexcept { return text_; }

private:
    Type type_;
    union {
        std::int64_t int_;
        std::uint64_t uint_;
        double double_;
        bool bool_;
    };
    std::string_view text_;
};

// One key and its value. Keys are lower case with underscores by convention
// (section 5.12); a record must not write one key twice (the Logger turns such
// a record into an Error record, section 5.2).
struct Field final {
    std::string_view key;
    FieldValue value;
};

// The wall clock: milliseconds since the Unix epoch and the C runtime's offset
// from UTC at that instant (local minus UTC; +09:00 is 32400).
struct WallTime final {
    std::int64_t unixMs{};
    std::int32_t utcOffsetSeconds{};
};

struct Record final {
    Level level{Level::Info};
    std::string_view category;
    std::string_view text;
    WallTime wall;
    // The engine time base (GYO::Engine::Time::NowNs) when the record was written.
    std::int64_t monoNs{};
    // The World, when the record came from one (Forward, TakeKernelDiagnostics).
    std::string_view world;
    // The record was taken from a World that had already been interrupted (section 5.4).
    bool worldInterrupted{false};
    std::span<const Field> fields;
};

} // namespace GYO::Engine::Logging
