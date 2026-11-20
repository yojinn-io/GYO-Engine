# 日誌機制（`v8-arch-gyo2-logging`）

日期：2026-11-20～21。分支 `claude/gyo2-logging-10dfcd`（從 `gyo2` 8ec005f2 切出，2026-11-21 rebase 到 e4072b5c，PR #254、#255 之後）；draft PR #256。**狀態：logging 的部分做完，等合併前核准；trace 改成另一個 Module，另寫宣告、另開 PR（使用者 2026-11-21）。**

這一批由任務卡做（GYO2 主會話開出）。事前宣告：[logging-declaration.zh-Hant.md](../architecture/target/v2/logging-declaration.zh-Hant.md)（修訂 1 動工前核准；修訂 2 第 17 節；修訂 3 第 18 節）。Design Invariants：[logging.zh-Hant.md](../architecture/target/v2/logging.zh-Hant.md)。

## 做了什麼

| 步 | 提交 | 內容 |
|---|---|---|
| 宣告 | 96f2d85d、d1360409、566ac3cb | 修訂 0；修訂 1（Fable 5.2／high 的敵對式檢查：必改 9、建議改 10、小問題 10，全部照改）；動工前核准的紀錄與 O-11 例子的更正 |
| 第 1 步 | 0341f715 | 骨架：`engine/logging`、`engine/world_logging` 兩個 Module 與登記檔兩列；`Record`、`Level`、`RecordBuilder`、`Logger` 的安裝與卸下、`MemorySink`、`Base::Log` 處理器、`Write` |
| 第 2 步 | 358dd53f | `ConsoleSink`、JSON lines（封套、NaN 等的字串、重複鍵、寫不出的那一筆、結尾行、golden）、`JsonLinesFileSink`（有上限的佇列、寫者執行緒、件數、致命寫出口）、兩個附加 handle 的並行寫測試 |
| 第 3 步 | ffe64928 | `Forward`、描述器、`LogWorldLabel`、`TakeKernelDiagnostics`、LG-3 的編譯失敗樣本、篩選與 `SetLevel` |
| 第 4 步 | 6c263938、46a22ba3 | `WriteFatalRecord`、斷言與 terminate 處理器；`WriteFailure`、`WithFailureLog`（修訂 3 拿掉） |
| 第 5 步 | 67672c66、73a7b935 | 手動 span（修訂 3 拿掉，歸 trace）；情境第 3 段；判定 7 的量測 |
| 第 6 步 | 4830e53e、88ce4c66 | LG-4／LG-10 的含入掃描守衛；突變清單 |
| 修訂 2、3 | e380b223、4120c1a0、7c9accb5 | 見宣告第 17、18 節：去重拿掉（使用者「去重是補丁」）；異常分級之後 E1 不經日誌的函式（`WriteFailure`、`WithFailureLog` 拿掉）；terminate 先寫一筆 `Fatal`；`product` 鍵拿掉；日誌和 trace 分成兩個 Module，span 拿出 logging |
| 審查的修正 | aa2f508c | Fable 5.2／high 審查（必改 2、建議改 7、小問題 4）：`SetLevel` 的競合、致命紀錄的重複鍵、寫者執行緒的 `bad_alloc`、替換紀錄的送達對象（各先 Red）；`world_time` WT-4、DI 頁的守衛欄、Windows 的每執行緒 terminate、殘留的舊說法 |
| 敵對式檢查的補強 | 7504ef96 | Fable 5.2／high 的敵對式檢查找到清單外 31 處：條文與判定寫明的 17 處補測試與突變、只缺突變的 3 處補突變；讀標準錯誤的案改用探測程式 `ConsoleProbe`（OS 邊界守衛不准測試含平台標頭）；terminate 探測在 Windows 的誤判（`abort()` 的結束碼 3）修正 |

## 驗證與經過

