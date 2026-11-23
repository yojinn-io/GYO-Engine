# 事前宣告：Kernel 診斷紀錄接上交換函式「出」（`v8-arch-gyo2-kernel-diagnostics-exchange`）

狀態：修訂 0。**動工前核准（2026-11-23）**（第 8 節；HANDOFF `v8-arch-gyo2-kernel-diagnostics-exchange-approved`）。核准與修訂都在本檔上做。

性質：任務卡（主會話開，2026-11-23）的事前宣告，批次表〈後續小事〉「world_logging 給一個接 `WorldSetup` 的登記函式」那一列。依據：HANDOFF `v8-arch-gyo2-forwarding-audit-exchange-keep`（使用者「照建議，選 (a)」）；已核准的設計 `exchange-declaration.zh-Hant.md:373`、`logging-declaration.zh-Hant.md:229`。
分支：從 `origin/gyo2`（0bb64a28）開的工作樹分支，draft PR 到 `gyo2`。

## 1. 範圍

| 改 | 檔 |
|---|---|
| 新函式 `WorldLogging::ForwardKernelDiagnostics(Orchestration::WorldSetup&)`：在 `Build` 登記一個交換函式「出」，內容是 `TakeKernelDiagnostics(world)`、一律回成功 | `engine/modules/world_logging/include/engine/world_logging/TakeKernelDiagnostics.hpp`、`src/TakeKernelDiagnostics.cpp` |
| 測試（見第 3 節） | `tests/common/world_logging/DiagnosticsTests.cpp` |
| 新突變 1 個 | `tests/common/logging/mutations.json` |
| O-3、O-12 的例子改成一致（DI，第 2 節） | `docs/architecture/target/v2/orchestration.zh-Hant.md` |
| LG-1 的「由 runner 取走」改成兩條路（DI，第 2 節；O-3／O-12 一改它就矛盾，所以一起） | `docs/architecture/target/v2/logging.zh-Hant.md`（連同非 DI 的模組表與入口圖） |
| 批次表〈後續小事〉那一列→完成；dev_log；README 列表 | `rebuild-batch-plan.zh-Hant.md`、`docs/dev_logs/2026_11_23_gyo2_kernel_diagnostics_exchange.zh-Hant.md`、`target/v2/README.md` |

**不碰**：編排層程式（`Product.cpp`、`Setup.hpp`）、Kernel、`TakeKernelDiagnostics` 的介面與內容、`examples/orchestration/product_main.cpp`（它不用日誌 Module）、兩份宣告的舊文字（歷史紀錄，不改寫）、world_logging 做成登記單位（宣告說「或之後」，不在本卡）。
**依賴**：不變。`gyo_world_logging` 本來就連 `GYO::Orchestration`（LG-10 不變），`logging_guard` 的 LG4-WORLD 已准 include orchestration。

**名字**：`ForwardKernelDiagnostics` 照同一 Module 的 `Forward<D>(setup, …)`——「在 `WorldSetup` 上接一條進日誌的路」；`TakeKernelDiagnostics` 留給「現在就取」。

**兩條路**（已核准的設計，不是新決定）：
- World 健康：`RunFrame` 每個 World 一輪結束後呼叫「出」→ 取走（最後一個同步點之後，`Product.cpp` 的 `RunRound`）。`RunGameInit`／`RunSchedule` 不呼叫交換函式，它們產生的紀錄留在 World，由下一次 `RunFrame` 的「出」取走（不丟）。
- World 壞掉：「出」不呼叫（O-12）。壞掉那一幀的紀錄由 runner（或產品的 main）呼叫 `TakeKernelDiagnostics` 取，紀錄標 `world_interrupted`。
- 已知限制（寫進註解與 LG-1 的「不保證」）：上一次 `RunFrame` 之後用 `RunSchedule` 跑出的紀錄，若接著就 `ReplaceWorld`，隨舊 World 丟掉；要的人在替換前自己取。

## 2. Design Invariant 條文的改前改後（AGENTS §3，要使用者同意）

**O-3（`orchestration.zh-Hant.md:24`）**
- 改前：「runner 在兩次入口之間讀寫 World、呼叫 Module 給的函式（例：`WorldLogging::TakeKernelDiagnostics`）是交換點的「runner 直接讀寫」（O-12），只做屬於 runner 的事，Module 的資料走套用點或交換函式；」
- 改後：「runner 在兩次入口之間讀寫 World、呼叫 Module 給的函式是交換點的「runner 直接讀寫」（O-12），只做屬於 runner 的事，Module 的資料走套用點或交換函式（例：Kernel 的診斷紀錄，World 健康時由 world_logging 登記的交換函式「出」取走）；World 壞掉之後交換函式不再被呼叫，還要取的由 runner 取（例：壞掉那一幀的診斷紀錄，`WorldLogging::TakeKernelDiagnostics`，O-12）；」

**O-12（`:53` 起）**
- 交換函式那一點，改前：「給「要整個 World、在 Run 之外」的事（例：Kernel 的診斷紀錄要在最後一個同步點之後取）。」
- 改後：「給「要整個 World、在 Run 之外」的事（例：Kernel 的診斷紀錄要在最後一個同步點之後取：world_logging 的 `ForwardKernelDiagnostics` 在 `Build` 登記一個呼叫 `TakeKernelDiagnostics` 的「出」）。」
- runner 直接讀寫那一點，改前：「…直接讀寫 World 或呼叫 Module 給的函式（O-3）；只給 runner 自己擁有的東西。」
- 改後：「…直接讀寫 World 或呼叫 Module 給的函式（O-3）；只給 runner 自己擁有的東西，以及 World 壞掉之後交換函式不再呼叫、還要取的東西（例：壞掉那一幀的 Kernel 診斷紀錄）。」
- 末段「同一件事不要兩種形都做。」改後：「同一件事不要兩種形都做（World 壞掉之後由 runner 接手交換函式沒做完的，不算兩種形）。」

