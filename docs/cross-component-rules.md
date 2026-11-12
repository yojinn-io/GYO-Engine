# Cross-component rules

Rules that more than one component must follow because they are a direct consequence of an Engine or framework
contract. Each rule has exactly one owner; the other places only cite it.

Approved by the user on 2026-11-07. Maintained by the docs librarian session.

## How to use this page

- Before writing a rule that follows from an Engine or framework contract (for example "only the latest dialog answer
  counts"), look it up here.
  - Found: cite the owner's section and follow it. Do not restate the rule in your own words.
  - Not found: add a row with its owner before relying on it.
- An unclear or disputed owner goes to the architect session; do not decide it in a feature task.
- "Current owner" is where the rule lives today. "Target owner" is where an approved plan will move it; it is marked
  with the moving batch and stays empty when no move is planned. Do not merge the two into one cell.
- "Known sites" lists implementations and citations checked against the source on the date shown. A site that breaks
  the rule is listed as non-compliant with where its fix is recorded.

## Rules

| # | Rule | Current owner | Target owner | Guard | Known sites (checked 2026-11-12) |
|---|---|---|---|---|---|
| CCR-1 | System path dialogs: each request gets a new tag; only the latest request's answer counts; `Chosen` and `Cancelled` end the request; `Failed` does not end it, because one more answer may follow; the caller can abandon the request (stop waiting) and then drops whatever still comes. | Engine: [platform-sdl.md](architecture/platform-sdl.md#design-invariants) Design Invariant 10 states what the platform guarantees (at most two answers, `Failed` may be followed by one more); each consumer applies the bookkeeping itself. | framework `Framework::PathDialogRequests<Purpose>` (target, moving batch `v8-tool-path-dialog-rule-handover`, paused with the feature line). The framework itself is planned to retire into `plugins/` (kernel `v8-arch-plugins-layer`). | Platform side: `tests/common/platform_sdl/PathDialogTests.cpp` and the mutations in `docs/architecture/plans/framework-rework/scripts/platform_path_dialog_mutations.json`. Consumer side: ui_editor's rule tests in `tests/ui_editor/EditorCoreTests.cpp`; audio_editor and model_viewer have none. | Compliant: ui_editor `PathDialogRequests` (`tools/ui_editor/src/PathDialog.cpp`); audio_editor `ApplyDialogAnswers` (`tools/audio_editor/src/AudioEditorWindow.cpp`). Non-compliant: model_viewer (`tools/model_viewer/src/ViewerWindow.cpp`: one fixed tag `kContentDialog` at :43, answers read at :112, requests sent at :621-627; no waiting state, so a second click while the dialog is open sends a second request and every answer is applied; `Failed` only shows an error). Read from the code, not reproduced on a machine. Fix recorded in [kernel README](architecture/plans/kernel/README.md) and folded into `v8-tool-path-dialog-rule-handover`. |
