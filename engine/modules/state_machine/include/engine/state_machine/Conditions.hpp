#pragma once

// Run conditions on a state's value (state machine declaration section 5.2 row 7): the value is a template
// argument, since a condition captures nothing. Named "InState.<state>.<value>" and "NotInState.<state>.<value>"
// (a condition's name is in the authority Module's consistency fingerprint). Both are false while the state is not
// entered yet or absent (DI 4, DI 5).

#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/state_machine/Names.hpp"
#include "engine/state_machine/StateMachine.hpp"

namespace GYO::Engine::StateMachine {

namespace Detail {
template <class S, S V>
bool IsIn(Kernel::Res<StateMachine<S>> machine) {
    return machine->Status() == StateStatus::Active && *machine->Current() == V;
}
template <class S, S V>
bool IsNotIn(Kernel::Res<StateMachine<S>> machine) {
    return machine->Status() == StateStatus::Active && *machine->Current() != V;
}
} // namespace Detail

template <State S, S V>
[[nodiscard]] Kernel::ConditionRef InState() {
    static_assert(NameOf(V).has_value(), "InState: the value is not in the state's names table");
    return Kernel::Condition<&Detail::IsIn<S, V>>(Detail::Joined("InState", StateName<S>(), *NameOf(V)));
}

template <State S, S V>
[[nodiscard]] Kernel::ConditionRef NotInState() {
    static_assert(NameOf(V).has_value(), "NotInState: the value is not in the state's names table");
    return Kernel::Condition<&Detail::IsNotIn<S, V>>(Detail::Joined("NotInState", StateName<S>(), *NameOf(V)));
}

} // namespace GYO::Engine::StateMachine