- **本機**（macOS x86_64，AppleClang，core preset）：審查與敵對式檢查的修正之後，全量 ctest 151/151 過。
- **L1**：第 2 步的 run 44704959856，三平台綠，含兩個附加 handle 各寫 10 萬行的並行寫測試（RC-L5 沒有觸發）。合併前的 L1 見合併前核准頁。
- **判定 7**（只量、不設門檻；Intel i9-9980HK、Release，一次）：直接寫一筆進佇列中位數 3199 ns、p99 23386 ns；經 `Forward` 一筆約 6759 ns；被篩掉 47 ns；一筆 395 位元組。
- **terminate 處理器**：先 Red（三種情形都 0 筆，`step7-terminate-red.txt`），再 Green。
- **突變**：第 1 回 56/64（8 個沒過，原因都在清單與測試，不在實作，見宣告第 18 節）；第 2 回 63/63；修訂 3 之後第 3 回 60/60；審查與敵對式檢查之後第 4 回 85/86＋1 個 stale（審查修正改了寫者迴圈，照新程式改寫 `lg-file-batch-reversed` 後 killed）＝86/86；§14.4 驗證者建議的 `lg-baselog-not-written` 加入後 87/87。
- **審查**（Fable 5.2／high，隔離的工作樹，基準 7c9accb5）：必改 2（WT-4 條文、DI 頁守衛欄）、建議改 7、小問題 4；修了必改 2、建議改 S2～S5（S1 寫進「不保證」、S6 寫進宣告第 5 節的表、S7 本頁）、小問題 m1、m4；m2、m3 記在合併前核准頁。報告 `review-fable.md`。
- **突變清單的敵對式檢查**（Fable 5.2／high）：38 個候選存活（31 處），全部「沒有測試抓得到」；17 處補了、3 處補突變、14 處細節沒補（README：觀察到漏看才加）。報告 `mutation-adversarial-fable.md`。
- **L1**：審查前的 run 44774463275，windows-x64 的 terminate 探測三件失敗（探測的判讀問題，修了），其餘綠；修正後 run 44779446258。
- **經過的轉折**（都記在宣告第 17、18 節）：
  - 2026-11-20 去重的鍵在兩個 Product 之間撞到；使用者定「記日誌，然後退出」，不去重。
  - 2026-11-20 使用者要日誌停下等異常分級；異常分級的規範轉來後重判，之後異常分級撤回一部分，PR #254 合併。
  - 2026-11-21 使用者定 World 壞掉那一行「日誌只是看，trace 才是追問題」；`RunProduct` 由 PR #255 拿掉，記不記由產品的 main 定。
  - 2026-11-21 使用者指出卡片偏離職責（情境與判定曾在驗編排層與產品的行為），收窄到日誌自己的事；同日使用者定日誌和 trace 是兩個 Module，教「假設→查證→對比→推斷→總結」的流程，查證 Bevy、flecs 的原始碼後核准 trace 的方向。

## 突變的增減（§14.5）

- 清單 `tests/common/logging/mutations.json`：第 1～6 步 64 個，修訂 3 之後 60 個，最後 87 個。
- 退役 9：`WriteFailure`／`WithFailureLog` 的 6 個（`lg-failure-wrong-text`、`lg-failure-kernel-code-always`、`lg-withfailurelog-no-write`、`lg-withfailurelog-success-exit`、`lg-withfailurelog-rethrows`、`lg-withfailurelog-swallows`）；span 的 3 個（`lg-json-span-keys`、`lg-span-refused-reads-clock`、`lg-span-duration`）。機能拿掉，同一個變更退役（基準 7）。
- 新增 5：terminate 的 `lg-terminate-no-record`、`-category`、`-no-what`、`-other-as-std`、`-none-as-other`（每個改的是一個獨立的決定：寫不寫、category、`what()`、不是 `std::exception`、沒有例外）。
- 改寫：`lg-json-key-order` 改成 `world` 與 `world_interrupted` 換位置（`product` 鍵拿掉）；第 1 回沒過的 8 個（`expect` 與案例）。
- 審查與敵對式檢查之後：審查修正 4、敵對式檢查 22（17 處＋只缺突變 3＋級別名稱 1，另 `expect` 分辨度的改寫），清單 87。
- 數目：Design Invariants 10 條在用（LG-9 退役）× 5 ＝ 50，清單 87，超過的理由在合併前核准頁第 4 節。

## 證據的所在

本機 macOS，主 checkout `build/evidence/gyo2-logging/`（git 不追蹤）：參考引擎偵察（`recon-reference-engines.md`、`recon-log-vs-trace.md`、`recon-trace-b0.md`）、修訂 0 的敵對式檢查、各步的 Red 輸出、判定 7 的量測、突變第 1～4 回、審查與敵對式檢查的報告。
