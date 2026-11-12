#pragma once

#include "kernel_spike/Access.hpp"
#include "kernel_spike/Assert.hpp"
#include "kernel_spike/Descriptor.hpp"
#include "kernel_spike/Params.hpp"
#include "kernel_spike/Registry.hpp"
#include "kernel_spike/TypeList.hpp"
#include "kernel_spike/World.hpp"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// A System is a function plus the descriptor derived from its parameter
// types. There is no way to build a System from a hand-written descriptor.
//
// Two ways to call Systems: System::Run (one call, the original spike
// shape: every access-check violation fails KERNEL_SPIKE_ASSERT; no page
// guard), and Run(world, {systems...}) (the Executor's shape for the page
// guard batch: the page guard is active for the whole Run, and page-guard
// faults and second-layer differences come back as Err, a Violation list).

namespace KernelSpike {

class System;

namespace Detail {

template <class F>
constexpr bool CheckSystemFunction() {
    if constexpr (!FunctionArgs<F>::deducible) {
        return false;
    } else {
        return std::is_void_v<typename FunctionArgs<F>::Return> && AccessOf<F>::allParams && !HasAliasing<F>;
    }
}

template <class F>
inline constexpr bool IsSystemFunction = CheckSystemFunction<F>();

template <class F>
constexpr void RequireSystemFunction() {
    static_assert(FunctionArgs<F>::deducible,
                  "a System is a function, a member function, or a lambda/function object with one "
                  "non-template operator()");
    if constexpr (FunctionArgs<F>::deducible) {
        static_assert(std::is_void_v<typename FunctionArgs<F>::Return>, "a System returns void");
        static_assert(AccessOf<F>::allParams,
                      "every System parameter must be Read, Write, Res, ResMut, EventReader, EventWriter or "
                      "Deferred, taken by value");
        static_assert(!HasAliasing<F>, "a System may not write a key twice, or both read and write it");
    }
}

// Argument evaluation order is unspecified, so handle construction must not
// depend on it: constructing a handle only resolves a pointer.
template <class F, class... A>
void Invoke(F& function, World& world, TypeList<A...>) {
    function(ParamFactory::Make<A>(world)...);
}

template <class C, class M, class... A>
void InvokeMember(C& object, M member, World& world, TypeList<A...>) {
    (object.*member)(ParamFactory::Make<A>(world)...);
}

struct SystemFactory;

} // namespace Detail

// The descriptor of F, derived from F's type alone.
template <class F>
[[nodiscard]] SystemDescriptor Describe(const Registry& registry, std::string name) {
    Detail::RequireSystemFunction<F>();
    return SystemDescriptor{
        std::move(name),
        ResolveKeys(registry, typename AccessOf<F>::Reads{}),
        ResolveKeys(registry, typename AccessOf<F>::Writes{}),
    };
}

class System final {
public:
    [[nodiscard]] const SystemDescriptor& Descriptor() const noexcept { return descriptor_; }

    // Runs the function once with fresh handles. With access checks on, the
    // World verifies its storage before and after (World.hpp); a violation
    // fails KERNEL_SPIKE_ASSERT. The page guard is not active outside a Run.
    void Run(World& world) const { Step(world, nullptr); }

private:
    friend struct Detail::SystemFactory;
    friend class Executor;

    // One System call. Without sink, as Run above. With sink (inside a Run),
    // the second layer's differences and the page-guard faults of this call
    // are appended to it instead of failing KERNEL_SPIKE_ASSERT; the first
    // layer (access-time check) still fails KERNEL_SPIKE_ASSERT, so it keeps
    // stopping the System at the access (the original spike's tests rely on
    // that).
    void Step(World& world, std::vector<Violation>* sink) const {
        KERNEL_SPIKE_ASSERT(Detail::runningSystem == nullptr, "a System run cannot start inside another System run");
#if KERNEL_SPIKE_ACCESS_CHECKS
        if (!world.VerifyBetweenSystems(sink)) {
            return;  // attributed to the previous segment; this System is not called (spec E.2 step 0)
        }
#endif
        Detail::RunContext context;
        {
            const Detail::RunningScope scope(descriptor_, world, context);
            world.Guard(World::GuardStep::BeforeSystem, &descriptor_, &context);
            invoke_(world);
            world.Guard(World::GuardStep::AfterSystem, &descriptor_, &context);
        }
        const std::size_t faults = context.faultCount.load(std::memory_order_acquire);
        const std::size_t dropped = context.faultsDropped.load(std::memory_order_relaxed);
        KERNEL_SPIKE_ASSERT(sink != nullptr || (faults == 0 && dropped == 0), "page-guard fault outside a Run");
        for (std::size_t index = 0; index < faults; ++index) {
            const Detail::PageFault& fault = context.faults[index];
            sink->push_back(Violation{Violation::Source::PageGuard, descriptor_.name,
                                      AccessKey{AccessKind::Component, fault.column}, fault.address});
        }
        if (dropped != 0) {
            sink->push_back(Violation{Violation::Source::PageGuardDropped, descriptor_.name,
                                      AccessKey{AccessKind::Component, 0}, 0, dropped});
        }
#if KERNEL_SPIKE_ACCESS_CHECKS
        world.VerifyAfterSystem(descriptor_, sink);
#endif
    }

