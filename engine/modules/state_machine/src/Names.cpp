#include "engine/state_machine/Names.hpp"

#include <string>

namespace GYO::Engine::StateMachine::Detail {

std::string Joined(std::string_view a, std::string_view b, std::string_view c) {
    std::string joined;
    joined.reserve(a.size() + b.size() + c.size() + 2);
    joined.append(a).append(".").append(b);
    if (!c.empty()) joined.append(".").append(c);
    return joined;
}

Kernel::KernelError ValueNotInTable(std::string_view state, long long value) {
    Kernel::KernelError error(Kernel::KernelErrorCode::SystemFailed,
                              "state " + std::string(state) + ": value " + std::to_string(value) +
                                  " is not in its names table (StateMachineTraits::values)");
    error.rule = "SM-E1";
    return error;
}

} // namespace GYO::Engine::StateMachine::Detail
