# docs/architecture/target/v2：GYO2 線的文件

`gyo2` 分支上重建通用引擎（`v8-arch-rebuild`）的文件都放這裡（使用者 2026-11-14，`v8-arch-gyo2-docs-target-v2`）。
2026-11-14 以前 Kernel 研究線（改造舊架構）的計畫、規格與紀錄在 [`../../plans/kernel/`](../../plans/kernel/README.md)，是參考，不是這條線的現況。

**先讀**：[HANDOFF.md](HANDOFF.md)（現況、使用者的決定、經過、下一步）。

## 給作者看的

- [orchestration.zh-Hant.md](orchestration.zh-Hant.md)：編排層（`engine/orchestration`）的 Design Invariants O-1～O-11 與 Replacement Criteria（`v8-arch-gyo2-orchestration-skeleton` 合併前核准定稿；`v8-arch-gyo2-time-events` 改 O-1、O-4、O-11；`v8-arch-gyo2-authority-rollback` 在 O-2 補 `ModuleBuffer`（建 World 時 Module 之間、作者與 Module 之間的共用處）、加 RC-O10，最終文字使用者 2026-11-22 核准）。
- [world-time.zh-Hant.md](world-time.zh-Hant.md)：時間 Module（`engine/modules/world_time`）——Resource、System、取樣、和讀檔／回滾／權威的交界、Design Invariants（型別 1～3、WT-1～WT-6）與 Replacement Criteria（`v8-arch-gyo2-time-events`，最終文字在合併前核准定稿；`v8-arch-gyo2-authority-rollback` 補 WT-1 的 `Resimulation` 每次準備放 false、WT-5 的伺服器權威 tick 寫者檢查，型別 3 改成逐型別雜湊，最終文字使用者 2026-11-22 核准）。
- [authority.zh-Hant.md](authority.zh-Hant.md)：權威 Module（`engine/modules/authority`）——權威登記、策略的插口、伺服器權威策略的啟動檢查（AU-E1～AU-E9，含從 Kernel 搬來的 Z6、Z8、D-E15）、一致性指紋、digest、G-5 的匯出；Design Invariants DI-A 1～12 與 Replacement Criteria（`v8-arch-gyo2-authority-rollback`，最終文字使用者 2026-11-22 核准）。
- [rollback.zh-Hant.md](rollback.zh-Hant.md)：回滾 Module（`engine/modules/rollback`）——`RollbackStrategy` 的契約、公開登記、官方策略（記錄、判斷、還原、重跑、輸入的歷史、權威值的入口）；Design Invariants R-1～R-8（R-7 取代 Kernel 的 D-E16）與 Replacement Criteria（含從 Kernel 搬來的「回滾等價破裂」；`v8-arch-gyo2-authority-rollback`，最終文字使用者 2026-11-22 核准）。
- [audio.zh-Hant.md](audio.zh-Hant.md)：混音器 Module（`engine/modules/audio`）——clip、voice、提交口與它的執行緒契約、時間點的換算、後端要求的介面；Design Invariants 1～11 與 Replacement Criteria 1～9（`v8-arch-gyo2-audio-review` 從 master 的 `audio.md` 搬來改寫，最終文字在合併前核准定稿）。
- [json.zh-Hant.md](json.zh-Hant.md)：JSON Module（`engine/modules/json`）的 Design Invariants J-1～J-9 與 Replacement Criteria（最終文字在 `v8-arch-gyo2-json-module` 合併前的合併前核准定稿）。
- [text.zh-Hant.md](text.zh-Hant.md)：text Module（`engine/modules/text`）——字型的資料契約 `"font"`、FreeType／HarfBuzz／libunibreak 三個 adapter、Design Invariants TX-1～TX-10 與 Replacement Criteria、使用的一方要照做的四件（CCR-14～16）、授權告示的義務（`v8-arch-gyo2-sdl-t`，最終文字在合併前核准定稿）。
- [logging.zh-Hant.md](logging.zh-Hant.md)：日誌（`engine/modules/logging`、`engine/modules/world_logging`）——入口 ①②③④⑥、E0 的致命路徑、JSON lines 資料契約、Design Invariants LG-1～LG-11（LG-9 退役）與 Replacement Criteria。trace 是另一個 Module（下一列）。
- [trace.zh-Hant.md](trace.zh-Hant.md)：trace（Base 的門面 `Trace.hpp`、`engine/modules/trace`）——每執行緒的段堆疊、父子、計時、路徑，經日誌寫出；Kernel 與編排層發的段；Design Invariants TR-1～TR-7 與 Replacement Criteria（最終文字在 `v8-arch-gyo2-trace` 的合併前核准定稿）。
- [asset.zh-Hant.md](asset.zh-Hant.md)：資產（`engine/modules/asset`、`engine/modules/world_asset`）——id、catalog 的 Data Contract（格式、Version、Validation Rule (A)(B)）、World 外的 `AssetStore`（照 Bevy 的演算法）、每個 World 的 `AssetRequests` 與 `AssetView`、狀態與就緒查詢、錯誤與件數、雜湊函式；Design Invariants AS-1～AS-13 與 Replacement Criteria（`v8-arch-gyo2-asset`，取代 `target/assets.zh-Hant.md`；最終文字使用者 2026-11-23 合併前核准）。
- [state-machine.zh-Hant.md](state-machine.zh-Hant.md)：狀態機 Module（`engine/modules/state_machine`）——名字表、封閉的型別、要求、套用的 System、子狀態、條件的 Design Invariants 與寫法。
- [entity-ids.zh-Hant.md](entity-ids.zh-Hant.md)：預設的實體配發器 `entity_ids`（`RangeAllotter`）的 Design Invariants EI-1～EI-3 與 Replacement Criteria（`v8-arch-gyo2-world-gen`，使用者 2026-11-22 合併前核准）。
- [transform.zh-Hant.md](transform.zh-Hant.md)：姿態 `transform`（`Transform`、`GlobalTransform`、沒有父時的同步）的用法、Design Invariants TF-1～TF-4 與 Replacement Criteria（`v8-arch-gyo2-sdl-transform-camera`，SDL 線子批 X1；最終文字在合併前核准）。
- [camera.zh-Hant.md](camera.zh-Hant.md)：相機 `camera`（元件、投影、2D／3D、推導值、目標大小的副本、座標換算）的用法、Design Invariants CM-1～CM-5 與 Replacement Criteria（同上）。
- [save-version.zh-Hant.md](save-version.zh-Hant.md)：存檔版本 `save_version`（純程式庫、預設不帶）的 Design Invariants SV-1～SV-3 與 Replacement Criteria（`v8-arch-gyo2-restore-impl`，使用者 2026-11-22 合併前核准）。
- [sdl.zh-Hant.md](sdl.zh-Hant.md)：SDL 的 Plugin（`plugins/sdl*`）——列與依賴、目錄的形、只建被選的 Plugin（`GYO_PLUGINS`）、SDL 3.4.0 的取得與補丁、Design Invariants SDL-1、SDL-2（子批 P `v8-arch-gyo2-sdl-p` 開頁；W、A、R2a 各自補）。
- [system-contract-page.zh-Hant.md](system-contract-page.zh-Hant.md)：System 契約的說明頁——怎麼寫一個 System、錯誤怎麼回報、要注意的地方、生成的元件與寫者。寫的是 `claude/gyo2-mechanism` 上能跑的程式。
- [diagrams/mechanism-runtime.svg](diagrams/mechanism-runtime.svg)：圖一，一個 Product 的機制全貌（11 月 11 日的圖，只換名字）。
- [diagrams/control-plane-revised.svg](diagrams/control-plane-revised.svg)：圖二，基礎的 System 契約（11 月 11 日的圖，換名字並調整）。

