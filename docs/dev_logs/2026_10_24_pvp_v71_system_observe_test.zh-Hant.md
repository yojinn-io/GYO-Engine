# PvP v7.1 after-re-evaluate：系統其餘＋觀測＋測試

日期：2026-10-24 起。分支 `claude/pvp-v71-system-observe-test`（從 master `23fab75`）。

**狀態：進行中**。D159 的三條 PR 線之中的第二條（第一條 sdl-isolation 是 PR #85，第三條「發行」之後另開）。批次的內容、驗收與決策點見 [after-re-evaluate 第 9 節](../object_fps_pvp/plans/v7.1/after-re-evaluate.md)。使用者 2026-10-24：「after-re-evaluate 按順序來，開個PR。」

## 批次

| 批次 | 檔位 | 估時（h） | 狀態 |
|---|---|---|---|
| `silent-branch-asserts` | high | 4～6 | 列點完成（[silent-branch-asserts.md](../object_fps_pvp/plans/v7.1/silent-branch-asserts.md)），等使用者決定 Go 的機制與各點 |
| `cxx-flag-removal` | high | 3～4 | |
| `contract-readers` | medium | 3～4 | |
| `trace-no-overwrite` | high | 4～6 | |
| `analyzer-converge` | high | 5 | |
| `token-tests` | high | 4～6 | |
| `client-path-tests` | high | 3～4 | |

時間框：任一批超過估時 1.5 倍，或單一問題累計超過 2 小時，就停下回報。
