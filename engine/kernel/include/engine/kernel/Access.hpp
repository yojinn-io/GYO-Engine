#pragma once

#include "engine/kernel/TypeKey.hpp"
#include "engine/kernel/TypeList.hpp"

#include <cstddef>

// Access entries: the elements of a parameter's Contribution (spec B.4 point
// 3). A Contribution only collects types at compile time; Registry::Finish
// turns each entry into ids through AccessResolver (spec B.4 point 6: type ->
// id at registration, not at compile time; an unregistered type is R-E4).
//
// The entry set is the derived half of spec B.1's SystemDescriptor:
//   reads / writes         ComponentRead/Write, ResourceRead/Write, EventRead/Write, DomainTickRead
//   membershipReads        Membership (With / Without)
//   changeFilters          ChangeFilter (Changed / Added); also a membership read of its type
//   deferred               DeferredWrite (insert / remove / bundle), Despawn, Spawn (spawn also writes the allotter)
//   setReads               SetReadOf (expanded at finish to the named set's components)
//   diagnostics            Diagnostics (effect Diagnostics; no access, no conflict; spec S-8 exception)
//   local                  LocalState
//   exclusive              WholeWorld (ExclusiveWorld)
//   extract                ExtractComponentRead / ExtractResourceRead (read-only, source World)
//   untyped commands       UntypedCommands (checked at run time; spec F.1, F-10)

namespace GYO::Kernel {

enum class AccessMode : unsigned char {
    Read,
    Write,
};

namespace Detail {

// Implemented in Registry.cpp. One resolver per System or run condition per
// Finish; it records ids into that System's access set and reports R-E4 for an
// unregistered type, naming the parameter position.
class AccessResolver {
public:
    virtual void Parameter(std::size_t position) = 0;
    virtual void Component(TypeKey type, AccessMode mode) = 0;
    virtual void Resource(TypeKey type, AccessMode mode) = 0;
    virtual void Event(TypeKey type, AccessMode mode) = 0;
    virtual void DomainTick() = 0;
    virtual void Membership(TypeKey component) = 0;
    virtual void ChangeFilter(TypeKey component) = 0;
    virtual void DeferredWrite(TypeKey component) = 0;
    virtual void Despawn() = 0;
    virtual void Spawn() = 0;
    virtual void UntypedCommands() = 0;
    virtual void SetRead(TypeKey setTag) = 0;
    virtual void Local() = 0;
    virtual void WholeWorld() = 0;
    virtual void Diagnostics() = 0;
    virtual void ExtractComponent(TypeKey component) = 0;
    virtual void ExtractResource(TypeKey resource) = 0;

protected:
    AccessResolver() = default;
    ~AccessResolver() = default;
    AccessResolver(const AccessResolver&) = default;
    AccessResolver& operator=(const AccessResolver&) = default;
};

} // namespace Detail

namespace Access {

template <class T>
struct ComponentRead {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.Component(KeyOf<T>(), AccessMode::Read); }
};
template <class T>
struct ComponentWrite {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.Component(KeyOf<T>(), AccessMode::Write); }
};
template <class T>
struct ResourceRead {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.Resource(KeyOf<T>(), AccessMode::Read); }
};
template <class T>
struct ResourceWrite {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.Resource(KeyOf<T>(), AccessMode::Write); }
};
template <class T>
struct EventRead {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.Event(KeyOf<T>(), AccessMode::Read); }
};
template <class T>
struct EventWrite {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.Event(KeyOf<T>(), AccessMode::Write); }
};
// The domain tick of every mount's schedule (CurrentTick; spec B.4, S-4).
struct DomainTickRead {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.DomainTick(); }
};
template <class T>
struct Membership {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.Membership(KeyOf<T>()); }
};
template <class T>
struct ChangeFilter {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.ChangeFilter(KeyOf<T>()); }
};
template <class T>
struct DeferredWrite {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.DeferredWrite(KeyOf<T>()); }
};
struct Despawn {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.Despawn(); }
};
// Writes the World's allotter resource (spec A.1: Spawn derives write(EntityAllotter)).
struct Spawn {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.Spawn(); }
};
struct UntypedCommands {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.UntypedCommands(); }
};
template <class SetTag>
struct SetReadOf {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.SetRead(KeyOf<SetTag>()); }
};
template <class T>
struct LocalState {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.Local(); }
};
struct WholeWorld {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.WholeWorld(); }
};
template <class Record>
struct Diagnostics {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.Diagnostics(); }
};
template <class T>
struct ExtractComponentRead {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.ExtractComponent(KeyOf<T>()); }
};
template <class T>
struct ExtractResourceRead {
    static void Resolve(Detail::AccessResolver& resolver) { resolver.ExtractResource(KeyOf<T>()); }
};

} // namespace Access

namespace Detail {

template <class... Entries>
void ResolveEntries(AccessResolver& resolver, TypeList<Entries...>) {
    (Entries::Resolve(resolver), ...);
}

} // namespace Detail

} // namespace GYO::Kernel