**LG-1（`logging.zh-Hant.md:33`）**
- 改前：「Kernel 的診斷紀錄（`DroppedMessages`、`RejectedSpawn`）由 runner 取走，一個 World 一個取走的人；中斷的 World 也取，紀錄標 `world_interrupted`；」
- 改後：「Kernel 的診斷紀錄（`DroppedMessages`、`RejectedSpawn`）World 健康時由 `ForwardKernelDiagnostics` 登記的交換函式「出」在每輪結束取走（O-12），沒有登記時、或 World 壞掉之後由 runner 取走；一個 World 一個取走的人；中斷的 World 也取，紀錄標 `world_interrupted`；」
- 「**不保證**」末尾加：「上一次 `RunFrame` 之後用 `RunSchedule` 跑出、接著被 `ReplaceWorld` 換掉的 World 的診斷紀錄（替換前自己取）。」

緩的面／緊的面：這三條是說明「誰在什麼時候取」，沒有測試判定被改（§14.4 不適用，見第 4 節）。

## 3. 測試

在 `DiagnosticsTests.cpp`（`world_logging.diagnostics` 組）加兩案，都走真的 `TakeKernelDiagnostics`、經真的 `Product::RunFrame` 與交換函式：
1. `diagnostics: ForwardKernelDiagnostics takes the records at the end of every frame`：build 裡呼叫 `ForwardKernelDiagnostics`；第一幀之後不手動取，`kernel` 紀錄已有 1 筆（`world`＝World 名、不標 interrupted）；手動 `TakeKernelDiagnostics` 回 0（已被取走）；第二幀之後共 2 筆。
2. `diagnostics: on a World broken in its round the out is not called; the runner takes the records`：同一產品、讓 World 在一輪中途壞掉；`RunFrame` 回錯之後 `kernel` 紀錄 0 筆（「出」沒呼叫）；runner 呼叫 `TakeKernelDiagnostics` 回 1，紀錄標 `world_interrupted`。

不新增 ctest 名（照舊 `world_logging.diagnostics`）。

## 4. 突變（D184、§14.5）與 §14.4

- 新加 1 個：`lg-diag-exchange-in`——`ForwardKernelDiagnostics` 登記成「進」而不是「出」（一輪之前取，該幀的紀錄晚一幀才進日誌）。只有它捕得到「時機錯」；「完全沒登記」會被同一個判定捕到，故不另加。守：O-12 交換函式、LG-1。預期失敗：第 1 案「第一幀之後 1 筆」那一行的 CHECK。
- 相關重跑：`lg-diag-dropped-skipped`、`lg-diag-rejected-skipped`、`lg-diag-no-world`、`lg-diag-not-interrupted`（測試檔被改）＋新的 1 個。編排層的 `or-exch-` 不動（程式沒改）。
- §14.4：**不適用**。不改任何既有判定，只加測試；DI 的文字改動走 §3 的同意，不是守衛測試的判定變更。

## 5. 設計審查（批次表〈每批的設計審查〉）的預先說明

- Bevy 沒有每個 World 的診斷 buffer，也沒有每個 World 一輪前後的掛鉤（Bevy 的 `Events` 雙緩衝溢出時不留紀錄；`bevy_diagnostic` 是 Resource 加 System；SubApp 的 extract 是唯一接近的 World 間掛點）。這是 GYO 自己的機制：交換函式（O-12）＋ Kernel 診斷紀錄（B-13），接合點是 Bevy runner 呼叫 `app.update()` 之後那個位置。
- 同一件事一個公開出口：取診斷仍只有 `TakeKernelDiagnostics`；新函式只是把它登記成「出」，不另寫一份。

## 6. 模型／ギア

- 實作者：Opus 5.5／high（本會話）。
- DI 條文的敵對式檢查：**Fable／high**（和作者不同模型；Fable 比主會話大，**要使用者同意**）。時機：核准後、commit 文件之前；若改動條文文字，改後的版本再給使用者看。
- 設計審查（批次表規定的審查者 Fable／high、唯讀）：同上一併請同意；不同意就用 Opus／high 並在報告寫明「同模型」。

## 7. 要使用者定的

1. 第 2 節三條的改前改後（含把 LG-1 一起改）。
2. 敵對式檢查與設計審查用 Fable／high。

## 8. 決定紀錄

2026-11-23 使用者在本卡直接回覆：「照宣告做，LG-1 一起改，用 Opus就可以了。」
- 第 7 節第 1 件：第 2 節三條的改前改後照寫，LG-1 一起改。
- 第 7 節第 2 件：DI 條文的敵對式檢查與設計審查用 **Opus／high**（不用 Fable）。和實作者同模型，報告寫明「同模型」（navigator「別的模型用不了時」那一條的例外是使用者選的）。
- 同日主會話也轉來使用者在那邊的原話：「照建議，兩件都同意，用 Opus」（HANDOFF `v8-arch-gyo2-kernel-diagnostics-exchange-approved`）。比過的選項：第 1 件 同意／不改 LG-1（不改就和 O-3、O-12 矛盾）；第 2 件 Fable／high（主會話原本的建議）／Opus／high（使用者選）。理由：條文只寫明「誰在什麼時候取」，沒有放寬任何判定。