## 用詞與命名

- [rebuild-vocabulary.zh-Hant.md](rebuild-vocabulary.zh-Hant.md)：同詞異義表（v2 與使用者的回覆；v3 待寫）。
- [rebuild-naming.zh-Hant.md](rebuild-naming.zh-Hant.md)：命名（N17～N24；採 11 月 11 日命名規範的第 1 到 4 條）。

## 設計與判斷

- [rebuild-concept.zh-Hant.md](rebuild-concept.zh-Hant.md)：概念層的重構稿。
- [rebuild-judgment-rules.zh-Hant.md](rebuild-judgment-rules.zh-Hant.md)：舊設計去留的判斷機制。
- [rebuild-entity-order.zh-Hant.md](rebuild-entity-order.zh-Hant.md)：Entity 的順序。
- [rebuild-diagram-review.zh-Hant.md](rebuild-diagram-review.zh-Hant.md)：11 月 11 日三張圖的重新評審（用詞、思路、資料的流向）。
- [rebuild-1111-decisions-status.zh-Hant.md](rebuild-1111-decisions-status.zh-Hant.md)：11 月 11 日那批機制的決定在 gyo2 上的現況。
- [rebuild-time-design.zh-Hant.md](rebuild-time-design.zh-Hant.md)：時間、固定步、runner、跨時鐘事件的設計 v2（修訂 4）。主要的決定已定（第 0 節，使用者 2026-11-14）；第 2 節以後的選項表是決定之前寫的，留作紀錄。
- [kernel-nested-schedule-declaration.zh-Hant.md](kernel-nested-schedule-declaration.zh-Hant.md)：事前宣告（K0）：獨占 World 的 System 可以跑一張 schedule。使用者 2026-11-16 核准，#239 已實作。
- [time-types-declaration.zh-Hant.md](time-types-declaration.zh-Hant.md)：事前宣告（T1）：累加器與時間的型別。修訂 2 草稿，**由 time-events 宣告取代**（`v8-arch-gyo2-t1-closed-by-time-events`），留作紀錄。
- [nested-queue-spike-synthesis.zh-Hant.md](nested-queue-spike-synthesis.zh-Hant.md)：巢狀時延後的變更怎麼放——三個探索（兩個試做、一個掃描）的統合。草稿，待對抗檢查與使用者判。
- [stamp-order-spike-synthesis.zh-Hant.md](stamp-order-spike-synthesis.zh-Hant.md)：外層刪除、裡面用同一個編號生成——「用 stamp 分先後」三個探索的統合。草稿，待對抗檢查與使用者判。

## 計畫與批次的紀錄

