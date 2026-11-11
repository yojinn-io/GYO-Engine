#pragma once

#include <cstddef>
#include <type_traits>

// Compile-time sets of types. The access set of a System is a TypeList of
// Access<Kind, T> (Descriptor.hpp); tests compare it with a hand-written
// expectation through SameSet.

namespace KernelSpike {

template <class... Ts>
struct TypeList {
    static constexpr std::size_t size = sizeof...(Ts);
};

template <class List, class T>
struct CountOf;

template <class... Ts, class T>
struct CountOf<TypeList<Ts...>, T>
    : std::integral_constant<std::size_t,
                             (std::size_t{0} + ... + (std::is_same_v<Ts, T> ? std::size_t{1} : std::size_t{0}))> {};

template <class List, class T>
inline constexpr bool Contains = CountOf<List, T>::value > 0;

template <class... Lists>
struct ConcatT;

template <>
struct ConcatT<> {
    using type = TypeList<>;
};

template <class... Ts>
struct ConcatT<TypeList<Ts...>> {
    using type = TypeList<Ts...>;
};

template <class... As, class... Bs, class... Rest>
struct ConcatT<TypeList<As...>, TypeList<Bs...>, Rest...> : ConcatT<TypeList<As..., Bs...>, Rest...> {};

template <class... Lists>
using Concat = typename ConcatT<Lists...>::type;

template <class A, class B>
struct IncludesAllT;

template <class... As, class B>
struct IncludesAllT<TypeList<As...>, B> : std::bool_constant<(Contains<B, As> && ...)> {};

template <class List>
struct HasDuplicatesT;

template <class... Ts>
struct HasDuplicatesT<TypeList<Ts...>>
    : std::bool_constant<((CountOf<TypeList<Ts...>, Ts>::value > 1) || ...)> {};

template <class List>
inline constexpr bool HasDuplicates = HasDuplicatesT<List>::value;

// Set equality: order and repetition do not matter.
template <class A, class B>
inline constexpr bool SameSet = IncludesAllT<A, B>::value && IncludesAllT<B, A>::value;

} // namespace KernelSpike
