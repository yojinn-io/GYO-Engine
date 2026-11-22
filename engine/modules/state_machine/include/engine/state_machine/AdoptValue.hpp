#pragma once

// Taking a value decided elsewhere (net-module declaration docs/architecture/target/v2/net-module-declaration.zh-Hant.md,
// Q8 (b); Design Invariant 6 in docs/architecture/target/v2/state-machine.zh-Hant.md). The network Module replicates a
// state's value and calls an apply function at the client; these two are the state machine's, one per kind of
// request. Both go through a request (DI 1), so the value changes at the next apply of the state, the same frame
// when the network receives in Main/First (the apply stage is after PreUpdate by default).
//
// They take only kernel types: the state machine does not depend on the network Module and the network Module does
// not depend on the state machine; the product joins the two in its own build, for example
//   NetEntry(s).ReplicateResource<StateMachine<Weather>, Weather>("Weather",
//       {DefaultCodec<Weather>(), [](const StateMachine<Weather>& m) { return m.Current(); },
//        &AdoptByRequestSilent<Weather>});

#include "engine/base/Assert.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/state_machine/StateMachine.hpp"

#include <optional>

namespace GYO::Engine::StateMachine {

namespace Detail {

// CCR-8: nothing is written when the state already goes to value, so a value sent again on every snapshot does not
// mark the request Resource, nor make the apply run anything. "Already goes to": the pending request's value and
// kind when there is one, else the current value.
template <State S>
Kernel::SystemResult Adopt(Kernel::ExclusiveWorld<>& world, const S value, const RequestKind kind) {
    auto current = world.template Read<StateMachine<S>>([](const StateMachine<S>& m) { return m.Current(); });
    GYO_ASSERT(current && current->has_value() && "AdoptByRequest: the state machine is not registered in this World");
    auto same = world.template Read<StateMachineRequest<S>>([&](const StateMachineRequest<S>& r) {
        return r.Pending() ? (*r.Pending() == value && r.PendingKind() == kind) : (**current == value);
    });
    GYO_ASSERT(same && same->has_value());
    if (**same) return {};
    auto written = world.template Modify<StateMachineRequest<S>>([value, kind](StateMachineRequest<S>& r) {
        if (kind == RequestKind::Silent) {
            r.RequestSilent(value);
        } else {
            r.Request(value);
        }
    });
    GYO_ASSERT(written && *written);
    return {};
}

} // namespace Detail

// Takes value as a Request: at the next apply the exits and enters run (DI 3, 5).
template <State S>
Kernel::SystemResult AdoptByRequest(Kernel::ExclusiveWorld<>& world, const S& value) {
    return Detail::Adopt<S>(world, value, RequestKind::Normal);
}

// Takes value as a RequestSilent: at the next apply the value changes without exits and enters (DI 6).
template <State S>
Kernel::SystemResult AdoptByRequestSilent(Kernel::ExclusiveWorld<>& world, const S& value) {
    return Detail::Adopt<S>(world, value, RequestKind::Silent);
}

} // namespace GYO::Engine::StateMachine
