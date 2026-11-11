#pragma once

#include "kernel_spike/Access.hpp"
#include "kernel_spike/Assert.hpp"
#include "kernel_spike/Descriptor.hpp"
#include "kernel_spike/Params.hpp"
#include "kernel_spike/Registry.hpp"
#include "kernel_spike/TypeList.hpp"
#include "kernel_spike/World.hpp"

#include <functional>
#include <string>
#include <type_traits>
#include <utility>

// A System is a function plus the descriptor derived from its parameter
// types. There is no way to build a System from a hand-written descriptor.

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
    // World verifies its storage before and after (World.hpp).
    void Run(World& world) const {
        KERNEL_SPIKE_ASSERT(Detail::runningSystem == nullptr, "a System run cannot start inside another System run");
#if KERNEL_SPIKE_ACCESS_CHECKS
        world.VerifyBetweenSystems();
#endif
        {
            const Detail::RunningScope scope(descriptor_, world);
            invoke_(world);
        }
#if KERNEL_SPIKE_ACCESS_CHECKS
        world.VerifyAfterSystem(descriptor_);
#endif
    }

private:
    friend struct Detail::SystemFactory;

    System(SystemDescriptor descriptor, std::function<void(World&)> invoke)
        : descriptor_(std::move(descriptor)), invoke_(std::move(invoke)) {}

    SystemDescriptor descriptor_;
    std::function<void(World&)> invoke_;
};

namespace Detail {

struct SystemFactory {
    template <class F>
    static System Make(const Registry& registry, std::string name, F function) {
        SystemDescriptor descriptor = Describe<F>(registry, std::move(name));
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

// A free function, a non-generic lambda, or a function object.
template <class F>
[[nodiscard]] System MakeSystem(const Registry& registry, std::string name, F function) {
    return Detail::SystemFactory::Make(registry, std::move(name), std::move(function));
}

// A member function called on object, which must outlive the System.
template <class C, class M>
    requires std::is_member_function_pointer_v<M>
[[nodiscard]] System MakeSystem(const Registry& registry, std::string name, C& object, M member) {
    return Detail::SystemFactory::MakeMember(registry, std::move(name), object, member);
}

} // namespace KernelSpike
