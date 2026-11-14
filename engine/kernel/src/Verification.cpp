// Access verification, layer 1 (spec A.6): the thread's current System or hook
// call. The Executor sets it around every call (Detail::CallScope); a World
// access made while it is set is compared with that call's access description.
// On a thread the System started itself it is empty, so those accesses are taken
// as the Host's (spec R-03, limit L7). tests/common/kernel/kernel_guard.py (K4)
// and kernel_symbols.py allow exactly this thread_local and TypeKey's anchor.

#include "engine/kernel/detail/Context.hpp"

#include "engine/kernel/Registry.hpp"

#include <algorithm>

namespace GYO::Kernel::Detail {

// Constant-initialized (a null pointer). kernel_guard K4 allows exactly the shape
// "thread_local T* tlCurrent" (no constinit keyword in front).
thread_local SystemContext* tlCurrent = nullptr;

SystemContext* CurrentThreadCall() noexcept {
    return tlCurrent;
}

void SetCurrentThreadCall(SystemContext* call) noexcept {
    tlCurrent = call;
}

AccessDesc MakeAccessDesc(const AccessSet& access) {
    AccessDesc desc;
    desc.wholeWorld = access.exclusive;
    auto add = [&desc](const AccessKey& key, std::uint8_t mode) {
        EntryKind kind = EntryKind::Component;
        switch (key.kind) {
        case AccessKind::Component: kind = EntryKind::Component; break;
        case AccessKind::Resource: kind = EntryKind::Resource; break;
        case AccessKind::Event: kind = EntryKind::Event; break;
        case AccessKind::DomainTick: return;
        }
        for (AccessEntry& entry : desc.entries) {
            if (entry.kind == kind && entry.id == key.id) {
                entry.modes = static_cast<std::uint8_t>(entry.modes | mode);
                return;
            }
        }
        desc.entries.push_back(AccessEntry{kind, key.id, mode});
    };
    for (const AccessKey& key : access.reads) add(key, kModeRead);
    for (const AccessKey& key : access.writes) add(key, kModeWrite);
    std::sort(desc.entries.begin(), desc.entries.end(), [](const AccessEntry& a, const AccessEntry& b) {
        return a.kind != b.kind ? a.kind < b.kind : a.id < b.id;
    });
    return desc;
}

} // namespace GYO::Kernel::Detail
