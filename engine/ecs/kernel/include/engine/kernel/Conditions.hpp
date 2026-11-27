#pragma once

// The Kernel's run conditions on a Resource, as Bevy's common conditions (bevy_ecs/src/schedule/condition.rs, bevy
// 6a911653f): the same names and meanings. Each is named by the caller (a condition's identity is in the
// fingerprints); the ones with a value carry it as Condition<&Fn>(name, value) does (copied once, never written).
//
//   ResourceExists<R>(name)                   R is there                                      (resource_exists, :688)
//   ResourceEquals<R>(name, value)            *R == value; R not there is an assertion         (resource_equals, :728;
//                                             Bevy fails the parameter's validation and gives it to its error handler,
//                                             a panic by default; a GYO condition returns bool and the Kernel has no
//                                             error handler, DI-KS 2)
//   ResourceExistsAndEquals<R>(name, value)   R is there and *R == value; not there: false    (resource_exists_and_equals,
//                                                                                              :770)

#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"

#include <concepts>
#include <string>
#include <utility>

namespace GYO::Kernel {

namespace Detail {
template <class R>
bool ResourceIsThere(Res<R> resource) {
    return resource.Present();
}
template <std::equality_comparable R>
bool ResourceIs(const R& value, Res<R> resource) {
    return *resource == value;
}
template <std::equality_comparable R>
bool ResourceIsThereAndIs(const R& value, Res<R> resource) {
    return resource.Present() && *resource == value;
}
} // namespace Detail

template <class R>
[[nodiscard]] ConditionRef ResourceExists(std::string name) {
    return Condition<&Detail::ResourceIsThere<R>>(std::move(name));
}

template <std::equality_comparable R>
[[nodiscard]] ConditionRef ResourceEquals(std::string name, R value) {
    return Condition<&Detail::ResourceIs<R>>(std::move(name), std::move(value));
}

template <std::equality_comparable R>
[[nodiscard]] ConditionRef ResourceExistsAndEquals(std::string name, R value) {
    return Condition<&Detail::ResourceIsThereAndIs<R>>(std::move(name), std::move(value));
}

} // namespace GYO::Kernel
