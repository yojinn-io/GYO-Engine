# v8 工具：voxel 地圖編輯器的手感調查與大地圖量測（`ve-feel-survey`、`ve-scale-measure`）事前宣告

更新：2026-11-07。Owner：voxel 地圖編輯器線（本會話）。
- 設計稿：[design.zh-Hant.md](../../../voxel_map_editor/design.zh-Hant.md)（Q1～Q12 已核准，決定 `v8-tool-voxel-design-approve`）。
- 紀錄：[HANDOFF](voxel-map-editor-HANDOFF.md)。

**狀態：宣告草稿，等使用者核准；核准前不實作。**

## 代號對照

| 代號 | 白話 |
|---|---|
| `ve-feel-survey` | 手感調查：量延遲、列出和 Goxel 的差異、請使用者回答 H1～H8，不改操作 |
| `ve-scale-measure` | 大地圖量測：用 1 km² 的合成圖量格子檔大小、記憶體、按下時的延遲、整張重烘、畫全部區塊的影格 |

## 兩批共通的前提

- **只量不改**：
  - 不修任何上限、不改操作、不改格式；
  - 只加量測用的命令與它們的自我測試。
  - 結果只描述、不設門檻；門檻在之後的批次（`ve-feel-1`、`ve-big-fill`、`ve-grid-v2`）依結果宣告。
  - 這兩批產出的數字，就是 AGENTS §2「Architecture の反復を計画する条件」要求的「判定に使う予算」的來源。
- **量測方法在這裡固定**，看到結果後不改。要改的話，另記一次，兩次都留。
- **機器**：本機 MacBook Pro（Intel Core i9-9980HK、64 GB、macOS 26.7.1），RelWithDebInfo 的 test 建置樹，GPU 由 SDL GPU 自選。
- **件數の突き合わせ**：每個情境都記「要求的次數／量到的次數／失敗的次數」。不一致時不下結論，交使用者。
- **證據**：主 checkout `build/evidence/tool-v8-voxel-map/editor-next/`（git 不追蹤）。

## 合成圖（兩批共用；產生方法現在固定）

| 代號 | 內容 | 規模 |
|---|---|---|
| `building` | tool-chunked 的 `measure-map` 產生的同一張（`minstd_rand(20261106)`，256×48×256 格，grid sha `60e3aa3c…`） | 768 區塊 |
| `km2-ground` | 2000×2000 格的地面：y = 0 一層方塊；每 64 格一條 1 格寬、2 格高的牆。種子固定 | 約 15,625 區塊 |
| `km2-building` | 2000×48×2000 格：把 `building` 的產生規則平鋪到 2000×2000（同一個種子序列，依區塊座標決定） | 約 46,875 區塊 |

- `km2-building` 的格子文字預估超過 16 MiB，`ParseGrid` 會拒讀。所以兩張 km² 圖都在記憶體裡直接產生 `GridDocument`，不經檔案。檔案大小用 `WriteGrid` 的長度量。
- 1 km² 的推算（design §1）都是未量的數字，這兩批就是要量它們。

## `ve-feel-survey`

### 範圍

1. **延遲的量測**：擴充 `measure-preview`。
   - 情境 `--scenario press|drag|release|undo`，對應 H1 與 H4。
   - 每個情境照視窗做的事情計時，從「輸入」到「畫面上傳完成」：
     - press：拖曳開始（含現在 `Drag::base` 的整份複製）＋第一筆＋重烘＋上傳＋畫＋讀回；
     - drag：拖曳中的一筆（Brush 大小 1 與 4，沿固定路徑）；
     - release：`EndStroke`＋下一幀；
     - undo：`Undo`＋重烘＋下一幀。
   - 圖：`building` 與 `km2-ground`。`km2-building` 放在 `ve-scale-measure`，因為它的記憶體量是那一批的主題。
   - 每個情境 300 次，丟掉前 30 次，記 p50、p95、p99、max。
2. **和 Goxel 的差異表**：照 design §3 與 §4，逐項列出 window-1 和 Goxel 的操作差異（行為，不含程式碼）。
3. **使用者回答 H1～H8**：
   - 我列出每一項的問題（design §4 的表）與量到的數字，請你在畫面上試，指出哪裡差。
   - Q10（Line 用拖曳還是點兩下）與 Q12（飛行相機）照核准，在這時決定。

