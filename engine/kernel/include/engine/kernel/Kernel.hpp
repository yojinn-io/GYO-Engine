#pragma once

// Engine kernel (docs/architecture/target/kernel.zh-Hant.md): the public
// headers. System parameter types and their derived access (Params.hpp,
// SystemTraits.hpp), the Registry and its lifecycle (Registry.hpp), Plans and
// fingerprints (Plan.hpp) since registry-schedule; the World (World.hpp) and
// the Product with its Serial Executor (Product.hpp) since world-exec
// (docs/architecture/plans/kernel/migration-plan.zh-Hant.md, section 3).
// Every file under engine/kernel/ is checked by tests/common/kernel/
// (kernel_guard.py for spec J-9, naming_guard.py for target/naming DI 1 and 3,
// kernel_symbols.py for writable data symbols).

#include "engine/kernel/Access.hpp"
#include "engine/kernel/Product.hpp"
#include "engine/kernel/Declarations.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Plan.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/SystemTraits.hpp"
#include "engine/kernel/TypeKey.hpp"
#include "engine/kernel/TypeList.hpp"
#include "engine/kernel/World.hpp"