    System(SystemDescriptor descriptor, std::function<void(World&)> invoke)
        : descriptor_(std::move(descriptor)), invoke_(std::move(invoke)) {}

    SystemDescriptor descriptor_;
    std::function<void(World&)> invoke_;
};

namespace Detail {

struct SystemFactory {
    template <class F>
    static System Make(const Registry& registry, std::string name, Zone zone, F function) {
        SystemDescriptor descriptor = Describe<F>(registry, std::move(name));
        descriptor.zone = zone;
        return System(std::move(descriptor), [function = std::move(function)](World& world) mutable {
            Invoke(function, world, typename FunctionArgs<F>::Args{});
        });
    }

    template <class C, class M>
    static System MakeMember(const Registry& registry, std::string name, C& object, M member) {
        SystemDescriptor descriptor = Describe<M>(registry, std::move(name));
        return System(std::move(descriptor), [&object, member](World& world) {
            InvokeMember(object, member, world, typename FunctionArgs<M>::Args{});
        });
    }
};

} // namespace Detail

// A free function, a non-generic lambda, or a function object. Zone C.
template <class F>
[[nodiscard]] System MakeSystem(const Registry& registry, std::string name, F function) {
    return Detail::SystemFactory::Make(registry, std::move(name), Zone::C, std::move(function));
}

// The same, in the given zone (the page guard guards (c) Systems only).
template <class F>
[[nodiscard]] System MakeSystem(const Registry& registry, std::string name, Zone zone, F function) {
    return Detail::SystemFactory::Make(registry, std::move(name), zone, std::move(function));
}

// A member function called on object, which must outlive the System.
template <class C, class M>
    requires std::is_member_function_pointer_v<M>
[[nodiscard]] System MakeSystem(const Registry& registry, std::string name, C& object, M member) {
    return Detail::SystemFactory::MakeMember(registry, std::move(name), object, member);
}

// The result of a Run: Ok when violations is empty, otherwise Err with every
// violation of the System that ended the Run (a plain list in place of spec
// B-9's Siblings).
struct RunResult {
    std::vector<Violation> violations;

    [[nodiscard]] bool IsOk() const noexcept { return violations.empty(); }
};

// The spike's Executor: runs Systems in order on one World. The page guard
// (WorldOptions::pageGuard) is active from entry to exit; Ready (outside a
// Run) is not guarded. The first System that returns Err ends the Run (the
// spec's Interrupted; the spike has no such state, so the World can Run
// again).
class Executor final {
public:
    [[nodiscard]] static RunResult Run(World& world, std::initializer_list<const System*> systems) {
        KERNEL_SPIKE_ASSERT(Detail::runningSystem == nullptr, "a Run cannot start inside a System run");
        RunResult result;
        {
            // ExitRun on every way out, including a KERNEL_SPIKE_ASSERT that a
            // test handler turns into a throw.
            const ExitScope exit(world);
            world.Guard(World::GuardStep::EnterRun, nullptr, nullptr);
            for (const System* system : systems) {
                system->Step(world, &result.violations);
                if (!result.violations.empty()) {
                    break;
                }
            }
        }
        for (const Violation& violation : result.violations) {
            Print(world, violation);
        }
        return result;
    }

private:
    class ExitScope final {
    public:
        explicit ExitScope(World& world) noexcept : world_(world) {}
        ~ExitScope() { world_.Guard(World::GuardStep::ExitRun, nullptr, nullptr); }
        ExitScope(const ExitScope&) = delete;
        ExitScope& operator=(const ExitScope&) = delete;

    private:
        World& world_;
    };

    static void Print(const World& world, const Violation& violation) {
        const char* kind = World::KindName(violation.key.kind);
        switch (violation.source) {
        case Violation::Source::PageGuard:
            std::fprintf(stderr, "kernel spike page guard: System '%s' wrote %s '%s' (id %u) at 0x%llx without declaring a write\n",
                         violation.system.c_str(), kind, world.NameOf(violation.key), violation.key.id,
                         static_cast<unsigned long long>(violation.address));
            break;
        case Violation::Source::PageGuardDropped:
            std::fprintf(stderr, "kernel spike page guard: System '%s' faulted on %zu more columns than it could record\n",
                         violation.system.c_str(), violation.dropped);
            break;
        case Violation::Source::AfterSystem:
            std::fprintf(stderr, "kernel spike access check: System '%s' changed %s '%s' without declaring a write\n",
                         violation.system.c_str(), kind, world.NameOf(violation.key));
            break;
        case Violation::Source::BeforeSystem:
            std::fprintf(stderr, "kernel spike access check: %s '%s' changed outside any System without the World API\n",
                         kind, world.NameOf(violation.key));
            break;
        }
        std::fflush(stderr);
    }
};

[[nodiscard]] inline RunResult Run(World& world, std::initializer_list<const System*> systems) {
    return Executor::Run(world, systems);
}

} // namespace KernelSpike