- [transform-camera-declaration.zh-Hant.md](transform-camera-declaration.zh-Hant.md)：`v8-arch-gyo2-sdl-transform-camera`（SDL 線子批 X1：`transform`＋`camera`）的事前宣告（修訂 1，使用者 2026-11-23 動工前核准「照建議，第 1 題選乙，同意副本」）。
- [state-machine-declaration.zh-Hant.md](state-machine-declaration.zh-Hant.md)：`v8-arch-gyo2-state-machine` 的事前宣告（修訂 8，2026-11-22 核准）。
- [global-audit.zh-Hant.md](global-audit.zh-Hant.md)：全局計畫審計（2026-11-14）——這條線的起點。
- [rebuild-declaration.zh-Hant.md](rebuild-declaration.zh-Hant.md)：`v8-arch-rebuild` 的事前宣告（階段 R0～R6）。
- [rebuild-r0-carry-list.zh-Hant.md](rebuild-r0-carry-list.zh-Hant.md)：R0 帶過去清單（已核准）。
- [rebuild-r0b-report.zh-Hant.md](rebuild-r0b-report.zh-Hant.md)：R0b 的報告（刪除舊實現；第 3 節是 R0b-3 的判定變更）。
- [json-module-declaration.zh-Hant.md](json-module-declaration.zh-Hant.md)：`v8-arch-gyo2-json-module` 的事前宣告（JSON Module；修訂 1，使用者 2026-11-20 動工前核准）。
- [json-module-merge-approval.zh-Hant.md](json-module-merge-approval.zh-Hant.md)：`v8-arch-gyo2-json-module` 的合併前核准頁（判定的結果、判定 3 的更正、突變的數目、和宣告不同的地方）。
- [text-declaration.zh-Hant.md](text-declaration.zh-Hant.md)：`v8-arch-gyo2-sdl-t` 的事前宣告（SDL 線子批 T：text Module；修訂 1，使用者 2026-11-23 動工前核准「照建議，1 選丁，2 同意下載，3 門檻照建議」）。
- [text-merge-approval.zh-Hant.md](text-merge-approval.zh-Hant.md)：`v8-arch-gyo2-sdl-t` 的合併前核准頁。
- [logging-declaration.zh-Hant.md](logging-declaration.zh-Hant.md)：`v8-arch-gyo2-logging` 的事前宣告（第 6 組：日誌機制；修訂 1，使用者 2026-11-20 動工前核准「照建議，全部核准」；修訂 2 記日誌後退出、不去重；修訂 3 異常分級之後的改寫、日誌和 trace 分成兩個 Module）。
- [logging-merge-approval.zh-Hant.md](logging-merge-approval.zh-Hant.md)：`v8-arch-gyo2-logging` 的合併前核准頁。
- [trace-declaration.zh-Hant.md](trace-declaration.zh-Hant.md)：`v8-arch-gyo2-trace` 的事前宣告（修訂 1，使用者 2026-11-21 動工前核准「照建議，全部核准」）。
- [trace-merge-approval.zh-Hant.md](trace-merge-approval.zh-Hant.md)：`v8-arch-gyo2-trace` 的合併前核准頁。
- [os-wait-declaration.zh-Hant.md](os-wait-declaration.zh-Hant.md)：`v8-arch-gyo2-os-wait` 的事前宣告（core/time 的等待改呼叫 core/os 的 `WakeSignal`；W3 的分析；修訂 3，使用者 2026-11-21 動工前核准「照建議，全部核准，開始」）。
- [os-wait-merge-approval.zh-Hant.md](os-wait-merge-approval.zh-Hant.md)：`v8-arch-gyo2-os-wait` 的合併前核准頁。
- [os-wake-test-backoff-declaration.zh-Hant.md](os-wake-test-backoff-declaration.zh-Hant.md)：`v8-arch-gyo2-os-wake-test-backoff` 的事前宣告（WakeSignalTests「loses no signal」的退避：原案自旋 65536 次後才 yield；修訂 4 撤回：問題只在本機 Intel Mac 滿載時出現，CI 與 Linux 沒有；只拿掉 #289 的 Note）。
- [os-src-platform-declaration.zh-Hant.md](os-src-platform-declaration.zh-Hant.md)：`v8-arch-gyo2-os-src-platform` 的事前宣告（core/os 的平台程式集中到 `src/sys/`，macOS 與 Linux 不同的步驟走抽象類別 `Detail::Platform`；修訂 3）。
- [hook-flush-declaration.zh-Hant.md](hook-flush-declaration.zh-Hant.md)：`v8-arch-gyo2-hook-flush` 的事前宣告（hook 的紀錄在 schedule 跑完時套用；修訂 3，使用者 2026-11-15 核准）。
- [spawn-reject-declaration.zh-Hant.md](spawn-reject-declaration.zh-Hant.md)：`v8-arch-gyo2-spawn-reject` 的事前宣告（同步點丟掉不能生成的那一筆；修訂 2，使用者 2026-11-15 核准）。
- [kernel-app-boundary-declaration.zh-Hant.md](kernel-app-boundary-declaration.zh-Hant.md)：`v8-arch-gyo2-kernel-app-boundary` 的事前宣告（統合）：Kernel 只做 ECS、另立編排層、每層的職責清單、跨份的關聯表、確認清單 13 件；修訂 38，使用者 2026-11-17 核准（c1be70eb）。推導與四家對照在[設計稿](kernel-app-boundary-draft.zh-Hant.md)。
- [authority-module-declaration.zh-Hant.md](authority-module-declaration.zh-Hant.md)：`v8-arch-gyo2-authority-module` 的事前宣告（權威 Module：策略 interface、伺服器權威策略、代為登記、digest；不數寫者；邊界統合的子份）；修訂 4 與之後的改動，使用者 2026-11-17 核准（7f9bbb85）。
- [rollback-module-declaration.zh-Hant.md](rollback-module-declaration.zh-Hant.md)：`v8-arch-gyo2-rollback-module` 的事前宣告（回滾 Module：`RollbackStrategy` interface、第一版官方實作、P2P 佔位、要 Kernel 給的原語；邊界統合的子份，修訂 4 使用者 2026-11-17 核准（7dfd7167）；修訂 5、6 照第四輪檢查與窄複查改，使用者 2026-11-17 核准（9418fcf1））。
- [event-cursor-declaration.zh-Hant.md](event-cursor-declaration.zh-Hant.md)：`v8-arch-gyo2-event-cursor` 的事前宣告（事件：保留式改名 `Message` 與游標修剪，推送式 `Event`＋`Observer`；邊界統合的子份）；修訂 4 與修訂 5、6，使用者 2026-11-17 核准（758f651f）。
- [event-cursor-reeval-declaration.zh-Hant.md](event-cursor-reeval-declaration.zh-Hant.md)：`v8-arch-gyo2-event-cursor-reeval` 的事前宣告（被條件長期擋住或不常跑的 `Message` 讀者：查證後判為作者的問題，Kernel 不改，說明頁寫寫法、Kernel 加 RC 24）；修訂 3，使用者 2026-11-22 核准。
- [kernel-res-changed-declaration.zh-Hant.md](kernel-res-changed-declaration.zh-Hant.md)：`v8-arch-gyo2-kernel-res-changed` 的事前宣告（`Res`／`ResMut` 的 `Changed()`：比掛載的上次跑、寫入就算；同值不寫的規則登記 CCR-8）。使用者 2026-11-23 核准（3.1 選 (a)）。
- [kernel-summary-messages-declaration.zh-Hant.md](kernel-summary-messages-declaration.zh-Hant.md)：`v8-arch-gyo2-kernel-summary-messages` 的事前宣告（World 摘要與訊息：序號與游標是歷史的計數，回滾重跑或外部事件會讓同樣的狀態得到不同摘要）。使用者 2026-11-23 選 (b)。
- [kernel-relationship-declaration.zh-Hant.md](kernel-relationship-declaration.zh-Hant.md)：`v8-arch-gyo2-kernel-relationship`（子批 KR）的事前宣告（照 Bevy 把父子關係放進 Kernel：`ChildOf`／`Children`、兩邊一致的維護在套用步驟、連帶刪除、回滾與讀檔裡的父子）。使用者 2026-11-23 動工前核准（Q1～Q4 照建議）。
- [kernel-relationship-merge-approval.zh-Hant.md](kernel-relationship-merge-approval.zh-Hant.md)：KR 的合併前核准頁（R-1 同值再插入算不算拆開、條文的最終文字、和宣告的偏差、判定的變更、審查）。
- [restore-entry-declaration.zh-Hant.md](restore-entry-declaration.zh-Hant.md)：`v8-arch-gyo2-restore-entry` 的事前宣告（讀檔一律在新的 World、編號兩種寫法、hook 照常觸發；讀檔放編排層的生成世界）；修訂 11，使用者 2026-11-17 核准（ec31e863）。
- [threads-io-review-declaration.zh-Hant.md](threads-io-review-declaration.zh-Hant.md)：`v8-arch-gyo2-threads-io-review` 的重審報告兼事前宣告（threads 加 `WorkerPool`、`RoleThread` 不改名；io 拿掉沒有使用者的部分與 `Vfs`、補 `TooLarge` 與多執行緒讀的契約）；修訂 1，使用者 2026-11-22 動工前核准（74d38577）。
- [orchestration-skeleton-declaration.zh-Hant.md](orchestration-skeleton-declaration.zh-Hant.md)：`v8-arch-gyo2-orchestration-skeleton` 的事前宣告（編排層的骨架：登記單位、`Product`、遊戲初始化與每幀清單、runner、準備、替換與帶過去、動態插入的警告、Module／Plugin 登記檔、Kernel 拿掉開局與登記單位、scenario test 第一段）；修訂 7，使用者 2026-11-18 動工前核准。
- [orchestration-skeleton-merge-approval.zh-Hant.md](orchestration-skeleton-merge-approval.zh-Hant.md)：同一批的合併前核准資料（判定的結果、照 G-7 改寫的第 7.1、7.2 節、第 10 節的最終文字、和宣告不同的地方、要你判的一件）。
- [time-events-declaration.zh-Hant.md](time-events-declaration.zh-Hant.md)：`v8-arch-gyo2-time-events` 的事前宣告（第 3 批：時間 Module `engine/modules/world_time`、Kernel 拿掉時鐘、保留式 `Message` 與游標、推送式 `Event`＋`Observer`、編排層的預設骨架、scenario 第 2 段）；修訂 4，使用者 2026-11-19 動工前核准。
- [asset-declaration.zh-Hant.md](asset-declaration.zh-Hant.md)：`v8-arch-gyo2-asset` 的事前宣告（asset Module：作者只用 catalog 的 id、資產管理在 World 外、照 Bevy 的演算法、依賴由 Engine 推出）；修訂 3，使用者 2026-11-22 動工前核准（55a14825）；實作完成，使用者 2026-11-23 合併前核准；實作時的調整在第 30 節。
- [os-user-data-declaration.zh-Hant.md](os-user-data-declaration.zh-Hant.md)：`v8-arch-gyo2-os-user-data` 的事前宣告（core/os 加「這個使用者的資料目錄」：一個公開出口、三個平台注入 base；Q1 名字一層、Q2 新建的層 0700）；修訂 1，使用者 2026-11-22 動工前核准（「Q1 (b)、Q2 (a)，開始」）。
- [world-gen-merge-approval.zh-Hant.md](world-gen-merge-approval.zh-Hant.md)：同一批的合併前核准資料（守衛搬家的 1～5、和宣告的偏差、Design Invariants 的最終文字、設計審查與 §14.4 檢證的結論）；使用者 2026-11-22 核准「照建議，全部核准」。
- [world-gen-declaration.zh-Hant.md](world-gen-declaration.zh-Hant.md)：`v8-arch-gyo2-world-gen` 的事前宣告（第 5 批：生成世界——每個 World 建好就從「生成中」開始、生成期間每幀照跑、作者用 run 條件 `WhileGenerating()`／`AfterGenerating()` 分開生成與遊戲邏輯、沒人 `Hold()` 就結束；預設的實體配發器 `entity_ids` Module、`RangeAllotter` 搬出 Kernel）；修訂 2，使用者 2026-11-22 動工前核准（「照建議，全部核准」）。
- [audio-review-declaration.zh-Hant.md](audio-review-declaration.zh-Hant.md)：`v8-arch-gyo2-audio-review` 的重審報告與事前宣告（第 6 批 audio 重審：`Submitter` 不綁執行緒、時間點的邊界；修訂 2，使用者 2026-11-22 動工前核准「照建議，全部核准」；之後 `world_audio` 撤掉（`v8-arch-gyo2-world-audio-withdrawn`），作者的寫法併進 [audio.zh-Hant.md](audio.zh-Hant.md) 第 5 節，clip 的來源與音效 loader 歸 audio 那一側的小批）。
- [audio-review-merge-approval.zh-Hant.md](audio-review-merge-approval.zh-Hant.md)：同一批的合併前核准頁（拿掉 `gyo_audio_board_readers` 的 1～5、和宣告的偏差、Design Invariants 的最終文字、判定的結果）。
- [audio-clip-loader-declaration.zh-Hant.md](audio-clip-loader-declaration.zh-Hant.md)：`v8-arch-gyo2-audio-clip-loader` 的事前宣告（audio 那一側的小批：clip 改成共用的不可變值、拿掉 `ClipBank` 與 clip 格、混音器的參照留在提交口這一側；音效 loader `WavLoader` 與 `sound`／`wav` 的 Data Contract；`GYO::Audio` 連 `GYO::Asset`；修訂 2，使用者 2026-11-23 動工前核准「照建議，全部核准」）。
- [kernel-mutation-prune-report.zh-Hant.md](kernel-mutation-prune-report.zh-Hant.md)：`v8-arch-gyo2-mutation-prune` 的盤點與退役表（Kernel 突變清單 777 個的分類、退役候選與緩的面、增長判準草案；使用者 2026-11-18 核定，777 → 714）。
- [kernel-mutation-subsumption-report.zh-Hant.md](kernel-mutation-subsumption-report.zh-Hant.md)：同一批第 3 步的 subsumption 檢查（714 個逐個跑全部 Kernel 測試、最小集合 469、退 51 個；使用者 2026-11-19 核准，714 → 663）。
- [scripts/base_mutations.json](scripts/base_mutations.json)：GYO2 線 Base 的突變清單（`bs-`，`IdIssuer`／`CurrentThreadSerial`；2026-11-19 從 Kernel 的清單搬來，`v8-arch-gyo2-move-bs-mutants`）。

