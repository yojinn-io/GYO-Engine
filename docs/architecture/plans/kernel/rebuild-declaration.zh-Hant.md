# 事前宣告：`v8-arch-rebuild`（新分支 `gyo2` 重建）與第一批 R0（帶過去清單、刪除舊實現）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：草稿，待使用者核准（第 9 節各題）。**

出處：[全局審計](global-audit.zh-Hant.md)；kernel `HANDOFF.md` 決定行：
- `v8-arch-engine-scope-general`
- `v8-arch-gyo2-in-repo`
- `v8-arch-single-player-no-complexity`
- `v8-arch-rebuild-branch`

使用者原話（2026-11-14）：
> 如果是人類的做法，現在最好的方式就是開一個新分支，在那個新分支里移除掉所有的舊實現重新搭建新的架構。完成之後再把產品一直過去。
> 走新分支重建，改寫宣告吧

本檔取代同日的草稿 `rederive-detect-declaration`（停點「中」：凍結加檢測先行）。那份沒有核准，也沒有執行。

## 1. 目的（Feature → Contract → Architecture）

- **Feature**：通用引擎。三類產品用同一套機制：
  1. **PvP**：object_fps_pvp 的 Match 與 Client。
  2. **2D 橫版通關單機**：使用者點名的例子，目前沒有產品。
  3. **Editor 與四個工具**：voxel_map_editor、model_viewer、audio_editor、ui_editor。

  強化 FPS 或 PvP 不得讓第 2、3 類的作者多做事；作者與玩家感受不到差異即可，多一些性能開銷可以接受。
- **Contract**：由 R1 從三類產品的 Feature 推出。本檔不預定。
- **Architecture**：在新分支上重建。舊程式留在 master 與 git 歷史中，只當參考，不當約束（`v8-arch-gyo2-in-repo`）。

全局審計找到的偏差機制，是舊模組、舊守衛、舊 golden 把設計拉回舊形狀。新分支上沒有這些舊實現，這股拉力就機械地消失，不必靠會話每次判斷新舊。

## 2. 分支與 repo 的規則

| 項 | 規則 |
|---|---|
| 分支 | 長期分支 `gyo2`，2026-11-14 從 master 08566260 切出（使用者：「你先開一個新分支，不要在 master 上做。」）。本宣告與全局審計也以 PR 進 `gyo2`，不進 master |
| 日常工作 | 一律開小 PR 進新分支。`cross-platform.yml` 的 `pull_request` 沒有限定目標分支（2026-11-14 查證），所以每個 PR 都跑 L1 三平台。直接推到新分支不跑 CI，所以不直接推 |
| master | 凍結。只收事故修正；每個修正在新分支補上，或記錄「新分支不適用」 |
| 合併回 master | 產品都搬完、使用者決定時點後，新分支整個取代 master。方式由使用者在那時定 |
| 舊程式 | 只讀參考。新分支的設計選項照 AGENTS §4 寫系譜；理由不得只有「與舊的一致」 |
| 規則文件 | AGENTS.md、`agents/`、`docs/architecture/README.md` 的入口與檢查項，兩邊都適用。其他會話（風紀委員、圖書管理員、原架構師會話）仍在 master 修改，本線定期把它們同步進 `gyo2` |

## 3. 階段（每階段各有事前宣告；估時在 R1 結束時重算）

| 階段 | 內容 | 驗收 |
|---|---|---|
| **R0** | 帶過去清單：逐項列出頂層目錄、engine 模組、第三方、工具鏈與文件，標「帶過去」「帶過去並重審」「不帶」，由使用者逐項核准 | 第 5 節 |
| **R0b** | 在 `gyo2` 上刪除「不帶」的項目，讓帶過去的集合能單獨建置 | 第 5 節 |
| **R1** | 從三類產品的名指 Feature 推出 Contract 與 Architecture。參考 Bevy，偏離時寫獨立理由。產出新分支的 target 文件（Design Invariants、RC，含 RC-出處與 RC-同構），以及其後各階段的批與估時 | 使用者核准 |
| **R2** | 薄實作核心：只做第一個產品直接用得到的部分，其餘只定介面（AGENTS §2） | 該批宣告 |
| **R3** | 搬第一個產品：工具或 2D 樣本，不是 PvP（第 9 節第 3 題） | 該批宣告 |
| **R4** | 搬第二類產品 | 該批宣告 |
| **R5** | 搬 PvP：Match、Client、Gateway。golden 37 只當 PvP 自己的驗收，不當核心設計的判官 | 該批宣告 |
| **R6** | 合併回 master | 使用者決定 |

