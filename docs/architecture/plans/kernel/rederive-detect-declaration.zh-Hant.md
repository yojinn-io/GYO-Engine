# 事前宣告：`v8-arch-rederive-detect`（凍結 World 資料模型，檢測先行）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：草稿，待使用者核准（含第 6 節的門檻與第 9 節各題）。**

出處：[全局審計](global-audit.zh-Hant.md) 第 5 節的「中」選項；使用者 2026-11-14 的四個決定（kernel `HANDOFF.md` 決定行 `v8-arch-engine-scope-general`、`v8-arch-gyo2-in-repo`、`v8-arch-single-player-no-complexity`、`v8-arch-rederive-medium`）。

使用者原話：
> 我的目標依然是通用引擎，但是如何強化 FPS、PVP 不影響其他通用遊戲的設計製作，其實機制是沒問題的。例如我做一個 2D 橫版通關遊戲，Engine 的機制也必然要支持。
> 其實就是 GYO2，但是還是放在 GYO 的 git repo 中。
> 能支持，並且不要增加複雜度。只要作者和玩家感受不到差異，多些性能開銷實際上沒有問題。
> 選這個，其實現在有壞味道，但基礎邏輯還沒問題。

## 1. 目的（Feature → Contract → Architecture）

- **Feature**：通用引擎。三類產品用同一套 Kernel 機制：
  1. PvP：object_fps_pvp 的 Match 與 Client（已有）。
  2. 2D 橫版通關單機：使用者 2026-11-14 點名的例子，目前沒有產品，用第 5.2 節的候選 Feature 清單代表。
  3. Editor 與四個工具：voxel_map_editor、model_viewer、audio_editor、ui_editor（已有）。
  強化 FPS 或 PvP 不得讓 2、3 類的作者多做事；作者與玩家感受不到差異即可，多一些性能開銷可以接受。
- **Contract**：本批不改任何契約。產出的是判斷用的兩份表與量測。
- **Architecture**：本批不改架構。只凍結第 2 節列出的部分，並起草第 4 節的兩條 RC，交使用者同意後另批落字。

## 2. 凍結（「小」的部分）

| 對象 | 處置 |
|---|---|
| PR #218 world-exec | 不合併，保留為證據；關不關由使用者另定 |
| DI-KS 3 的 W-1（容器依 id 排序）、DI-KS 4 的配發策略與「世代＝重建」、DI-KS 9 的「依 id 套用」與「hook 下一窗」、RC 6 的處置句、Delta-3（World 重用 entity_store） | 條文不改，但不再往上積：任何新批不得以它們為前提 |
| zone、role、rollback、ingress、rollbackSafe、notInRollback、digestSet、cFingerprint、`ExportZoneCSources`、`DeclaredEdge.dataflow` 這類 Kernel API | 不再新增使用處；已合併的不動 |
| 遷移計畫 world-exec 以後依賴 World 的各批 | 暫停 |
| 卡 01（plan-dryrun）、卡 02（預算重核）、卡 03（runtime-composition DI 落字）、卡 05（spec v0.5）、卡 08（cFingerprint patch）、PR #219（B1）的後續 | 暫停，等本批結果；分支與 patch 都保留 |
| json-adapter、pvp-baseline | 不依賴 World、不新增 Kernel 概念，可以照計畫繼續 |

## 3. 流程規則（已由 #222 落地，本批不另起草）

全局審計第 5.3 節要的兩條規則，已由風紀委員的 PR #222（`v8-rules-foundation-option-criteria`，2026-11-14 合併）寫進 AGENTS §4：
- 基盤 Architecture 的選項要附系譜、被否決的替代案與決勝理由、需要它的 Feature 與 Product。
- 不以「不必改既有的 Test、守衛、golden、突變」當決勝理由。
- 交給使用者前，由作者以外的模型的 Agent 確認。

細目寫在 `docs/architecture/README.md` 的〈Batch declaration checks〉，另一個架構師會話在分支 `claude/arch-batch-option-checks` 處理。本批的選項照這兩處執行。

## 4. 兩條置換條件（草稿，Architecture Delta，要使用者同意）

加在 `target/kernel.zh-Hant.md` 的 RC。本批只起草，不落字。

- **RC-出處**：Kernel 的一條規則或一個公開欄位，只有一類產品需要，而且對其他類有強制成本時觸發。強制成本指無預設的宣告、啟動錯誤或執行期分支。觸發時這一項要重新推導：移到 plugin、改成可選，或刪除。量法：第 5.1 節的表，每次改 Kernel 的批都更新。
- **RC-同構**：遷完一個 host 後，新 Plan 的 System 序列和舊迴圈的呼叫序列幾乎一一對應，交換點和舊 mutex 也一一對應，而且這批沒有任何「新性質」的判定時觸發。觸發時標為「重新包裝」，停下回報。只用於 Engine、Kernel 與 Host 的契約，不用於產品的 System 清單。