## dev_log

- [2026_11_14_arch_global_audit.zh-Hant.md](../../../dev_logs/2026_11_14_arch_global_audit.zh-Hant.md)：全局計畫審計。
- [2026_11_14_gyo2_mechanism_skeleton.zh-Hant.md](../../../dev_logs/2026_11_14_gyo2_mechanism_skeleton.zh-Hant.md)：機制的骨架跑通。
- [2026_11_14_gyo2_r0b3_ci.zh-Hant.md](../../../dev_logs/2026_11_14_gyo2_r0b3_ci.zh-Hant.md)：R0b-3（守衛與 CI 政策的調整），PR #225 併入。
- [2026_11_14_gyo2_initial_value.zh-Hant.md](../../../dev_logs/2026_11_14_gyo2_initial_value.zh-Hant.md)：漏斗工具 `Registrar::InitialValue`。
- [2026_11_14_gyo2_strict_order.zh-Hant.md](../../../dev_logs/2026_11_14_gyo2_strict_order.zh-Hant.md)：歧義的嚴格模式 `Registrar::RequireExplicitOrder`。
- [2026_11_15_gyo2_funnel_check.zh-Hant.md](../../../dev_logs/2026_11_15_gyo2_funnel_check.zh-Hant.md)：漏斗工具的對抗檢查與區規則，PR #229 併入（標記為需要產品測試）。
- [2026_11_15_gyo2_hook_flush.zh-Hant.md](../../../dev_logs/2026_11_15_gyo2_hook_flush.zh-Hant.md)：hook 的紀錄在 schedule 跑完時套用（清空輪、兩層上限、指紋升版）。
- [2026_11_15_gyo2_spawn_reject.zh-Hant.md](../../../dev_logs/2026_11_15_gyo2_spawn_reject.zh-Hant.md)：同步點丟掉不能生成的那一筆（三條斷言改成丟掉、留明細與總數、LIM-7、指紋升版）。
- [2026_11_16_gyo2_nested_schedule.zh-Hant.md](../../../dev_logs/2026_11_16_gyo2_nested_schedule.zh-Hant.md)：K0，獨占 World 的 System 可以跑一張 schedule（巢狀執行、`ExclusiveWorld<…>` 的閉包與帶寫入的宣告、開局由 Kernel 跑、指紋升版）。
- [2026_11_18_gyo2_kernel_subtract_structure.zh-Hant.md](../../../dev_logs/2026_11_18_gyo2_kernel_subtract_structure.zh-Hant.md)：Kernel 減法・結構段（改名 `WorldExecutor`、兩步登記、唯讀登記表、`ExclusiveWorld` 全讀寫與當場改、`KernelSettings` 可寫、拿掉執行期攔截與歧義偵測、執行緒改成綁執行緒的 Resource、指紋升版；S10 停下待定）。
- [2026_11_18_gyo2_orchestration_skeleton.zh-Hant.md](../../../dev_logs/2026_11_18_gyo2_orchestration_skeleton.zh-Hant.md)：編排層的骨架（四步：編排層本體、Kernel 減法、登記檔、scenario 第一段；進行中）。
- [2026_11_19_gyo2_mutation_prune.zh-Hant.md](../../../dev_logs/2026_11_19_gyo2_mutation_prune.zh-Hant.md)：Kernel 突變清單的盤點與退役（第 2 步：直接退 41、合併進接手 22、改寫 3；第 3 步：subsumption 檢查後合併 51 個；777 → 663）。
- [2026_11_19_gyo2_summary_padding.zh-Hant.md](../../../dev_logs/2026_11_19_gyo2_summary_padding.zh-Hant.md)：World 摘要的 padding（Kernel 的 `RangeAllotter` 補 0 欄位與編譯期檢查；未完全解決，正式解法逐型別雜湊在第 4 批）。
- [2026_11_20_gyo2_time_events.zh-Hant.md](../../../dev_logs/2026_11_20_gyo2_time_events.zh-Hant.md)：第 3 批，時間 Module＋事件（`Message` 與游標、拿掉 Kernel 的時鐘、編排層的預設骨架、`engine/modules/world_time`、推送式 `Event`／`Observer`、scenario 第 2 段；等合併前核准）。
- [2026_11_16_gyo2_module_plugin_definition.zh-Hant.md](../../../dev_logs/2026_11_16_gyo2_module_plugin_definition.zh-Hant.md)：Module／Plugin 的定義改成「誰維護、介面是誰的」，AGENTS §6 與 v2 文件對齊。
- [2026_11_16_gyo2_test_debts.zh-Hant.md](../../../dev_logs/2026_11_16_gyo2_test_debts.zh-Hant.md)：Kernel 測試的三筆舊帳（四個對不上的突變、刪除實體時 OnRemove 的先後、還沒搬的 world-exec 突變）。
- [2026_11_20_gyo2_json_module.zh-Hant.md](../../../dev_logs/2026_11_20_gyo2_json_module.zh-Hant.md)：JSON Module（`v8-arch-gyo2-json-module`）。
- [2026_11_23_gyo2_sdl_t_text.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_sdl_t_text.zh-Hant.md)：SDL 線子批 T，text Module（`v8-arch-gyo2-sdl-t`）。
- [2026_11_20_gyo2_error_levels.zh-Hant.md](../../../dev_logs/2026_11_20_gyo2_error_levels.zh-Hant.md)：異常分級（Kernel 不再印錯誤、編排層不再記警告、P12、P15；級別與命名的條文草稿）。
- [2026_11_20_gyo2_logging.zh-Hant.md](../../../dev_logs/2026_11_20_gyo2_logging.zh-Hant.md)：日誌機制（`v8-arch-gyo2-logging`；trace 另一個 Module）。
- [2026_11_22_gyo2_exchange_guards.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_exchange_guards.zh-Hant.md)：第 5 批第 0 步，Kernel「拿掉的名字不回來」與權威 Module 的連結集（PR #268 併入）。
- [2026_11_22_gyo2_exchange.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_exchange.zh-Hant.md)：第 5 批，Product 一份的共用處、交換函式的登記、多個 World 的執行緒與 Extract 的契約（`v8-arch-gyo2-exchange`，PR #270）。
- [2026_11_21_gyo2_trace.zh-Hant.md](../../../dev_logs/2026_11_21_gyo2_trace.zh-Hant.md)：trace（`v8-arch-gyo2-trace`）。
- [2026_11_21_gyo2_stale_mutants.zh-Hant.md](../../../dev_logs/2026_11_21_gyo2_stale_mutants.zh-Hant.md)：os-subsystem 清單裡 6 個在基準上跑不起來的突變（改寫 6、敵對式檢查後新增 4；`v8-arch-gyo2-stale-mutants`）。
- [2026_11_21_gyo2_os_wait.zh-Hant.md](../../../dev_logs/2026_11_21_gyo2_os_wait.zh-Hant.md)：core/time 的等待改呼叫 core/os（`v8-arch-gyo2-os-wait`）。
- [2026_11_21_gyo2_os_src_platform.zh-Hant.md](../../../dev_logs/2026_11_21_gyo2_os_src_platform.zh-Hant.md)：core/os 的平台程式集中到 `src/sys/`（`v8-arch-gyo2-os-src-platform`）。
- [2026_11_22_gyo2_authority_rollback.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_authority_rollback.zh-Hant.md)：第 4 批，權威 Module＋回滾 Module（`v8-arch-gyo2-authority-rollback`；Kernel 的區、寫者數量、C 指紋、Z6、Z8、D-E15、D-E16 拿掉，判定 5 在 Linux 實機通過；第 8 步的審查與完整突變後，使用者 2026-11-22 核准）
- [2026_11_22_gyo2_namespace_root.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_namespace_root.zh-Hant.md)：namespace 的根統一成 `GYO::`，`naming_guard` 守 N17 的根（`v8-arch-gyo2-namespace-root`）。
- [2026_11_22_gyo2_stale_cleanup.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_stale_cleanup.zh-Hant.md)：三個 include 守衛補 `..`、刪 15 份 master 時代的突變清單、文件路徑補層名（`v8-arch-gyo2-stale-cleanup`）。
- [2026_11_22_gyo2_naming_cleanup.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_naming_cleanup.zh-Hant.md)：命名收尾（`v8-arch-gyo2-naming-cleanup`；`naming_guard` 搬到 `tests/common/naming/`、共用讀法與 `gyo_expect_links` 搬到 `tests/common/support/`、`engine/` 改短名、測試輔助 namespace 收斂成 `GYO::Test::<單位>`、`Detail` 對齊 N13）。
- [2026_11_22_gyo2_n13_short_names.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_n13_short_names.zh-Hant.md)：`engine/` 剩下的全名照 N13「引用別的單位」改成短名（`v8-arch-gyo2-n13-short-names`；IO 指自己的拿掉、Kernel 寫 `Engine::Base::`、`HashValue` 寫 `Kernel::Hasher`；巨集保留全限定）。
- [2026_11_22_gyo2_world_gen.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_world_gen.zh-Hant.md)：第 5 批 生成世界（生成中、`WorldGeneration`、`WhileGenerating()`／`AfterGenerating()`）與預設的實體配發器 `entity_ids`（`RangeAllotter` 搬出 Kernel）；使用者 2026-11-22 合併前核准，合併待使用者說。
- [2026_11_22_gyo2_au_z6_survivor.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_au_z6_survivor.zh-Hant.md)：權威 Module 的突變 `au-z6-skipped-counted` 存活的原因（61d6f72c 起突變指定的 AU-E8 一例因 Resource 輸入另有歷史而失去檢出力）與修正（突變改接既有的 `every mount skipped`；`v8-arch-gyo2-au-z6-survivor`）。
- [2026_11_22_gyo2_audio_review.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_audio_review.zh-Hant.md)：第 6 批 audio 重審。
- [2026_11_22_gyo2_event_cursor_reeval.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_event_cursor_reeval.zh-Hant.md)：事件游標的重評——帶條件或不常跑的讀者交給作者、System 契約說明頁第 15 節、Kernel RC 24、CCR-7（`v8-arch-gyo2-event-cursor-reeval`）。
- [2026_11_22_gyo2_asset.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_asset.zh-Hant.md)：asset Module（`asset` 函式庫＋`world_asset`；catalog 的 Data Contract、World 外的 `AssetStore`、`AssetView` 與套用點、突變 108 個全部 killed；`v8-arch-gyo2-asset`，使用者 2026-11-23 合併前核准）。
- [2026_11_22_gyo2_threads_io_review.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_threads_io_review.zh-Hant.md)：threads、io 重審（`v8-arch-gyo2-threads-io-review`；`WorkerPool`、io 縮減、`TooLarge`、io DI 5 刪除與 DI 6）。
- [2026_11_22_gyo2_os_user_data.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_os_user_data.zh-Hant.md)：core/os 加 `OS::Files::UserDataDirectory`（`v8-arch-gyo2-os-user-data`；三個平台經 `Detail::Platform::UserDataBase()` 注入 base、組合只寫一次；名字一層、新建的層 0700）。
- [2026_11_22_gyo2_state_machine.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_state_machine.zh-Hant.md)：狀態機 Module（第 5 批，宣告修訂 8）。
- [2026_11_22_gyo2_restore_impl.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_restore_impl.zh-Hant.md)：第 5 批 讀檔的實作（讀檔宣告修訂 12：沿用既有案例、補四個生成期間的讀檔案例、說明頁第 15 節）與存檔版本 `save_version`（純程式庫、預設不帶）。
- [2026_11_23_gyo2_kernel_res_changed.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_kernel_res_changed.zh-Hant.md)：Resource 的變更偵測——`Res`／`ResMut` 的 `Changed()`、Kernel DI-KS 28、System 契約說明頁第 18 節、CCR-8。
- [2026_11_23_gyo2_kernel_summary_messages.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_kernel_summary_messages.zh-Hant.md)：World 摘要版本 4——訊息只收留著的筆數與內容，不收寫入序號、紀錄的序號與游標表（使用者選 (b)；Kernel DI-KS 10、27，RC 25）。
- [2026_11_23_gyo2_compile_fail_judgement.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_compile_fail_judgement.zh-Hant.md)：負向編譯樣本的判法——「靜態斷言失敗＋標記在同一行」做成共用函式 `tests/common/support/ExpectStaticAssert.cmake`，Kernel、world_logging、world_asset 三處改用（asset 宣告第 15 節第 12 項）。
- [2026_11_23_gyo2_audio_clip_loader.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_audio_clip_loader.zh-Hant.md)：audio 那一側的小批（clip 改成共用的不可變值、拿掉 `ClipBank` 與 clip 格、參照表與回收環；`WavLoader` 與 `sound`／`wav` 的 Data Contract；`GYO::Audio` 連 `GYO::Asset`）。
- [2026_11_23_gyo2_os_test_timeout.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_os_test_timeout.zh-Hant.md)：os 測試在本機偶爾跑很久或逾時的原因（`v8-arch-gyo2-os-test-timeout`；加逐案計時的觀測；時間花在「loses no signal」案例自己的 signaller 每回合 yield、負載下讓出 CPU，不在 WakeSignal；改不改測試交使用者）。
- [2026_11_23_gyo2_sdl_transform_camera.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_sdl_transform_camera.zh-Hant.md)：SDL 線子批 X1，`transform`＋`camera` Module（`v8-arch-gyo2-sdl-transform-camera`；突變 45/45、合併前核准頁 [transform-camera-merge-approval.zh-Hant.md](transform-camera-merge-approval.zh-Hant.md)）。
- [2026_11_23_gyo2_kernel_relationship.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_kernel_relationship.zh-Hant.md)：Kernel 的關係與父子（子批 KR）——`ChildOf`／`Children`、第 5b 步、連帶刪除（深度優先、先拆再刪）、`Restore` 的兩件、`RejectedRelationship`；Kernel DI-KS 29～33、RC 26、27；說明頁第 19 節。
- [2026_11_23_gyo2_build_games_dir.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_build_games_dir.zh-Hant.md)：建置層認 `games/`，建置與 CI 的 app 改叫 game（`projects.csv`→`games.csv`、`GYO_GAMES`、KIND game、打包清單 4、工具描述 2、身分標頭改巨集）；四個守衛的掃描根；複製整合的 mirror 修正；LIM-9。
- [2026_11_23_gyo2_os_wake_test_backoff.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_os_wake_test_backoff.zh-Hant.md)：「loses no signal」退避的調查與撤回（`v8-arch-gyo2-os-wake-test-backoff`；65536 次自旋在 6 次參考量測中讓本機 Mac 的拖延消失（改後都 ≤ 1.4 s），但閒置時檢出力可能變弱；問題在 CI 與 Linux 上沒有，使用者決定撤回，只拿掉 #289 在案例裡的 Note）。
- [2026_11_23_gyo2_mech_cleanup.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_mech_cleanup.zh-Hant.md)：累積的機械式清理（`v8-arch-gyo2-mech-cleanup`；master 路徑的突變 74 個退役（其中 `tt2-flush-no-wait` 改寫進 logging 清單）、確認前批已做掉的三件、兩份 dev_log 補已合併）。
- [2026_11_23_gyo2_kernel_diagnostics_exchange.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_kernel_diagnostics_exchange.zh-Hant.md)：Kernel 診斷紀錄接上交換函式「出」（`WorldLogging::ForwardKernelDiagnostics`；O-3、O-11、O-12、LG-1 對「誰取」改成一致，使用者同意修訂 1）。

## 沒有搬過來的

- 圖三 `networking-systems.svg` 沒有改過，留在 [`../../plans/kernel/diagrams/`](../../plans/kernel/diagrams/)；網路的圖等機制完成後再處理（`v8-arch-gyo2-network-diagram-after-mechanism`）。
- 規格（`spec.zh-Hant.md`）、mechanism、blueprint、networking、spike-report 等 11 月 11 日到 13 日的文件留在 `plans/kernel/`，本資料夾的文件引用它們時連到那裡。
- [recovered-from-stale-cards.zh-Hant.md](recovered-from-stale-cards.zh-Hant.md)：從前提已變的任務卡（狀態機、重新評估 Loading）回收的資源，以及哪些舊設計不要帶進來（2026-11-17）。
- [rebuild-batch-plan.zh-Hant.md](rebuild-batch-plan.zh-Hant.md)：GYO2 批次表（2026-11-17 重排，取代時間設計第 11 節的表）：設計段（2026-11-17 完成）、統合核准之後的實作段、不擋路的小事。
