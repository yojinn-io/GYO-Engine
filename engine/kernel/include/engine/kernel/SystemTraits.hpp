#pragma once

#include "engine/base/Result.hpp"
#include "engine/kernel/Access.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/TypeList.hpp"

#include <cstddef>
#include <type_traits>

// Compile-time checks of a System's callable and signature (target/kernel
// DI-KS 1; spec S-1 to S-3, S-5, S-10, B.4 points 1 to 5).
//
// A System is a non-member function passed as a non-type template argument
// (AddSystem<&Fn>) or a captureless lambda passed as a type (AddSystem<F>).
// Rejected, each with its own reason so a test can tell them apart (spec J-10,
// AGENTS 14.2): a lambda with a capture, a member function, a generic lambda or
// an overloaded operator(), a return type other than Result<void, KernelError>,
// a parameter without ParamTraits, a signature that reads and writes one key
// or writes it twice, ExclusiveWorld beside other parameters, more than 16
// parameters. A run condition returns bool and has read-only parameters (S-5).

namespace GYO::Kernel {

using SystemResult = Engine::Base::Result<void, KernelError>;

// Spec B.4 point 5, B.6 risk 3.
inline constexpr std::size_t kMaxParameters = 16;

enum class Rejection : unsigned char {
    None,
    NotCallable,
    MemberFunction,
    GenericOrOverloaded,
    CapturingCallable,
    ReturnType,
    Parameter,
    TooManyParameters,
    SelfConflict,
    ExclusiveWorldNotAlone,
    ConditionReturn,
    ConditionNotReadOnly,
};

namespace Detail {

template <class F>
struct Signature {
    static constexpr bool function = false;
    static constexpr bool member = false;
};
template <class R, class... A>
struct Signature<R (*)(A...)> {
    static constexpr bool function = true;
    static constexpr bool member = false;
    using Return = R;
    using Params = TypeList<A...>;
    using Pointer = R (*)(A...);
};
template <class R, class... A>
struct Signature<R (*)(A...) noexcept> : Signature<R (*)(A...)> {};
template <class R, class C, class... A>
struct Signature<R (C::*)(A...)> {
    static constexpr bool function = false;
    static constexpr bool member = true;
    using Return = R;
    using Params = TypeList<A...>;
    using Pointer = R (*)(A...);
};
template <class R, class C, class... A>
struct Signature<R (C::*)(A...) const> : Signature<R (C::*)(A...)> {};
template <class R, class C, class... A>
struct Signature<R (C::*)(A...) noexcept> : Signature<R (C::*)(A...)> {};
template <class R, class C, class... A>
struct Signature<R (C::*)(A...) const noexcept> : Signature<R (C::*)(A...)> {};

template <class F>
concept SingleCallOperator = std::is_class_v<F> && requires { &F::operator(); };

// The parameter list's derived facts.
template <class ParamList>
struct ParamFacts;

template <class... P>
struct ParamFacts<TypeList<P...>> {
    static constexpr bool allValid = (ParamTraits<P>::valid && ...);
    static constexpr std::size_t count = sizeof...(P);
};

template <class... P>
    requires((ParamTraits<P>::valid && ...))
struct ValidParamFacts {
    using Contribution = Concat<typename ParamTraits<P>::Contribution...>;
    static constexpr bool readOnly = (ParamTraits<P>::readOnly && ...);
    static constexpr std::size_t soleCount = (std::size_t{0} + ... + (ParamTraits<P>::sole ? 1U : 0U));
    static constexpr bool extractOnly = (ParamTraits<P>::extractOnly || ...);
};

// Spec S-3: one key written twice, or both read and written, by one signature.
// Reads may repeat (two queries reading the same column are fine).
template <class Entries>
struct SelfConflictT;

template <class... E>
struct SelfConflictT<TypeList<E...>> {
    using List = TypeList<E...>;
    template <class X>
    struct One : std::false_type {};
    template <class T>
    struct One<Access::ComponentWrite<T>>
        : std::bool_constant<(CountOf<List, Access::ComponentWrite<T>>::value > 1) ||
                             Contains<List, Access::ComponentRead<T>>> {};
    template <class T>
    struct One<Access::ResourceWrite<T>>
        : std::bool_constant<(CountOf<List, Access::ResourceWrite<T>>::value > 1) ||
                             Contains<List, Access::ResourceRead<T>>> {};
    template <class T>
    struct One<Access::EventWrite<T>>
        : std::bool_constant<(CountOf<List, Access::EventWrite<T>>::value > 1) ||
                             Contains<List, Access::EventRead<T>>> {};
    static constexpr bool value = (One<E>::value || ...);
};

template <class ParamList>
constexpr Rejection CheckParams() {
    using Facts = ParamFacts<ParamList>;
    if constexpr (Facts::count > kMaxParameters) {
        return Rejection::TooManyParameters;
    } else if constexpr (!Facts::allValid) {
        return Rejection::Parameter;
    } else {
        return []<class... P>(TypeList<P...>) {
            using Valid = ValidParamFacts<P...>;
            if constexpr (SelfConflictT<typename Valid::Contribution>::value) {
                return Rejection::SelfConflict;
            } else if constexpr (Valid::soleCount > 0 && sizeof...(P) != 1) {
                return Rejection::ExclusiveWorldNotAlone;
            } else {
                return Rejection::None;
            }
        }(ParamList{});
    }
}

template <class Sig>
constexpr Rejection CheckSystemSignature() {
    if constexpr (!std::is_same_v<typename Sig::Return, SystemResult>) {
        return Rejection::ReturnType;
    } else {
        return CheckParams<typename Sig::Params>();
    }
}

template <class Sig>
constexpr Rejection CheckConditionSignature() {
    if constexpr (!std::is_same_v<typename Sig::Return, bool>) {
        return Rejection::ConditionReturn;
    } else {
        constexpr Rejection params = CheckParams<typename Sig::Params>();
        if constexpr (params != Rejection::None) {
            return params;
        } else {
            return []<class... P>(TypeList<P...>) {
                if constexpr (sizeof...(P) == 0 || ValidParamFacts<P...>::readOnly) {
                    return Rejection::None;
                } else {
                    return Rejection::ConditionNotReadOnly;
                }
            }(typename Sig::Params{});
        }
    }
}

template <class F>
constexpr Rejection CheckCallableShape() {
    if constexpr (!std::is_class_v<F>) {
        return Rejection::NotCallable;
    } else if constexpr (!SingleCallOperator<F>) {
        // No operator(), a template operator() (generic lambda) or several.
        return Rejection::GenericOrOverloaded;
    } else if constexpr (!Signature<decltype(&F::operator())>::member) {
        return Rejection::NotCallable;
    } else if constexpr (!std::is_empty_v<F> || !std::is_default_constructible_v<F> ||
                         !std::is_convertible_v<F, typename Signature<decltype(&F::operator())>::Pointer>) {
        // A capture, or a function object with state or a this to reach.
        return Rejection::CapturingCallable;
    } else {
        return Rejection::None;
    }
}

} // namespace Detail

// Why AddSystem<F> (a lambda type) would be rejected; Rejection::None when it is accepted.
template <class F>
constexpr Rejection SystemRejectionOf() {
    constexpr Rejection shape = Detail::CheckCallableShape<F>();
    if constexpr (shape != Rejection::None) {
        return shape;
    } else {
        return Detail::CheckSystemSignature<Detail::Signature<decltype(&F::operator())>>();
    }
}

// Why AddSystem<Fn> (a function pointer or member pointer constant) would be rejected.
template <auto Fn>
constexpr Rejection SystemRejectionOfFunction() {
    using Sig = Detail::Signature<decltype(Fn)>;
    if constexpr (Sig::member) {
        return Rejection::MemberFunction;
    } else if constexpr (!Sig::function) {
        return Rejection::NotCallable;
    } else {
        return Detail::CheckSystemSignature<Sig>();
    }
}

template <class F>
constexpr Rejection ConditionRejectionOf() {
    constexpr Rejection shape = Detail::CheckCallableShape<F>();
    if constexpr (shape != Rejection::None) {
        return shape;
    } else {
        return Detail::CheckConditionSignature<Detail::Signature<decltype(&F::operator())>>();
    }
}

template <auto Fn>
constexpr Rejection ConditionRejectionOfFunction() {
    using Sig = Detail::Signature<decltype(Fn)>;
    if constexpr (Sig::member) {
        return Rejection::MemberFunction;
    } else if constexpr (!Sig::function) {
        return Rejection::NotCallable;
    } else {
        return Detail::CheckConditionSignature<Sig>();
    }
}

namespace Detail {

// One static_assert per reason, each with its own marker: tests/common/kernel's
// try_compile samples look for the marker, so a failure for any other reason is
// not taken as the expected one (spec J-10, migration plan U5).
template <Rejection R>
constexpr void RequireAccepted() {
    static_assert(R != Rejection::NotCallable, "GYO_KERNEL_REJECT_NOT_CALLABLE: a System is a function or a lambda");
    static_assert(R != Rejection::MemberFunction,
                  "GYO_KERNEL_REJECT_MEMBER_FUNCTION: a member function reaches state through this (DI-KS 1)");
    static_assert(R != Rejection::GenericOrOverloaded,
                  "GYO_KERNEL_REJECT_GENERIC: a generic lambda or an overloaded operator() has no single "
                  "parameter list to derive access from (DI-KS 1)");
    static_assert(R != Rejection::CapturingCallable,
                  "GYO_KERNEL_REJECT_CAPTURE: a System callable has no capture and no state (DI-KS 1, spec S-1)");
    static_assert(R != Rejection::ReturnType,
                  "GYO_KERNEL_REJECT_RETURN: a System returns Result<void, KernelError> (DI-KS 2, spec S-2)");
    static_assert(R != Rejection::Parameter,
                  "GYO_KERNEL_REJECT_PARAMETER: every System parameter is a kernel handle with ParamTraits, "
                  "taken by value; not World&, T&, a pointer or int (spec S-2)");
    static_assert(R != Rejection::TooManyParameters, "GYO_KERNEL_REJECT_TOO_MANY: at most 16 parameters (spec B.4)");
    static_assert(R != Rejection::SelfConflict,
                  "GYO_KERNEL_REJECT_SELF_CONFLICT: a signature may not write a key twice or both read and "
                  "write it (spec S-3)");
    static_assert(R != Rejection::ExclusiveWorldNotAlone,
                  "GYO_KERNEL_REJECT_EXCLUSIVE_WORLD: ExclusiveWorld is the only parameter (spec S-10)");
    static_assert(R != Rejection::ConditionReturn, "GYO_KERNEL_REJECT_CONDITION_RETURN: a run condition returns bool");
    static_assert(R != Rejection::ConditionNotReadOnly,
                  "GYO_KERNEL_REJECT_CONDITION_WRITE: a run condition has read-only parameters (spec S-5)");
}

using ResolveFn = void (*)(AccessResolver&);

template <class... P>
void ResolveParams(AccessResolver& resolver) {
    std::size_t position = 0;
    ((resolver.Parameter(position++), ResolveEntries(resolver, typename ParamTraits<P>::Contribution{})), ...);
}

template <class Sig>
struct ResolverOf;
template <class R, class... P>
struct ResolverOf<R (*)(P...)> {
    static constexpr ResolveFn value = &ResolveParams<P...>;
    static constexpr bool extractOnly = (ParamTraits<P>::extractOnly || ...);
};

} // namespace Detail

} // namespace GYO::Kernel
