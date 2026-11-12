# 交接卡：產品線的 kernel 改造前置（`v8-product-kernel-prep`）

建立：2026-11-12（架構師會話）。Owner：`object_fps_pvp`（內容歸產品）。**執行者：架構師開的任務卡**（使用者 2026-11-12：v8 產品會話只處理 v8 版本，改造與產品遷移完成後再接手）。本卡是計畫＋記錄器：現況節由執行的任務卡更新；每批照 AGENTS §14 寫事前宣告（`v8-product-kernel-prep-<批>-declaration.md`）、經使用者核准再開工；完成時 dev_log 與該批同一變更。
架構師線是 Architecture Delta 與區判定的審核者；kernel 本體與 spec 修訂不在本卡。

## 前提

- 2026-11-11 功能線停止（停法 (a)）：`hitbox-rewind`、`pm-detect`、`presentation-smoothness`、`movement-fix`、`big-map` 仍不開。本卡是改造的前置，不是功能；**不改玩法、不改 wire、不改 digest 的語義**。
- 使用者 2026-11-12 同意分出並行線；產品線這兩批排在 J-17 掃描器線（`docs/architecture/plans/kernel/j17-scanner.zh-Hant.md`）之後：第一批不依賴它，第二批要等它的基線報告。
- 出處：audit `docs/architecture/plans/kernel/audit.zh-Hant.md` §1.3（pvp 逐模組的留／包／重寫與區）、§4.1（Body 拆欄與 Life 兩寫者）、§3.4／spec G.4（五處現況違反：牆鐘與 IO 執行緒碰權威）；`docs/architecture/target/naming.zh-Hant.md` 第 4 條（決定性區＝位置不是名字）；entity-model §5.2（目標形狀）。

## 第一批：`v8-product-kernel-prep-detect`（現在可開；只加偵測與固定現況，不改設計）

目的：把遷移批要決定的三件事，先變成**可量、可判**的現況紀錄（「量不到先建檢測」）。遷移批據此寫事前宣告，而不是憑讀碼推測。

| 項 | 現況（出處） | 要加的東西 | 不做的事 |
|---|---|---|---|
| D1 Life 的寫者與時序 | 死亡在 `ResolveActions` 內同 tick 讀後寫（`PvpMatch.cpp:303`）、復活在 `:328`；audit §4.1 說「漏斗放下一 tick 會讓死亡晚一 tick 出現在 snapshot」但**無結論** | 特徵測試（characterization）固定今天的時序：擊殺發生的 tick T，snapshot(T) 的 `lifeState` 已是 Dead、復活在哪個 tick 變 Alive；用現有 fixture 與 `AuthorityDigest` 的方式寫，數值從實跑得到、不手算 | 不選漏斗、不拆欄 |
| D2 牆鐘與 IO 執行緒碰權威的讀點 | spec G.4 五條：`now_()` 在 `MatchRuntimeHost.cpp:218、445、483、604、670`，`SubmitInput` 的 `RequestMovementResync`（`:249-253`）、`match_.Snapshot()`（`:178`）、`IpcHost.cpp:645、685` | **棘輪守衛**（ratchet）：一個 Python ctest 列出 `src/Pvp` 內允許的牆鐘讀點與 IO 執行緒碰權威的呼叫點（白名單＝今天這幾處，以「檔＋函式名＋呼叫形狀」描述，不用行號），新增一處＝失敗；遷移批每搬走一處就從白名單刪。先用正例確認掃描有效（`git grep -P` 或 Python，不用 BSD grep -P） | 不搬、不改 `Movement.cpp`（G.4 第 3 條：它本身乾淨） |
| D3 `connectionQualityFailures` 的寫點 | 進 digest，但今天只寫在 snapshot 複本上（audit §4.1） | 特徵測試固定「寫在哪個物件、哪個時點進 digest」 | 不改歸屬 |

判定（事前宣告再補數字）：D1、D3 的特徵測試在今天的程式上**過**，且對應的突變（把死亡改成下一 tick 套用；把 failures 寫到另一處）會**失敗**；D2 的守衛對白名單過、對合成的新增讀點失敗；34 個 golden、`AuthorityDigest` 全部不變；pvp 既有測試全過；`engine/`、`framework/`、`tests/common/`、`build/`、`.github/` 零修改。
估時 4～6 小時。模型／檔位建議：下段，Opus／high；D2 的掃描正規式 xhigh（只這段）。（實際：Fable 5.1／high，D2 段 xhigh；golden 實為 37 行，上表「34」是舊數，以 37 為準。）

## 第二批：`v8-product-kernel-prep-zones`（等 J-17 掃描器的基線報告）

目的：照 naming 第 4 條，把 (c) 的程式集中到產品的一個 namespace／目錄，(b) 到 `Present`／`Ingress` 等，讓 J-17 的掃描有對象、讓遷移批每次只動一個區。**只搬位置與整數化時間型別，不改行為**。

