#pragma once

// Engine kernel (docs/architecture/target/kernel.zh-Hant.md). This header is the
// scaffold of the subsystem: it holds no type yet. World, System contract,
// Registry, Scheduler and Executor arrive in later batches
// (docs/architecture/plans/kernel/migration-plan.zh-Hant.md, section 3).
// Every file under engine/kernel/ is checked by tests/common/kernel/
// (kernel_guard.py for spec J-9, naming_guard.py for target/naming DI 1 and 3).

namespace GYO::Kernel {
} // namespace GYO::Kernel