## 5. 檢測

### 5.0 新舊框架地圖（第一步，其餘檢測以它為底）

使用者 2026-11-14：「但是實際上現在就是全停的情況，但是架構上我不知道現在混亂程度有多少。現在新舊框架混在一起，而且我怕你自己也區分不開。」

這張地圖要機械產生、標準事先寫定，給使用者與各會話當同一份參照，不靠架構師的判斷。三張表：

1. **歸屬表**：每個程式目錄與每份架構文件，各屬哪一代。
   - **新**：`engine/kernel/`、`docs/architecture/target/`、`docs/architecture/plans/kernel/` 的規格與計畫、`spikes/kernel/`。
   - **舊**：其餘 `engine/` 模組、`framework/`、`apps/`、`tools/`、`services/`、頂層 `docs/architecture/*.md`。
   - **過渡**：新的部分直接重用或改寫自舊的，例如 `engine/entity_store`、target 中標「相容 legacy」的 DI、a6 重標過的 legacy 條文。

   判定依據只用可掃描的事實：目錄、Legacy 標記、target README 的對照欄。
2. **跨代引用表**：
   - 新的程式對舊的依賴：`#include`、CMake 連結。
   - 新的文件對舊的引用：檔名、條文代號、類別名。
   - 舊的對新的引用。

   逐條列出，標明這個引用是「重用」「理由」還是「對照」。全局審計已知新文件對 legacy 的反向引用有 91 處（legacy-impact），以它為正例。
3. **同詞異義表**：新舊兩代都用、意思不同的詞，例如 Runtime、Host、World、Resource、Link、Domain、Stage、Plugin、Component、Session。
   - 每個詞寫出舊義的出處（類別或檔:行）與新義的出處（target 或 spec 條文）。
   - 再列出新文件中用到這個詞、但上下文其實是舊義的地方。

   這是混淆最容易發生、也最難自己察覺的地方。

量出的「混亂程度」，看結果前定：
- 過渡項目數。
- 新→舊引用中「理由」類的件數。
- 同詞異義中被誤用的件數。

只記錄、不判門檻，結果交使用者看。

### 5.1 概念出處表

- **對象**：
  - target 的全部 DI 與 RC。
  - spec 的規則，以 PvP 滲入檢查的兩份盤點為底稿，共約 269 列。
  - master 上 Kernel 的公開 API：`Declarations.hpp`、`Registry.hpp`、`Plan.hpp`、`Access.hpp`、`SystemTraits.hpp`。
  - PR #218 的 World 與參數的公開 API。
- **欄**：

  | 欄 | 內容 |
  |---|---|
  | 項目 | 條文或欄位 |
  | 需要它的產品類別 | 第 1 節的三類，可多選；要附 Feature 或出處 |
  | 對其他類的強制成本 | 無、宣告、啟動錯誤、執行期分支、性能 |
  | RC-出處 | 是否觸發 |
  | 全局審計的類別 | R、O-結、O-理、P-範、S、? |
  | 處置方向候選 | 留 Kernel、移到 plugin、改成可選、刪 |

- 判斷「需要」的根據只用三種：名指的 Feature、參考實作的對應件（附檔:行）、已觀測的壓力。只有「Bevy 有」不算（AGENTS §2）。

### 5.2 宣告級試跑

- **兩個對象**：
  1. **一個工具**：建議 voxel_map_editor。它是最大、最近仍在開發的工具；也可以改成 ui_editor（第 9 節第 3 題）。
  2. **2D 橫版通關**：候選 Feature 清單是架構師依使用者的例子提出的，要使用者確認或修改（第 9 節第 2 題）：
     - 玩家左右移動與跳躍（鍵盤或手把）
     - 平台與地形碰撞
     - 敵人巡邏與接觸傷害
     - 拾取物（金幣、回血）
     - 關卡載入與切換
     - 存檔點與重生
     - 鏡頭跟隨
     - HUD（血量、分數）
     - 音效
     - 暫停
- **做法**：
  1. 先寫出「一般作者」的元件、Resource、System 清單。不看 GYO 的規則，照 Bevy 的寫法。工具從它的現有程式讀出；2D 從上面的清單推出。
  2. 再把清單登記到 master 上的 Kernel：Registry、Scheduler、finish、匯出 Plan。System 本體是空的，只回 Ok。
  3. 只記錄「為了讓它通過」要多寫或改寫的東西。
- **放置**：`spikes/kernel-rederive/`，比照 `spikes/kernel` 獨立建置。要改 `spikes/` 以外的建置檔才能接上 Kernel 時，停下回報，改提「`tests/` 下不進 ctest 的記錄執行檔」。

