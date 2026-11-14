#pragma once

// Product (spec section 0 "Runtime", E; kernel HANDOFF v8-arch-kernel-app-name):
// one World, the frozen Registry it was made from, and the Serial Executor
// that runs that Registry's schedules (target/kernel DI-KS 2, 9, 14; spec E.2).
//
// Run(schedule) follows spec E.2: for each Plan step in order,
//   System     (0) with verification on, the boundary comparison "before"
//              (a change found here is the previous step's or the Ready
//              window's: a kernel-level Err, the System is not called);
//              (1) tlCurrent set; (2) run conditions, in order, short-circuit;
//              (3) the step's stamp (also when a condition is false: X-5);
//              (4) the call; (5) with verification on, the comparison
//              "after" (also when (4) was skipped); every error of the call is
//              one sibling list with the System's own Err; (6) tlCurrent cleared;
//   SyncPoint  a stamp; the deferred records applied in the order of spec F.2;
//              with verification on, W-1; the sync epoch +1;
//   EventTrim  the domain's records before the current tick dropped (F.3).
// An Err from any source is recorded once (the handler), the World becomes
// Interrupted, every record not yet applied is discarded, diagnostics are kept,
// and Run returns the Err (spec E.5). An exception from a System or hook is not
// caught: the World becomes Interrupted the same way and the exception goes on.
// Running an Interrupted World is a GYO_ASSERT (rebuild it: a new Product).
//
// v1 is Serial only (LIM-2). The Product owns no thread (DI-KS 15): Run is called
// on the schedule's runner thread by the Host.

#include "engine/base/Result.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace GYO::Kernel {

class Product {
public:
    // registry must be frozen and outlive the Product. worldName identifies this
    // Product's World in errors and verification reports (spec A.6).
    explicit Product(const Registry& registry, std::string worldName = {});
    ~Product();
    Product(const Product&) = delete;
    Product& operator=(const Product&) = delete;

    [[nodiscard]] World& GetWorld() noexcept { return world_; }
    [[nodiscard]] const World& GetWorld() const noexcept { return world_; }

    // Runs one schedule's Plan once (spec E.2). GYO_ASSERT: the World is Ready
    // and the schedule exists.
    Engine::Base::Result<void, KernelError> Run(std::string_view schedule);

    // The Executor's handler: called once per Err (target/error-handling DI 2:
    // the only place a kernel error is logged). Default: the whole chain to stderr.
    void SetErrorHandler(std::function<void(const KernelError&)> handler) { handler_ = std::move(handler); }

    // The fields of a "verification on" report (target/system-contract 2b
    // condition 2). Meaningful once the first Run has read KernelSettings.
    [[nodiscard]] VerificationReport Verification() const;

private:
    struct MountState {
        // Spec B-12: per mount.
        ChangeStamp lastRun = 0;
        std::vector<std::pair<TypeKey, std::shared_ptr<void>>> locals;
    };
    struct SystemState {
        Detail::AccessDesc access;
        std::vector<MountState> mounts;
    };

    Engine::Base::Result<void, KernelError> Fail(KernelError error);

    const Registry* registry_;
    World world_;
    std::vector<SystemState> systems_;
    std::function<void(const KernelError&)> handler_;
};

} // namespace GYO::Kernel
