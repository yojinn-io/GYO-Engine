# 架構 v8：v8-arch-guards-inventory（守衛與突變的盤點）

日期：2026-11-12。Owner：架構（架構師會話開出的任務卡）。計畫：[kernel README](../architecture/plans/kernel/README.md) 文件清單、遷移計畫 §3 共通「守衛與突變隨程式一起遷移」與 §5.2（分支 `claude/arch-migration-plan`）、使用者決定 `v8-arch-guards-migrate-with-code`。
- 執行：本卡會話（Fable／high；逐檔讀碼與 grep low），工作樹 `.claude/worktrees/objective-mclaren-370123`，分支 `claude/arch-guards-inventory`（自 origin/master `d0d8563f`）。
- 性質：只讀盤點。不改守衛、突變、程式；寫入只有報告、本 dev_log、kernel README 一行、HANDOFF 一行。

## 起因

使用者（2026-11-12，主會話）：很多概念會變，以前的突變和守衛不一定能判斷新架構，這些檢查也要重構，否則會把架構拉回原來的形狀。架構師定為遷移計畫的橫切規則（§3 共通、§0 A10），起點清單是 legacy-impact §4 的 21 個守衛，§5.2 預計了每列判去處的批。本卡在遷移批開工前，把 21 個守衛與 repo 內全部突變清單逐個判「守的性質、來源條文、去處、涉及的突變、批、不動會判誰違規」，給各批事前宣告對帳。

## 做了什麼

- 報告 [guards-inventory.zh-Hant.md](../architecture/plans/kernel/guards-inventory.zh-Hant.md)：§2 守衛 21 節（63 子列，每列七欄）、§2.22 補遺 6 個不在 21 列但遷移批會碰到的守衛、§3 突變清單 26 檔＋執行器 3 支、§4 缺口 9 條、§5 給架構師的問題 12 條、§6 與 §5.2 的不一致 9 處、§7 範囲の限界。
- 掃描：`git ls-files | grep -i mutation`（正例 `tests/object_fps_pvp/mutations.json` 先命中）；Python 逐檔讀 `mutants` 陣列；pvp 925 條按 `file` 欄分組對照遷移批；`git grep -n 'determinism\.md' -- tests apps build/acceptance engine spikes` 退出碼 1（正例 `entity-store.md` 命中退出碼 0）確認無守衛以 determinism.md 為判定依據。

## 件数の突き合わせ

**一致**：守衛 21（legacy-impact §4）＝報告 §2 的 21 節；突變清單 26 檔 1841 條（計數檔）＝§3 的 26 列、去處合計留 1131／重列 627／退役 83＝1841；執行器 3。守衛子列 63＝留 31／改寫 21／退役 11，另新增 3 件。

## 受理判定（看結果前宣告於卡；數字於開始時補）

| # | 判定 | 結果 |
|---|---|---|
| 1 | 覆蓋：21 守衛逐列、26 檔 mutations.json 逐檔、3 執行器各一列 | 成立 |
| 2 | 每列七欄齊；性質欄不是測試名 | 成立 |
| 3 | 第 7 欄以規則、正規式、白名單、連結期望等事實支撐 | 成立（每格引檔名行號） |
| 4 | 零修改：除報告、dev_log、README 一行、HANDOFF 一行外不改任何檔 | 成立（`git diff --stat origin/master`：4 檔，見下） |

事前宣告的數字送 parent 核定時 `send_message` 回「session parent not found」（`list_sessions linked:true` 回空），所以未經 parent 核定就寫了正文；由使用者在本會話核定補上（報告 §7 已記）。

## 主要發現（給架構師與各批宣告）

1. **會把架構拉回舊形狀的守衛**（報告第 7 欄）：`EntityModelTests` 是 `PvpMatch` 的 friend、直接讀七個私有欄（G1、G2、G7）——match-sim 把欄搬進 World 的第一個 commit 就編不過，留著就逼出 `PvpMatch` 門面；`PvpMatchTests` 的 OutageRig 以 `LocalPlayerPrediction`＋`MovementTrace` 建構驅動——留著就逼 client-sim 保留該型別；framework 的 PV2／PV5 判定本身就是 readback 形狀——搬進 plugin 不改判定就把 readback 帶進渲染 World；四個工具守衛的正向清單沒有 `GYO::Kernel`、沒有 plugin target——不改就只能留在 framework 上。
2. **空轉（掃不到新程式）**：`os_boundary_guard.py` 的 `ROOTS`、OS 編譯定義檢查的根、`ProcessCodePageCheck` 的 regex 都沒有 `plugins/`（framework 也從來不在）；platform-sdl G1／G3 與 `tests/common/input/CMakeLists.txt` 的 `if(TARGET …)` 在 target 搬走時靜默退出；`framework_dependency_guard.py` 在 `framework/` 刪除後 `rglob` 回空、空轉通過，但 CMake link check 會硬失敗，所以 `tests/framework` 必須同批刪。
3. **缺口 9 條**：target DI 有條文無批的 3（naming DI 1 掃描、Peer 掃描、J-9 加 log 呼叫）；legacy 退役無人接的 3（framework「不是產品」「中性」對共用 plugin；entity-model E-5 後半「欄成員關係恰等於種類表」）；掃描根與靜默退出的 3。
4. **與 §5.2 不一致 9 處**（報告 §6）：主要是第 18 列（G3 歸 match-host、`RequireEntityOrder` 是退役）、第 19 列（OutageRig 歸 client-sim 改寫）、第 4～6 列（要加 plugins-foundation 改掃描根）、第 10 列（plugin 側是新增不是改寫；fw-* 在搬檔的批重列）。

## 證據

- 主 checkout `build/evidence/guards-inventory/`（git 管理外）：`mutations-count.txt`（26 檔 1841 條與每檔鍵）、`pvp-mutations-by-file.txt`（925 條分組）、`migration-plan.snapshot.md`、`kernel-HANDOFF.snapshot.md`、`migration-plan.snapshot.rev`（計畫分支快照與 rev）。
- `git diff --stat origin/master`：`docs/architecture/plans/kernel/guards-inventory.zh-Hant.md`（新）、`docs/dev_logs/2026_11_12_arch_guards_inventory.zh-Hant.md`（新）、`docs/architecture/plans/kernel/README.md`（＋2 行）、`docs/architecture/plans/kernel/HANDOFF.md`（＋1 行）。
- 時間：起點 2026-11-12 23:32；報告、dev_log 與兩行補記完成 23:56（估 4～6 h，實走約 25 分鐘；讀碼以並行工具呼叫壓縮，判斷沒有省——未執行守衛與突變，見報告 §7）。

## 下一步

- 使用者核定事前宣告的數字（26 檔 1841 條、3 執行器）與報告；合併由使用者決定。
- 架構師回答 §5 的 12 個問題，把 §6 的不一致回寫到遷移計畫 §5.2（計畫分支或 a6-revision）。
- 各遷移批的事前宣告以報告 §2～§3 對應列為「守衛與突變的去處」表的起點；補遺 §2.22 一併對帳。
