#pragma once

#include <span>

namespace Engine::Runtime {

// A caller-neutral runtime boundary. The game supplies its concrete snapshot,
// legal command, and event types; GYO standardizes how external controllers
// query, request, and observe without introducing an untyped event payload.
// Query() and Events() are borrowed current-frame views. A caller must copy any
// data it needs to retain before the runtime advances again.
template <class Snapshot, class Command, class Event>
class IRuntimePort {
public:
    virtual ~IRuntimePort() = default;

    [[nodiscard]] virtual const Snapshot& Query() const noexcept = 0;
    virtual void Submit(Command command) = 0;
    [[nodiscard]] virtual std::span<const Event> Events() const noexcept = 0;
};

} // namespace Engine::Runtime