- 區的來源：audit §1.3 的判定——留（(c) 純函式：`src/Collision`、`src/Gameplay`、`StepMovement`／`Movement`、P9 射擊／姿勢／動畫的 System 本體、P13 內容身分）→ `Sim`；呈現 P10、`src/App` → `Present`；診斯 P8／P12 → 服務，位置由產品線提、架構師核；P1～P6、P11 是重寫對象，**不搬**（遷移批重寫時直接落到新位置）。
- 時間型別：spec J-17 規則 1 說 (c) 檔不 include `<chrono>`、時間用整數 ns（今天 `Combat.hpp`、`Movement.hpp` 的型別用法要改）。這是本批唯一的程式改動（型別，不是邏輯）；digest 與 golden 必須不變，否則停下報告（那表示型別轉換改了數值）。
- 驗收：J-17 基線報告在 `Sim` 目錄的命中，搬完後規則 1、2、4、6 歸零（規則 3 第三方全域與規則 5 候選列的殘留逐條列理由）；34 個 golden 與 `AuthorityDigest` 不變；pvp 測試全過；L1 三平台綠；共通層零修改。
- Delta：namespace 改名若影響 `tests/object_fps_pvp` 以外的任何地方（例如 `build/acceptance/object_fps_pvp` 的 include），列出來；影響到 `engine/`、`framework/` 就是 Delta，停下交架構師。
估時 8～16 小時（搬檔多、每步跑測試）。模型／檔位建議：下段，Opus／high；審查者不同模型／high，重點是「搬錯區」與「型別轉換改了數值」。

## 範囲の限界（兩批共通）

- 不做遷移批的設計決定（Life 的漏斗形狀、Body 拆欄、`HostClockSample`、Ingress／Judge／Egress 的拆法）；本卡只提供它們的現況量測與整理後的目錄。
- 不碰 Match／Client 的 host 骨架（P2、P11）、不碰 wire（pv8 設計與帳本、兩通道已暫停）。
- 第一批的守衛白名單是**現況的紀錄**，不是許可：寫進白名單不代表那處合規。

## 與其他線的接點

- J-17 掃描器線：第二批的驗收工具；基線報告出來前第二批不開。清單格式 v1 由架構師核定後，產品線手寫 pvp 的 (c) 檔清單給掃描器。
- 架構師線：區判定有疑義、搬動碰到共通層、golden 或 digest 改變 → 停下回報。
- 圖書管理員：dev_log 與本卡現況由產品線寫；缺漏由圖書管理員點檢。

## 開場白（任務卡的 prompt 以此為底；會話確認職責後等「開始」）

> 你是 GYO-Engine 架構師開出的任務卡，在 `object_fps_pvp` 上做 kernel 改造的前置（v8 產品會話不參與）。交接卡：`docs/object_fps_pvp/plans/v8/kernel-prep.zh-Hant.md`；出處 `docs/architecture/plans/kernel/audit.zh-Hant.md` §1.3／§4.1／§3.4、`spec.zh-Hant.md` G.4、`docs/architecture/target/naming.zh-Hant.md` 第 4 條。功能線仍停（停法 (a)），只做卡上兩批。先讀卡、寫第一批的事前宣告給使用者核准，再開始。工作分支從 origin/master 開，HANDOFF（`docs/object_fps_pvp/plans/v8/HANDOFF.md`）每批開始與停止時更新。

## 現況（產品線更新）

- 2026-11-12 交接卡建立（架構師）。未開始。
- 2026-11-12 第一批 `v8-product-kernel-prep-detect` 完成（任務卡會話，實作約 4 h）：[事前宣告](v8-product-kernel-prep-detect-declaration.md)、[dev_log](../../../dev_logs/2026_11_12_pvp_kernel_prep_detect.zh-Hant.md)。D1 固定 T=35／R=215（R−T=180，Match 層與 host 層相同）；D2 守衛 `tests/object_fps_pvp/test_kernel_prep_sites.py` 白名單 47 列（W 12、G 13、A 22）；D3 固定 failures 在 kW 當 tick 進發布快照、Match 的 `Snapshot()` 恒 0。突變 6／6 killed，golden 37 不變，pvp 57 測試全過，共通層零修改。回報架構師：audit 未列的讀點（dev_log「spec G.4 補列候選」）。golden 數改為 37。L1 待 PR。
- 2026-11-12 第二批 `v8-product-kernel-prep-zones` 完成（任務卡會話，實作約 9 h；估 16 h）：[事前宣告](v8-product-kernel-prep-zones-declaration.md)（含「宣告錯誤的修正」與五項修訂）、[dev_log](../../../dev_logs/2026_11_12_pvp_kernel_prep_zones.zh-Hant.md)。Sim 30 檔＋Present 17 檔搬動、`Sim/PlayerPose.hpp` 拆出、`inline namespace sim`、`Combat.hpp` 的時間型別整數 ns 化、liveness 二常數搬 `ConnectionLiveness.hpp`；三份清單歸 `tests/object_fps_pvp/`（確認／候選／rewrite），共通層只刪基線。判定：確認清單 26→**7**（逐行等於釘住的殘留；ZS2～ZS6 為 0）、兩樹比對 42 情境 0 差異、golden 37 零差、`ctest -L pvp` 61／61、D2 白名單 47 列零變動、突變 4／4＋手動 1 killed、審查（Opus／high）must 1 已修。回報架構師：`ArenaMesh` 疑為 P13 誤判、App 內容定義的區待判、`Combat.hpp` 與 audit P1 分組不同。L1 待 PR。
