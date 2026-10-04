#pragma once

#include <type_traits>
#include <utility>
#include <variant>

#include "engine/base/Assert.hpp"

// Result<T, E> reports a Runtime Error: it holds either the value T or the
// error E. A Programmer Error is a GYO_ASSERT instead; see
// docs/architecture/error-handling.md.
//
//     Base::Result<Path, IoError> Parse(std::string_view raw) {
//         auto normalized = Normalize(raw);
//         if (!normalized) return Base::Err(std::move(normalized).error());
//         return Path::FromNormalized(std::move(*normalized));
//     }
//     Base::Result<void, IoError> Close() { ...; return {}; }
//
// The interface is a subset of C++23 std::expected (Err for std::unexpected),
// so a later migration is mechanical. Unlike std::expected, reading the side
// that is not held is a Programmer Error rather than an exception.

namespace Engine::Base {

// The failure side of a Result, like std::unexpected: `return Base::Err(e);`.
template <class E>
class [[nodiscard]] Err final {
    static_assert(std::is_object_v<E> && !std::is_array_v<E> && !std::is_const_v<E> &&
                      !std::is_volatile_v<E>,
                  "Err<E> holds a plain object type");

public:
    template <class G = E>
        requires(!std::is_same_v<std::remove_cvref_t<G>, Err> && std::is_constructible_v<E, G>)
    constexpr explicit Err(G&& error) : error_(std::forward<G>(error)) {}

    [[nodiscard]] constexpr E& error() & noexcept { return error_; }
    [[nodiscard]] constexpr const E& error() const& noexcept { return error_; }
    [[nodiscard]] constexpr E&& error() && noexcept { return std::move(error_); }

private:
    E error_;
};

template <class G>
Err(G) -> Err<G>;

namespace Detail {

template <class>
inline constexpr bool isErr = false;
template <class G>
inline constexpr bool isErr<Err<G>> = true;

} // namespace Detail

template <class T, class E>
class [[nodiscard]] Result final {
    static_assert(!std::is_reference_v<T> && !std::is_reference_v<E>,
                  "Result holds objects; use pointers or std::reference_wrapper for references");
    static_assert(!std::is_same_v<std::remove_cv_t<T>, std::remove_cv_t<E>>,
                  "Result<T, E> with T == E cannot tell success from failure");

    // A success value: anything T is constructible from, except another
    // Result, an Err, or an E itself (so a forgotten Err does not compile).
    template <class U>
    static constexpr bool isValueArgument =
        !std::is_same_v<std::remove_cvref_t<U>, Result> && !Detail::isErr<std::remove_cvref_t<U>> &&
        !std::is_same_v<std::remove_cvref_t<U>, E> && std::is_constructible_v<T, U>;

public:
    using value_type = T;
    using error_type = E;

    // Success. There is no default constructor, so `return {};` does not
    // silently succeed with a default T; write `return T{...};`.
    template <class U>
        requires isValueArgument<U>
    constexpr explicit(!std::is_convertible_v<U, T>) Result(U&& value)
        : data_(std::in_place_index<0>, std::forward<U>(value)) {}

    // Failure, from an Err whose error converts to E.
    template <class G>
        requires std::is_constructible_v<E, const G&>
    constexpr explicit(!std::is_convertible_v<const G&, E>) Result(const Err<G>& failure)
        : data_(std::in_place_index<1>, failure.error()) {}

    template <class G>
        requires std::is_constructible_v<E, G>
    constexpr explicit(!std::is_convertible_v<G, E>) Result(Err<G>&& failure)
        : data_(std::in_place_index<1>, std::move(failure).error()) {}

    [[nodiscard]] constexpr bool has_value() const noexcept { return data_.index() == 0; }
    // True on success, also for Result<bool, E>: test the value with value().
    constexpr explicit operator bool() const noexcept { return has_value(); }

    // Reading the side that is not held is a Programmer Error.
    [[nodiscard]] constexpr T& value() & { GYO_ASSERT(has_value()); return *std::get_if<0>(&data_); }
    [[nodiscard]] constexpr const T& value() const& { GYO_ASSERT(has_value()); return *std::get_if<0>(&data_); }
    [[nodiscard]] constexpr T&& value() && { GYO_ASSERT(has_value()); return std::move(*std::get_if<0>(&data_)); }

    [[nodiscard]] constexpr T& operator*() & { return value(); }
    [[nodiscard]] constexpr const T& operator*() const& { return value(); }
    [[nodiscard]] constexpr T&& operator*() && { return std::move(*this).value(); }
    [[nodiscard]] constexpr T* operator->() { return &value(); }
    [[nodiscard]] constexpr const T* operator->() const { return &value(); }

    [[nodiscard]] constexpr E& error() & { GYO_ASSERT(!has_value()); return *std::get_if<1>(&data_); }
    [[nodiscard]] constexpr const E& error() const& { GYO_ASSERT(!has_value()); return *std::get_if<1>(&data_); }
    [[nodiscard]] constexpr E&& error() && { GYO_ASSERT(!has_value()); return std::move(*std::get_if<1>(&data_)); }

private:
    std::variant<T, E> data_;
};

template <class E>
class [[nodiscard]] Result<void, E> final {
    static_assert(!std::is_reference_v<E>, "Result holds objects");

public:
    using value_type = void;
    using error_type = E;

    // Success: `return {};`.
    constexpr Result() noexcept : data_(std::in_place_index<0>) {}

    template <class G>
        requires std::is_constructible_v<E, const G&>
    constexpr explicit(!std::is_convertible_v<const G&, E>) Result(const Err<G>& failure)
        : data_(std::in_place_index<1>, failure.error()) {}

    template <class G>
        requires std::is_constructible_v<E, G>
    constexpr explicit(!std::is_convertible_v<G, E>) Result(Err<G>&& failure)
        : data_(std::in_place_index<1>, std::move(failure).error()) {}

    [[nodiscard]] constexpr bool has_value() const noexcept { return data_.index() == 0; }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    // Reading the side that is not held is a Programmer Error.
    constexpr void value() const { GYO_ASSERT(has_value()); }

    [[nodiscard]] constexpr E& error() & { GYO_ASSERT(!has_value()); return *std::get_if<1>(&data_); }
    [[nodiscard]] constexpr const E& error() const& { GYO_ASSERT(!has_value()); return *std::get_if<1>(&data_); }
    [[nodiscard]] constexpr E&& error() && { GYO_ASSERT(!has_value()); return std::move(*std::get_if<1>(&data_)); }

private:
    std::variant<std::monostate, E> data_;
};

} // namespace Engine::Base
