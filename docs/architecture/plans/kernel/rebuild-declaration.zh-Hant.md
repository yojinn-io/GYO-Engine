# 事前宣告：`v8-arch-rebuild`（新分支 `gyo2` 重建）與第一批 R0（帶過去清單、刪除舊實現）

更新：2026-11-14。Owner：架構（架構師會話）。**狀態：已核准（使用者 2026-11-14「核准，開始 R0」）。R0 的[帶過去清單](rebuild-r0-carry-list.zh-Hant.md)已核准（使用者 2026-11-14「1 到 9 照建議」）；**判定 4 與判定 5 以清單第 7 節的修訂為準**。R0b 進行中。** 分支名稱照 `gyo2`；第 9 節第 3 題（第一個搬的產品）留到 R1 結束；第 8 節的 PR 處置（關閉 #218、#219，合併 #220）屬對外動作，等使用者另行指示。

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
| master | 凍結。只收事故修正；修正不併進 `gyo2`，產品搬回時（R3～R5）以當時的 master 為來源 |
| 合併回 master | `gyo2` 是新的工作空間。它整體完成、通過驗證、產品都移植之後，才整個取代 master；**不分段搬回**（使用者 2026-11-14）。時點與方式由使用者在那時定 |
| 舊程式 | 只讀參考。新分支的設計選項照 AGENTS §4 寫系譜；理由不得只有「與舊的一致」 |
| 規則文件 | AGENTS.md、`agents/`、`docs/architecture/README.md` 的入口與檢查項，兩邊都適用。**master 不併進 `gyo2`**（使用者 2026-11-14：「master 不用放到 gyo2，現在 gyo2 分支上做好，然後再搬到 master 上去。」）。其他會話在 master 改的規則，gyo2 需要時把內容重寫進來；兩邊的差異在 R6 搬回 master 時處理 |

## 3. 階段（每階段各有事前宣告；估時在 R1 結束時重算）

| 階段 | 內容 | 驗收 |
|---|---|---|
| **R0** | 帶過去清單：逐項列出頂層目錄、engine 模組、第三方、工具鏈與文件，標「帶過去」「帶過去並重審」「不帶」，由使用者逐項核准 | 第 5 節 |
| **R0b** | 在 `gyo2` 上刪除「不帶」的項目，讓帶過去的集合能單獨建置 | 第 5 節 |
| **R1** | 從三類產品的名指 Feature 推出 Contract 與 Architecture。參考 Bevy，偏離時寫獨立理由。產出新分支的 target 文件（Design Invariants、RC，含 RC-出處與 RC-同構），以及其後各階段的批與估時 | 使用者核准 |
| **R2** | 薄實作核心：只做第一個產品直接用得到的部分，其餘只定介面（AGENTS §2） | 該批宣告 |
| **R3** | 搬第一個產品：工具或 2D 樣本，不是 PvP（第 9 節第 3 題） | 該批宣告 |
| **R4** | 搬第二類產品 | 該批宣告 |
| **R5** | 搬 PvP：Match、Client、Gateway。不急：底層機制（R1、R2）建好並驗證之後才動（使用者 2026-11-14）。golden 37 只當 PvP 自己的驗收，不當核心設計的判官 | 該批宣告 |
| **R6** | 合併回 master | 使用者決定 |

**目前的範圍只到 R2**（使用者 2026-11-14：「先不動工具和產品，現在就是造架構設計做好機制，我同意移植才動。」）。R3～R5 的每一次移植都要使用者明說同意才開始；在那之前，R2 的機制用合成測試與架構師自建的 2D、3D 樣本驗證（使用者：「你可以自己做 2D、3D 樣本來測試。」）；樣本放在 gyo2 內、標明是測試樣本，不是既有工具或產品的移植。

## 3b. 判斷舊設計的做法：內容、名字、歸屬分開問（使用者 2026-11-14，R1 起適用）

使用者：「不是所有舊說法都是錯的，也不是所有舊設計都有問題。這個就是你要區分的。」

看到舊設計的一項，分開問三題，不壓成「新或舊」一個標籤：

1. **內容**：它說的事情對不對？拿掉舊的理由後，結論還成不成立？
2. **名字**：用的詞是不是已經改了（同詞異義表）？改詞不代表內容錯。
3. **歸屬**：這件事由誰持有、住在哪一層，有沒有變？

- 只有第 1 題不成立，才是「不帶」。
- 第 2 題變了，是「照帶、換名字」。
- 第 3 題變了，是「照帶、換位置」。

統合、口頭說明、套用同詞異義表時都照這三題；不因為用了舊詞就判成舊說法，也不因為追得到舊程式就判成錯（全局審計 v1 被指出的反向偏差）。

