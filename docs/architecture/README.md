# Engine architecture: entry

This page is the fixed entry for the Engine's architecture documents. It records where each kind of rule lives today
and is updated whenever that changes (for example when a target document moves back to `docs/architecture/`). It holds
no rules of its own: each section only points to the document that is the authority. AGENTS.md and agents/ point here
by section name, so the four section names below do not change.

The docs librarian owns this file, its place and its format. Which authority document and section each entry links to is
the architect's decision; the batch that moves back or creates an authority document updates the link in the same change.
Updated 2026-11-13.

## Design Invariants

Where the Engine's Design Invariants are written and which set binds a piece of code: [`target/README.md`](target/README.md),
the sections [分界](target/README.md#分界), [命名](target/README.md#命名) and
[目標 DI 與現況 DI 衝突時](target/README.md#目標-di-與現況-di-衝突時架構師決定2026-11-11), and its opening line: read
[`target/kernel.zh-Hant.md`](target/kernel.zh-Hant.md) before changing any component (the kernel's invariants are written
only there).

## Batch declaration checks

What every batch's pre-declaration must check when it moves architecture to a new shape:
[`target/README.md` — 每批事前宣告的檢查項](target/README.md#每批事前宣告的檢查項使用者-2026-11-11-的原則自--audit-後適用).

## Layers and dependencies

How the layers between the Engine and products or tools are composed, and the dependency rules between them:
[`target/runtime-composition.zh-Hant.md`](target/runtime-composition.zh-Hant.md) §4.
An Engine-owned plugin (maintained with the Engine) and an extension under `plugins/` (maintained outside it, for
example networking) are two different owners; the same §4 says how to tell them apart.

## Naming

What a name in the Engine or a reusable layer may refer to: [`target/naming.zh-Hant.md`](target/naming.zh-Hant.md)
(Design Invariant 4, "a name is the tag of a concept", is one of its rules).

## Other entries

- Target architecture documents and their status: [`target/README.md`](target/README.md).
- Limits accepted on purpose and the conditions for lifting them: [`target/limits.zh-Hant.md`](target/limits.zh-Hant.md).
- The rework in progress (a plan, current state, not a rule): [`plans/kernel/README.md`](plans/kernel/README.md).
- Rules that several components must follow because of one contract: [cross-component rules](../cross-component-rules.md).
