#pragma once

// The names the Module builds from a state's traits (Design Invariant 10): fixed, so both ends of a connection
// build the same ones (the authority Module's consistency fingerprint uses Resource and System names).

#include "engine/kernel/KernelError.hpp"
#include "engine/state_machine/Traits.hpp"

#include <string>
#include <string_view>

namespace GYO::Engine::StateMachine {

namespace Detail {
[[nodiscard]] std::string Joined(std::string_view a, std::string_view b, std::string_view c = {});
// A requested or initial value that is not in the names table (rule SM-E1): the apply System's Err.
[[nodiscard]] Kernel::KernelError ValueNotInTable(std::string_view state, long long value);
} // namespace Detail

// "StateMachine.<state>", "StateMachineRequest.<state>".
template <State S>
[[nodiscard]] std::string ResourceName() {
    return Detail::Joined("StateMachine", StateName<S>());
}
template <State S>
[[nodiscard]] std::string RequestName() {
    return Detail::Joined("StateMachineRequest", StateName<S>());
}
// "ApplyStateMachine.<root>" and the stage "<root>.Apply".
template <RootState S>
[[nodiscard]] std::string ApplySystemName() {
    return Detail::Joined("ApplyStateMachine", StateName<S>());
}
template <RootState S>
[[nodiscard]] std::string ApplyStageName() {
    return Detail::Joined(StateName<S>(), "Apply");
}
// "<state>.Enter.<value>", "<state>.Exit.<value>": one stage each, EnterExitStage.
template <State S>
[[nodiscard]] std::string EnterScheduleName(S value) {
    return Detail::Joined(StateName<S>(), "Enter", NameOf(value).value_or("?"));
}
template <State S>
[[nodiscard]] std::string ExitScheduleName(S value) {
    return Detail::Joined(StateName<S>(), "Exit", NameOf(value).value_or("?"));
}
inline constexpr std::string_view kEnterExitStage = "Update";

} // namespace GYO::Engine::StateMachine