例：舊稿說 Host 剩下五項職責——建 World、選 schedule、列 plugin、接外部服務、跑迴圈。
- 內容成立：這五項不該由 Scheduler 做，Bevy 也放在 World 外。
- 名字變了：Host 改成 main 加 runner。
- 歸屬變了：不再由一個物件全部持有，分給 main、runner 與各 plugin。
- 真正要重新推導的只有兩處：固定步累加器放哪裡；寫死 `FixedTickRuntime` 的白名單。

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
| 依第 4.5 節逐項定 | 舊 Kernel 的設計與程式：`engine/kernel`、spec、mechanism、blueprint、networking、target 各頁 | 使用者 2026-11-14：裡面很多東西可以拿過來。全局審計判為 R 的核心機制照帶；範圍放錯的設計換位置；其餘逐項判 |
| 不帶 | 其餘 engine 模組 | 依 R1 的推導在 R2～R5 重建，舊程式當參考 |
| 不帶 | `framework/` | 已決定退役 |
| 不帶 | 產品（apps、services、tools） | 在 R3～R5 依新架構搬回 |
| 不帶 | `spikes/` | — |
| 不帶 | legacy 頂層文件、遷移計畫與各批的宣告和報告 | 舊架構的設計與舊計畫；留在 master 供查閱。Kernel 的設計文件不在這一列，依第 4.5 節 |

### 4.4 同詞異義表（R0 一起產出）

新舊兩代都用、意思不同的詞，例如 Runtime、Host、World、Resource、Link、Domain、Stage、Plugin、Component、Session、Kernel。每個詞寫出：
- 舊義的出處；
- 在新分支的用法：改名、沿用並重新定義，或禁用。

新分支的文件與程式照這張表用詞，避免新舊混用。這也回應使用者 2026-11-14 的擔心：「新舊框架混在一起，而且我怕你自己也區分不開。」

### 4.5 舊 Kernel 設計的盤點（使用者 2026-11-14 追加）

使用者：「你可以去看下之前的 kernel 的設計。裡面很多東西是可以拿過來的。」

- **對象**：
  - 設計文件：mechanism、blueprint、networking、spec（§0、§A～§M、附錄）、target 各頁的 DI 與 RC。
  - 程式：master 上的 `engine/kernel`、PR #218 的 World 與 Executor、`tests/common/kernel`、`spikes/kernel`。
- **每個設計元素的處置**（五選一）：

  | 處置 | 意思 |
  |---|---|
  | 照帶 | 設計與理由都成立 |
  | 帶設計、換位置 | 設計成立，但範圍放錯。例：只有 PvP 需要，卻對所有 World 生效。改放 plugin，或改成可選 |
  | 帶設計、換理由 | 結論成立，理由要從 Feature 重寫 |
  | 帶程式 | 程式可以直接用；列出要刪或要改的部分 |
  | 不帶 | 由 R1 重新推導；寫出原因 |

- 每一項同樣寫第 4.2 節的系譜、需要它的產品類別、被否決的處置與決勝理由，並引用全局審計對它的判定。
- 判斷的起點是全局審計：
  - 判為 R 的核心機制，預設照帶。
  - 判為 P-範 的，預設換位置。
  - 判為 O-結 的，預設不帶。
  - 判為 O-理、S 或 ? 的，逐項判。

## 5. 判定（看結果前宣告）

**R0（清單）**
1. **件數**：第 4.1 節每一類都在清單裡，件數與 `git ls-tree` 重數的結果相同。重數要先以一個已知項目命中，寫進報告的件数の突き合わせ。第 4.5 節的表要涵蓋 spec 的每一節、target 各頁的每條 DI 與 RC、`engine/kernel` 的每個公開標頭。
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
| 盤點 (c) | Opus／high ×2 | 第 4.5 節：(c1) 舊 Kernel 的設計文件；(c2) Kernel 的程式、測試與守衛 |
| 統合 | 主會話（2026-11-14 R0 開始後，使用者把主會話換成 Fable 5.2） | 寫清單與建議 |
| 檢查 | Opus／xhigh，一次，只讀（原定 Fable／high；統合者變成 Fable 後，照 navigator 上段表換成 Opus） | #222 的別模型檢查：否定五欄與決勝理由 |
| R0b 實作 | Opus／high | 刪除與建置修補；L1 是外部驗證 |

## 7. 估時與停止

- R0 的清單：12～22 h（含第 4.5 節的 Kernel 盤點）；R0b：4～12 h。合計 16～34 h，上限 34 h × 1.5 = 51 h。都是推定。
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