## 4. R0：帶過去清單

### 4.1 範圍（件數以 2026-11-14 的 master 08566260 為準，開工時重數）

| 範圍 | 件數 |
|---|---|
| 頂層目錄 | 13 個：`.github`、`agents`、`apps`、`assets`、`build`、`docs`、`engine`、`framework`、`services`、`spikes`、`tests`、`third_party`、`tools` |
| `engine/` 模組 | 23 個：asset、audio、base、collision、config、entity_store、input、io、kernel、map_markers、math、model、net、os、platform、render、runtime、text、threads、time、trace、ui、voxel_mesh |
| `third_party/` | 10 個：asio、doctest、httplib、imgui、nlohmann_json、protobuf、sdl3、sdl3_image、sdl3_ttf、ufbx |
| 產品 | 6 個：apps 的 object_fps_pvp、services 的 gyo_gateway、tools 的 4 個工具（R0b 時全部不帶，到 R3～R5 才依新架構搬回） |
| 文件 | `docs/` 下各子目錄，含 target、legacy 頂層文件、產品文件 |
| 工具鏈 | `build/` 的 CMake 與腳本、`.github/` 的 workflow、`tests/` 的共通基礎設施（突變執行器、掃描器、測試支援） |

### 4.2 每一項要寫的欄（照 AGENTS §4 與〈Batch declaration checks〉）

| 欄 | 內容 |
|---|---|
| 處置 | 帶過去／帶過去並重審／不帶 |
| 系譜 | 這一項的形狀從哪裡來 |
| 需要它的 Feature 與產品類別 | 從第 1 節的三類中選 |
| 被否決的處置與決勝理由 | 例：帶過去但 R1 推不出需要它；不帶但要重寫一份 |
| 依賴 | 帶過去的項目依賴哪些項目；依賴「不帶」的項目要列出 |

### 4.3 架構師的初步建議（R0 的盤點會逐項查證，這裡只是起點）

| 處置 | 項目 | 理由 |
|---|---|---|
| 帶過去 | `engine/base` 的 Assert／Result 錯誤契約 | R0～R6 已完成，契約在 error-handling |
| 帶過去 | `engine/math` | 2026-10-03 核准的 GYO::Math 統一 |
| 帶過去 | `engine/os` | adaptor 原意；2026-10-28 收口 |
| 帶過去 | `third_party/` | 外部組件；adapter 的位置由 R1 定 |
| 帶過去 | `build/` 的 CMake 基礎、`.github/` 的 L1 | 建置與 CI 是三平台驗收的前提 |
| 帶過去 | `tests/` 的共通基礎設施 | 突變執行器、掃描器 |
| 帶過去 | AGENTS.md、`agents/`、`docs/architecture/README.md` 的入口 | 規則與入口 |
| 帶過去並重審 | `engine/kernel` 的簽名推存取、Scheduler、Plan 決定性 | 全局審計判為 R 的核心機制。程式裡同時織進 zone、rollback、ingress、digestSet、cFingerprint。兩個選項交 R1 定：只帶程式並刪掉 PvP 概念，或只帶設計、在 R2 重寫 |
| 不帶 | 其餘 engine 模組 | 依 R1 的推導在 R2～R5 重建，舊程式當參考 |
| 不帶 | `framework/` | 已決定退役 |
| 不帶 | 產品（apps、services、tools） | 在 R3～R5 依新架構搬回 |
| 不帶 | `spikes/` | — |
| 不帶 | target 文件、legacy 頂層文件、kernel 計畫文件 | 新分支的 target 由 R1 重寫；舊文件留在 master 供查閱 |

### 4.4 同詞異義表（R0 一起產出）

