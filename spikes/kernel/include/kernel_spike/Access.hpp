#pragma once

#include "kernel_spike/Descriptor.hpp"
#include "kernel_spike/Params.hpp"
#include "kernel_spike/Registry.hpp"
#include "kernel_spike/TypeList.hpp"

#include <algorithm>
#include <type_traits>
#include <vector>

// Derives a System's access set from its function type. The parameter list
// is the only declaration: AccessOf<F>::Reads and ::Writes are computed from
// it at compile time, and Describe() turns them into ids at run time.

namespace KernelSpike {

// ---- Parameter list of a callable type

template <class F, class = void>
struct FunctionArgs {
    // A generic lambda, an overloaded operator(), or not callable at all: the
    // parameter list is not a single type list, so no access set exists.
    static constexpr bool deducible = false;
};

template <class R, class... A>
struct FunctionArgs<R (*)(A...)> {
    static constexpr bool deducible = true;
    using Return = R;
    using Args = TypeList<A...>;
};

template <class R, class... A>
struct FunctionArgs<R (*)(A...) noexcept> : FunctionArgs<R (*)(A...)> {};

template <class R, class C, class... A>
struct FunctionArgs<R (C::*)(A...)> : FunctionArgs<R (*)(A...)> {};

template <class R, class C, class... A>
struct FunctionArgs<R (C::*)(A...) const> : FunctionArgs<R (*)(A...)> {};

template <class R, class C, class... A>
struct FunctionArgs<R (C::*)(A...) noexcept> : FunctionArgs<R (*)(A...)> {};

template <class R, class C, class... A>
struct FunctionArgs<R (C::*)(A...) const noexcept> : FunctionArgs<R (*)(A...)> {};

// Lambdas and function objects with exactly one non-template operator().
template <class F>
struct FunctionArgs<F, std::void_t<decltype(&F::operator())>> : FunctionArgs<decltype(&F::operator())> {};

// ---- Access set

template <class ArgList>
struct AccessFromArgs;

template <class... A>
struct AccessFromArgs<TypeList<A...>> {
    // Every parameter must be one of the seven handles, by value.
    static constexpr bool allParams = (ParamAccess<A>::valid && ...);
    using Reads = Concat<typename ParamAccess<A>::Reads...>;
    using Writes = Concat<typename ParamAccess<A>::Writes...>;
};

template <class F>
using AccessOf = AccessFromArgs<typename FunctionArgs<F>::Args>;

// Aliasing: a key written twice, or both read and written, by one System.
template <class Reads, class Writes>
struct AliasingT;

template <class Reads, class... W>
struct AliasingT<Reads, TypeList<W...>>
    : std::bool_constant<((CountOf<TypeList<W...>, W>::value > 1 || Contains<Reads, W>) || ...)> {};

template <class F>
inline constexpr bool HasAliasing = AliasingT<typename AccessOf<F>::Reads, typename AccessOf<F>::Writes>::value;

// ---- Ids

template <class A>
[[nodiscard]] AccessKey ResolveKey(const Registry& registry) {
    using T = typename A::Type;
    if constexpr (A::kind == AccessKind::Component) {
        return AccessKey{AccessKind::Component, registry.ComponentIdOf<T>()};
    } else if constexpr (A::kind == AccessKind::Resource) {
        return AccessKey{AccessKind::Resource, registry.ResourceIdOf<T>()};
    } else if constexpr (A::kind == AccessKind::Event) {
        return AccessKey{AccessKind::Event, registry.EventIdOf<T>()};
    } else {
        return AccessKey{AccessKind::Deferred, 0};
    }
}

template <class... As>
[[nodiscard]] std::vector<AccessKey> ResolveKeys(const Registry& registry, TypeList<As...>) {
    std::vector<AccessKey> keys{ResolveKey<As>(registry)...};
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

} // namespace KernelSpike