### 判定（完成的條件；不是門檻）

| 代號 | 判定 |
|---|---|
| FS1 | 四個情境 × 兩張圖都量完，件數一致 |
| FS2 | 差異表寫完，每一列都附 Goxel 的出處（檔名）與 window-1 的出處 |
| FS3 | 你回答 H1～H8，並決定 Q10、Q12；回答與決定記進 HANDOFF |
| FS4 | 量測命令的參數測試（CLI）通過；本機 `cpu|shader` 全部通過；L1 三平台通過 |

## `ve-scale-measure`

### 範圍

擴充 `measure-map`，加 `--scale km2-ground|km2-building`。每項各 5 次，記中位數與最大值。

| 代號 | 量的項目（對應 design §1 的列） |
|---|---|
| SM1 | 格子文字的大小（`WriteGrid` 的長度），以及是否超過 16 MiB |
| SM2 | 記憶體：`GridDocument` 的區塊數 × 16 KiB 的計算值，加上量測時行程的最大常駐記憶體（`getrusage`） |
| SM3 | 按下時的整份複製（`Drag::base` 的做法）的時間 |
| SM4 | 調色盤 undo 的一步（`DocumentStep`，前後兩份）佔的記憶體（計算值）與複製時間 |
| SM5 | 整張烘焙（`BakeMap`）與寫出（`WriteMap`）的時間；區塊化地圖的文字大小 |
| SM6 | 畫全部區塊的影格：`measure-preview --no-edit`，600 幀丟掉前 60 幀，記 p50、p95、p99、max；同時記上傳的 mesh 數與頂點數。GPU 記憶體不足而失敗時，把失敗本身當結果記下來 |
| SM7 | 拾取距離：從地圖一角看向另一角時，`Pick` 的 1024 格能不能碰到（計算） |

### 判定（完成的條件；不是門檻）

| 代號 | 判定 |
|---|---|
| SC1 | SM1～SM7 在兩張 km² 圖上都量完，件數一致；失敗（例如 GPU 記憶體不足）也記下 |
| SC2 | 結果整理成表，交你決定 Q3：v1 是否撐得住，要不要開 `ve-grid-v2` |
| SC3 | 量測命令的參數測試通過；本機 `cpu|shader` 全部通過；L1 三平台通過 |

## 不做

- 修 `Drag::base` 的整份複製、`DocumentStep` 的大小、上限、視錐剔除；
- 改操作、改格式；
- 正交投影。

這些都等量完，在之後的批次宣告。

## 分批、模型、ギア、估時

```text
v8-tool-voxel-editor-next（量測，下段：不碰 DI 與格式）
├── ve-scale-measure  Opus／medium｜3～5 h：km² 合成圖、SM1～SM7
└── ve-feel-survey    Opus／medium｜3～5 h＋你試用的時間：延遲量測、差異表、H1～H8
```

- 順序：先 `ve-scale-measure`，因為 km² 合成圖的產生器兩批都會用。
- 模型：實作者 Opus 5.5／medium（本會話）。兩批只加量測，照設計稿不安排審查者。量測命令照慣例寫自我測試。
- 合計 6～10 h（不含你試用的時間）。超過 15 h，或單一問題累計超過 2 h，停下回報。
- 分支：`claude/v8-tool-voxel-editor-measure`，從 PR #169（`1d2f9ffc`）切出，所以依賴 PR #169 先合併。兩批完成後開一個 PR。

## 要使用者決定的事

1. 本宣告（兩張 km² 合成圖的產生方法、情境、次數、丟棄規則、SM1～SM7、完成條件）。
2. **Q10、Q12 怎麼「試兩種」**：
   - 本批不改操作，所以不在我們的編輯器裡做出第二種。
   - 建議 (a)：你在 Goxel 裡試 Line 的拖曳；飛行相機用你熟悉的工具體驗，再決定。
   - 另一個選項 (b)：在本批做一個只供試用、不合併的臨時版本。這會超出「只量不改」。