新舊兩代都用、意思不同的詞，例如 Runtime、Host、World、Resource、Link、Domain、Stage、Plugin、Component、Session、Kernel。每個詞寫出：
- 舊義的出處；
- 在新分支的用法：改名、沿用並重新定義，或禁用。

新分支的文件與程式照這張表用詞，避免新舊混用。這也回應使用者 2026-11-14 的擔心：「新舊框架混在一起，而且我怕你自己也區分不開。」

## 5. 判定（看結果前宣告）

**R0（清單）**
1. **件數**：第 4.1 節每一類都在清單裡，件數與 `git ls-tree` 重數的結果相同。重數要先以一個已知項目命中，寫進報告的件数の突き合わせ。
2. **欄位**：每一項都有第 4.2 節的五欄；「帶過去」的項目要寫出需要它的 Feature。
3. **檢查**：交給使用者前，由作者以外的模型確認第 4.2 節的五欄與決勝理由（AGENTS §4，#222）。

**R0b（建立分支）**
4. **建置**：新分支的第一個 PR 只做刪除與讓建置通過，L1 三平台綠。
5. **無殘留引用**：被刪的目錄名與模組名，在新分支中零命中；掃描用 `git grep -P`，先以刪除前的已知引用命中。
6. **範圍**：建置修補只碰帶過去的項目。帶過去的項目依賴被刪項目、不能只改連結時，停下回報，不自己決定改成帶過去或改寫。

## 6. 編制

上段：涉及架構判斷與 Design Invariants。所有 agent 明寫模型與檔位。

| 角色 | 模型／檔位 | 做什麼 |
|---|---|---|
| 盤點 | Opus／high ×2 | (a) `engine/`、`third_party/`、產品的依賴與系譜；(b) `build/`、`.github/`、`tests/` 基礎設施、文件與同詞異義 |
| 統合 | 主會話 Opus | 寫清單與建議 |
| 檢查 | Fable／high，一次，只讀 | #222 的別模型檢查：否定五欄與決勝理由 |
| R0b 實作 | Opus／high | 刪除與建置修補；L1 是外部驗證 |

## 7. 估時與停止

- R0 的清單：8～16 h；R0b：4～12 h。合計 12～28 h，上限 28 h × 1.5 = 42 h。都是推定。
- R1 之後各階段的估時，在 R1 結束時寫進計畫，由使用者確認。
- 停止條件：AGENTS §14 冒頭三條；單一問題累計超過 2 h；第 5 節第 6 點。

## 8. 現有工作的處置

| 對象 | 建議 |
|---|---|
| PR #218 world-exec | 關閉，分支留作參考。設計依 R1 重來 |
| PR #219 B1 規劃 | 關閉，分支留作參考。plugin 與 App 的組裝由 R1 推導 |
| PR #220 卡 07 entity-store 重審 | 合併到 master，作為審計紀錄（只有 docs） |
| PR #223 Batch declaration checks 細目 | 照常在 master 進行；規則兩邊都適用 |
| 卡 01～05 的分支、卡 08 的 patch | 留作參考，不再推進 |
| 遷移計畫 `migration-plan.zh-Hant.md` 與 target 文件 | master 不動。`gyo2` 在 R0b 刪除它們；舊版留在 master 供查閱 |

## 9. 要使用者定的事

1. 核准本宣告，包括第 3 節的階段與第 2 節的分支規則。
2. 分支名稱：已先以 `gyo2` 建立；要改名的話在 R0b 之前改。
3. 第一個搬的產品（R3）：voxel_map_editor、ui_editor 或 2D 樣本。可以留到 R1 結束時定。
4. 第 4.3 節的初步建議。最後以 R0 的盤點結果逐項核准，這裡只看方向。
5. 第 8 節現有工作的處置。

## 10. 範囲の限界

- 整體工時要到 R1 結束才能估。在那之前只有 R0、R0b 的估時。
- 舊程式留在 master，只讀參考；新分支不會自動吸收 master 之後的改動，事故修正照第 2 節補上。
- 2D 橫版是合成對象，直到使用者點名真實產品。
- master 凍結期間，PvP 的事故修正仍在 master 做，用舊架構。