## 6. 量測與門檻（看結果前宣告，要使用者核准；AGENTS §14.1）

| # | 量什麼 | 量法 | 建議門檻 |
|---|---|---|---|
| M1 | PvP 或決定性概念的強制宣告數 | 作者不寫 zone、role、rollback、ingress 等 PvP 或決定性概念時，啟動錯誤的件數；加上為了消除它們要多寫的宣告數 | **0**（「作者感受不到差異」） |
| M2a | 因 Kernel 規則而被迫合併的 System 數 | 一般作者清單中的 System，因預設 Exclusive、S-1（無捕獲）、無資料驅動 spawn 等規則，必須合併或改形的件數，逐件記規則代號 | **0** |
| M2b | Shared 標記數 | 多寫者的型別必須標 Shared 的件數 | 只記錄、不判。Shared 是一般的單一寫者概念，不是 PvP 概念 |
| M3 | 宣告的行數 | Kernel 登記碼的行數，對照 Bevy 寫法的行數 | 只記錄、不判 |
| M4 | 概念出處表的觸發數 | 5.1 表中 RC-出處觸發的列數 | 只記錄；全部進第 7 節的範圍 |

**判定規則**（看結果前定）：
- 任一對象的 M1 或 M2a 超過門檻時，造成它的每一條規則都進入重新推導的範圍。
- 兩者都在門檻內時，重新推導的範圍只有 5.1 表中 RC-出處觸發、而且全局審計判 O-結 的列。
- 結果與判定規則不合時，不重跑、不放寬門檻，先回報。

## 7. 產出

- 報告 `docs/architecture/plans/kernel/rederive-detect-report.zh-Hant.md`，內容：
  - 件數の突き合わせ
  - 5.0 的三張表與混亂程度
  - 5.1 的表
  - M1～M4
  - 依判定規則得出的重新推導範圍，與下一批的形狀
- spike 程式 `spikes/kernel-rederive/`。
- 證據放在主 checkout 的 `build/evidence/rederive-detect/`。
- dev_log、kernel HANDOFF 與 README。

## 8. 編制與估時

上段，因為涉及 Design Invariants 與架構判斷。所有 agent 都明寫模型與檔位。

| 角色 | 模型／檔位 | 做什麼 |
|---|---|---|
| 新舊框架地圖 | Opus／high ×1（掃描用 `git grep -P` 或 Python，先以已知正例命中） | 5.0 的三張表 |
| 出處表提案 | Opus／high ×2 | (a) target DI／RC 與 spec 規則（以盤點為底稿）；(b) Kernel 公開 API 與 #218 |
| 一般作者清單 | Opus／high ×1 | 工具從現有程式讀；2D 從確認後的 Feature 清單推 |
| 試跑實作 | Opus／high | spike 程式；只登記宣告，不涉決定性邏輯 |
| 統合 | 主會話 Opus | 合併、套判定規則、寫報告 |
| 敵對檢查 | Fable／high，一次，只讀 | 否定地圖與出處表的分類、量法與判定 |

- 估時（推定）：
  - 「小」的部分：4～8 h，包括凍結紀錄與兩條 RC 的草稿（本宣告已含大半；流程規則已由 #222 落地）。
  - 新舊框架地圖：8～16 h。
  - 其餘檢測：24～46 h。
  - 合計 36～70 h；上限 70 h × 1.5 = 105 h。
- 停止條件：
  - AGENTS §14 冒頭三條。
  - 單一問題累計超過 2 h。
  - 試跑要改 `engine/` 才能登記：記錄為發現、不改，停在那一項。
  - 出處表有超過 20% 的列無法判「需要它的產品類別」：停下回報，表示要先由使用者點名 Feature。

## 9. 要使用者定的事

1. 核准本宣告，包括第 5.0 節新舊框架地圖排在最前面。
2. 2D 橫版的候選 Feature 清單（第 5.2 節）：照列、增刪，或換成別的例子。
3. 試跑的工具：voxel_map_editor（建議）或 ui_editor。
4. 第 6 節的門檻：M1＝0、M2a＝0、M2b 與 M3 只記錄。
5. 第 4 節兩條 RC：同意方向的話，另批落字（target 的 RC 變更是 Architecture Delta）。
6. 第 2 節「繼續」與「暫停」的清單。

## 10. 範囲の限界

- 宣告級試跑沒有 World 與執行期。執行期的代價只從 #218 的程式讀，例如 World 中 15 處 `Zone::` 分支、第二層驗證的成本。
- 2D 是合成的對象，不是真實產品；結論的強度以此為限。
- 全局審計第 6 節列的未審區域，本批不補：AGENTS 本身、CI 與守衛基礎設施、apps 與 services 的現況程式。
