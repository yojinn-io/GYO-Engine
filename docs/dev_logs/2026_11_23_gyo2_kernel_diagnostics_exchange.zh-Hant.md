# 2026-11-23 Kernel 診斷紀錄接上交換函式「出」（`v8-arch-gyo2-kernel-diagnostics-exchange`）

Owner：日誌（world_logging）＋編排層的條文（GYO2 主會話開的任務卡）。分支 `claude/gifted-booth-2f61ba`（開向 `gyo2`）。作者 Opus 5.5。

## 為什麼

2026-11-23 轉發層的代碼審計後，使用者選 (a)：交換函式的登記（`WorldSetup::ExchangeIn`／`ExchangeOut`）留著，把唯一點名的使用者——Kernel 的診斷紀錄——接上（HANDOFF `v8-arch-gyo2-forwarding-audit-exchange-keep`，原話「照建議，選 (a)」）。交換點實作 PR #270 的設計審查已指出：交換函式在 Engine 裡還沒有呼叫者，O-3 與 O-12 對診斷紀錄的例子互相矛盾（批次表〈後續小事〉）。

事前宣告：[`kernel-diagnostics-exchange-declaration.zh-Hant.md`](../architecture/target/v2/kernel-diagnostics-exchange-declaration.zh-Hant.md)。修訂 0 動工前核准（使用者在本卡「照宣告做，LG-1 一起改，用 Opus就可以了」；主會話轉來「照建議，兩件都同意，用 Opus」，HANDOFF `v8-arch-gyo2-kernel-diagnostics-exchange-approved`）；修訂 1 在審查後改條文，使用者「照修訂 1 改，繼續」。

## 做了什麼

- `engine/modules/world_logging`：新函式 `WorldLogging::ForwardKernelDiagnostics(Orchestration::WorldSetup&)`，在 `Build` 登記一個交換函式「出」，內容是 `TakeKernelDiagnostics(world)`、一律回成功。名字照同 Module 的 `Forward<D>(setup, …)`。`TakeKernelDiagnostics` 的介面與內容不變；編排層、Kernel 不改；依賴不變（`gyo_world_logging` 本來就連 `GYO::Orchestration`，LG-10）。header、CMakeLists 的註解改寫成兩條路。
- 兩條路（已核准的設計 `exchange-declaration` 第 5 節、`logging-declaration` 5.4）：登記了的 World 由「出」在每輪結束取走；runner 只取「出」不會再被呼叫的那部分（有 World 壞掉、Product 結束之後，或替換那個 World、銷毀 Product 之前）；沒有登記的 World 由 runner 取。
- 條文（Design Invariants，使用者同意）：O-3、O-11（括號註）、O-12、LG-1，連同 logging 的模組表與入口圖。改前改後見宣告第 2、9 節。O-12 的守衛欄補上本批的兩案與突變。

## 檢驗

- 測試：`tests/common/world_logging/DiagnosticsTests.cpp` 加兩案（`world_logging.diagnostics` 組，不新增 ctest 名），都經真的 `Product::RunFrame`、交換函式與真的 `TakeKernelDiagnostics`：
  - 「ForwardKernelDiagnostics takes the records at the end of every frame」：第一幀之後不手動取，`kernel` 紀錄已有 1 筆（World 名、不標 interrupted）；手動取回 0；第二幀之後共 2 筆。
  - 「on a World broken in its round the out is not called; the runner takes the records」：一輪中途壞掉後 0 筆；runner 取回 1 筆、標 `world_interrupted`。
- 本機（macOS Intel，Apple clang，preset `core`）：整個 core 建置成功；`world_logging|logging|orchestration|scenario|guard` 的 ctest 63/63 通過；merge `origin/gyo2` 之後 `world_logging.*`、`scenario.segment3` 3/3 通過。
- 突變（D184 只跑相關的；`tests/common/logging/mutations.json`）：`lg-diag-dropped-skipped`、`lg-diag-rejected-skipped`、`lg-diag-no-world`、`lg-diag-not-interrupted`、`lg-diag-exchange-in`，**5/5 killed**。
- §14.4：不適用（沒有改任何既有判定，只加測試）。

## 突變（AGENTS §14.5）

新加 1 個：`lg-diag-exchange-in`——`ForwardKernelDiagnostics` 登記成「進」而不是「出」。只有它捕得到「時機錯」（一輪之前取，該幀的紀錄晚一幀才進日誌）；守 O-12 交換函式的例子與 LG-1。預期失敗：第 1 案的 `REQUIRE( afterFirstFrame.size() == 1 )`。「完全沒登記」會被同一個判定捕到，不另加。

## 審查

Opus 5.5／high，和作者**同模型**（使用者選，宣告第 8 節）。設計審查（批次表〈每批的設計審查〉五項）：可合併——一個公開出口仍是 `TakeKernelDiagnostics`；函式該存在（替代是每個產品各寫一份 lambda）；命名一致；突變守時機不擋誤用。Bevy 對照（查證過原始碼）：Bevy 沒有每個 World 的診斷 buffer 與一輪後的掛鉤，對應的是每個 World 的 error handler 當場記；GYO 偏離的理由是 Kernel 不寫日誌、紀錄在 Run 外才取，膠水一個 lambda（補進宣告第 5 節）。條文的敵對式檢查：要改（三條互相矛盾、「一個取走的人」與「替換前取」打架、「不保證」不完整）→ 修訂 1。

證據：主 checkout `build/evidence/gyo2-kernel-diagnostics-exchange/review-r0.md`（git 不追蹤，這台 macOS Intel）。建置與測試、突變的輸出未保存（數字已記在上面）。

## 沒做、留給之後

- `examples/orchestration/product_main.cpp` 不示範取壞掉 World 的診斷紀錄（它不用日誌 Module）。
- world_logging 做成登記單位（免得每個產品各在 `build` 呼叫一次）：宣告說「或之後」，有產品需要時再看。
