#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "engine/base/Assert.hpp"

// A Runtime Error's payload; see docs/architecture/error-handling.md.
//
// Every error type E used with Result satisfies CodedError:
// - code: an enum owned by the module, whose zero value is not a valid code
//   (each enum starts at 1), with a ToString(code) found by argument-dependent
//   lookup;
// - message: for people (logs, diagnostics), never parsed by code;
// - detail: optional context, such as a path or, at a module boundary, the
//   inner error's code name and detail;
// - no default constructor, so an E always describes a failure.

namespace Engine::Base {

template <class E>
concept CodedError = !std::is_default_constructible_v<E> && requires(const E& error) {
    requires std::is_enum_v<std::remove_cvref_t<decltype(error.code)>>;
    { error.message } -> std::convertible_to<const std::string&>;
    { error.detail } -> std::convertible_to<const std::string&>;
    { ToString(error.code) } -> std::convertible_to<std::string_view>;
};

// The common error type: Error<Code> for a module's code enum. Construct with
// Make; there is no "no error" value.
template <class Code>
class Error final {
    static_assert(std::is_enum_v<Code>, "Error<Code>: Code must be an enum type");

public:
    using code_type = Code;

    Code code;
    std::string message;
    std::string detail;

    // A zero code is not a valid failure (every code enum starts at 1).
    [[nodiscard]] static Error Make(const Code errorCode, std::string errorMessage,
                                    std::string errorDetail = {}) {
        GYO_ASSERT(static_cast<std::underlying_type_t<Code>>(errorCode) != 0);
        return Error(errorCode, std::move(errorMessage), std::move(errorDetail));
    }

private:
    Error(const Code errorCode, std::string errorMessage, std::string errorDetail)
        : code(errorCode), message(std::move(errorMessage)), detail(std::move(errorDetail)) {}
};

// The detail an outer module records when it converts an inner module's error
// (rule 5 of docs/architecture/error-handling.md): "<InnerCode>" or
// "<InnerCode>: <inner detail>", after the outer module's own detail if any.
template <CodedError E>
[[nodiscard]] std::string CauseDetail(const E& inner, const std::string_view outerDetail = {}) {
    std::string text{outerDetail};
    if (!text.empty()) text += "; ";
    text += std::string_view(ToString(inner.code));
    if (!inner.detail.empty()) {
        text += ": ";
        text += inner.detail;
    }
    return text;
}

// One line for people: "<CodeName>: <message>", followed by " (<detail>)"
// when there is a detail.
template <CodedError E>
[[nodiscard]] std::string Describe(const E& error) {
    std::string text{std::string_view(ToString(error.code))};
    text += ": ";
    text += error.message;
    if (!error.detail.empty()) {
        text += " (";
        text += error.detail;
        text += ')';
    }
    return text;
}

} // namespace Engine::Base
