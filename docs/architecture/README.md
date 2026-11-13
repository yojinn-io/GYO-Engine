# Engine architecture: entry

This page is the fixed entry for the Engine's architecture documents. It records where each kind of rule lives today
and is updated whenever that changes (for example when a target document moves back to `docs/architecture/`). It holds
no rules of its own: each section only points to the document that is the authority. AGENTS.md and agents/ point here
by section name, so the four section names below do not change.

Maintained by the docs librarian; the content of the linked documents belongs to their owners (the architect for the
Engine). Updated 2026-11-13.

## Design Invariants

Where the Engine's Design Invariants are written and which set binds a piece of code:
[`target/README.md`](target/README.md) (the sections on design authority and legacy, and on conflicts between target
and legacy Design Invariants). The kernel's invariants are written once, in [`target/kernel.zh-Hant.md`](target/kernel.zh-Hant.md);
read it before changing an Engine component.

## Batch declaration checks

What every batch's pre-declaration must check when it moves architecture to a new shape:
[`target/README.md` — 每批事前宣告的檢查項](target/README.md#每批事前宣告的檢查項使用者-2026-11-11-的原則自--audit-後適用).

## Layers and dependencies

How the layers between the Engine and products or tools are composed, and the dependency rules between them:
[`target/runtime-composition.zh-Hant.md`](target/runtime-composition.zh-Hant.md) §4.

## Naming

What a name in the Engine or a reusable layer may refer to:
[`target/naming.zh-Hant.md`](target/naming.zh-Hant.md), Design Invariant 4.

## Other entries

- Target architecture documents and their status: [`target/README.md`](target/README.md).
- Limits accepted on purpose and the conditions for lifting them: [`target/limits.zh-Hant.md`](target/limits.zh-Hant.md).
- Kernel rework plan and records: [`plans/kernel/README.md`](plans/kernel/README.md).
- Rules that several components must follow because of one contract: [cross-component rules](../cross-component-rules.md).
