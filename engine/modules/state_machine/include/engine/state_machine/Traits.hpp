#pragma once

// The names table of a state (state machine declaration docs/architecture/target/v2/state-machine-declaration.zh-Hant.md,
// section 5.1 and 5.2 row 2; the user's v8-arch-gyo2-state-machine-names-by-enum-and-traits): the author writes an
// ordinary enum and specializes StateMachineTraits for it once, on the product side. No macro.
//
//     enum class Flow : std::uint8_t { MainMenu, InLevel };
//     template <> struct GYO::Engine::StateMachine::StateMachineTraits<Flow> {
//         static constexpr std::string_view name = "Flow";
//         static constexpr std::array values{ValueName<Flow>{Flow::MainMenu, "MainMenu"},
//                                            ValueName<Flow>{Flow::InLevel, "InLevel"}};
//         static constexpr Flow initial = Flow::MainMenu;
//         using SubStates = GYO::Kernel::TypeList<Pause>;  // optional: the children, in registration order
//     };
//     template <> struct GYO::Engine::StateMachine::StateMachineTraits<Pause> {
//         ... name, values, initial ...
//         using Parent = Flow;                               // a child: its parent
//         static constexpr Flow parentValue = Flow::InLevel; // and the parent value it exists under
//         static constexpr bool keepOnParentReenter = false; // optional (default false)
//     };
//
// One level only: a child has no SubStates and is not registered as a tree of its own. The constraints below make
// a mismatched table fail to compile (they are the type's design; no test checks them, v8-arch-gyo2-state-machine
// revision 8, section 1 item 12).

#include "engine/kernel/TypeList.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>

namespace GYO::Engine::StateMachine {

// One value of a state and its name (used in schedule and condition names, so both ends of a connection build
// the same names; DI 10).
template <class S>
struct ValueName {
    S value;
    std::string_view name;
};

// Specialized by the author for each state enum.
template <class S>
struct StateMachineTraits;

namespace Detail {

template <class S>
concept HasTable = std::is_enum_v<S> && requires {
    { StateMachineTraits<S>::name } -> std::convertible_to<std::string_view>;
    { StateMachineTraits<S>::initial } -> std::convertible_to<S>;
    StateMachineTraits<S>::values.size();
    { StateMachineTraits<S>::values[0].value } -> std::convertible_to<S>;
    { StateMachineTraits<S>::values[0].name } -> std::convertible_to<std::string_view>;
};

template <class S>
concept DeclaresParent = requires {
    typename StateMachineTraits<S>::Parent;
    StateMachineTraits<S>::parentValue;
};

template <class S>
concept DeclaresSubStates = requires { typename StateMachineTraits<S>::SubStates; };

template <class S>
struct SubStatesOfT {
    using type = Kernel::TypeList<>;
};
template <class S>
    requires DeclaresSubStates<S>
struct SubStatesOfT<S> {
    using type = typename StateMachineTraits<S>::SubStates;
};

template <class List>
struct IsTypeList : std::false_type {};
template <class... Ts>
struct IsTypeList<Kernel::TypeList<Ts...>> : std::true_type {};

} // namespace Detail

// The children of S in registration order (empty for a child or a root without children).
template <class S>
using SubStatesOf = typename Detail::SubStatesOfT<S>::type;

// A child: its traits name a parent that lists it, under one of the parent's values; it has no children itself.
template <class S>
concept SubState = Detail::HasTable<S> && Detail::DeclaresParent<S> && !Detail::DeclaresSubStates<S> &&
                   Detail::HasTable<typename StateMachineTraits<S>::Parent> &&
                   !Detail::DeclaresParent<typename StateMachineTraits<S>::Parent> &&
                   std::same_as<std::remove_cv_t<decltype(StateMachineTraits<S>::parentValue)>,
                                typename StateMachineTraits<S>::Parent> &&
                   Kernel::Contains<SubStatesOf<typename StateMachineTraits<S>::Parent>, S>;

namespace Detail {
template <class Root, class List>
struct ChildrenFit : std::false_type {};
template <class Root, class... Children>
struct ChildrenFit<Root, Kernel::TypeList<Children...>>
    : std::bool_constant<((SubState<Children> && std::same_as<typename StateMachineTraits<Children>::Parent, Root>) &&
                          ...)> {};
} // namespace Detail

// A root (the tree registered with RegisterStateMachine): no parent; every listed child names it as its parent.
template <class S>
concept RootState = Detail::HasTable<S> && !Detail::DeclaresParent<S> && Detail::IsTypeList<SubStatesOf<S>>::value &&
                    Detail::ChildrenFit<S, SubStatesOf<S>>::value;

// A state a condition or a resource can name: a root or a child.
template <class S>
concept State = RootState<S> || SubState<S>;

// The value's name; nullopt for a value not in the table (an enum value cast from an integer).
template <State S>
[[nodiscard]] constexpr std::optional<std::string_view> NameOf(S value) {
    for (const auto& entry : StateMachineTraits<S>::values) {
        if (entry.value == value) return entry.name;
    }
    return std::nullopt;
}

template <State S>
[[nodiscard]] constexpr std::string_view StateName() {
    return StateMachineTraits<S>::name;
}

// Whether a child keeps its value when its parent is entered again with the same value (default: reset).
template <SubState S>
[[nodiscard]] constexpr bool KeepsOnParentReenter() {
    if constexpr (requires { StateMachineTraits<S>::keepOnParentReenter; }) {
        return StateMachineTraits<S>::keepOnParentReenter;
    } else {
        return false;
    }
}

} // namespace GYO::Engine::StateMachine
