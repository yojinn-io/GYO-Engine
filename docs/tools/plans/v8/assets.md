# 資產工具鏈的規劃（`v8-tool-assets-plan`，草稿）

狀態：**已核准**（2026-10-31，使用者：「OK，這樣也行，開始。」）。audio-tool-line 由本會話開始；anim-events-line 提議另開會話並行（`v8-tool-assets-line-order`）。

## 方向

使用者在 2026-10-31 給了方向（專用工具 → Asset 登錄 → Build／打包 → Runtime；五類工具的範圍；共用規則），之後更正了兩點：

- 「assets不是管全局的，他也是針對單個產品的。」
- 「這個是一條工具鏈而不是一個工具。沒有統一的都去地方和輸出地方。核心是工具加工素材-》assets 登錄，（在遊戲打包是通過assets build 成自定義的二進制包，這個功能可以延後），關鍵是現在的工具。（手工-腳本-工具）這個順序。」

所以：

1. **工具鏈，不是一個工具**：每個工具各自決定素材從哪裡讀、自己可再編輯的資料存在哪。沒有統一的來源目錄，也沒有統一的「製作設定」格式。
2. **工具與 Asset 只有一個交接**：工具加工完，把可以用的資產檔放進產品的資產根目錄，並登錄到該產品的 catalog（現有的 catalog v1：`id`、`type`、`path`）。Asset 以單一產品為單位。
3. **Asset build 延後**：打包時把資產建成自訂的二進位包（`v8-tool-assets-pack`），現在不做。現在的組裝（`assemble_runtime.py` 複製已登錄的檔案）照舊。
4. **重點是工具，照「手工 → 腳本 → 工具」**（AGENTS §5）：每類資產先看它現在在哪個階段，只往下一個階段走。

## 現有能力

| 能力 | 現況 | 證據 | 工具怎麼用 |
|---|---|---|---|
| 登錄與穩定 ID | catalog v1；ID 是邏輯名稱，和路徑無關；跨 catalog 不能重複 | `engine/asset/include/engine/asset/catalog/`、`build/content_contract.py` | 工具輸出後寫入 catalog 條目，這就是「Asset 登錄」 |
| 登錄的完整性 | 資產根目錄下每個檔案都要登錄 | `tests/object_fps_pvp/test_asset_catalog.py` `test_every_asset_file_is_in_the_catalog` | 工具的可再編輯資料（原檔、工程檔）不放進資產根目錄，除非也登錄 |
| 資產間的引用 | 產品的 JSON 用 ID 引用，產品自己的測試檢查 | `assets/object_fps_pvp/players/character.json` | 沿用 |
| 組裝 | 只複製已登錄的檔案到 `bin/assets/<product>` | `build/assemble_runtime.py` | 不動 |
| 工具的登錄與外殼 | `tools.csv`＋`project.json`；ui_editor 的 ImGui、SDL3 系統選擇窗、掛載 catalog 與資產根 | `docs/tool_projects.md`、`tools/ui_editor/` | 新工具沿用 |
| Runtime 載入 | `SoundLoader` 只讀 16 位元 PCM；FBX 由 ufbx 在執行時直接讀 | `engine/asset/src/loaders/`、`engine/model/backend/ufbx/` | 工具輸出 Runtime 能直接讀的格式 |

## 各類資產現在的階段

| 資產 | 現在的階段 | 下一步 | 根據 |
|---|---|---|---|
| 音效 | **腳本**：`convert_wav.py`（任意 WAV → 48 kHz PCM16，可重現） | **工具**：音效工具 | `v8-tool-assets-audio-import`；腳本在 `docs/architecture/plans/audio/scripts/` |
| 動畫事件 | **手工**（2026-10-31，等實聽）：通用格式 `gyo.anim_events`（[契約](../../../architecture/animation-events.md)）＋PvP 手寫資料；C++ 的過渡規則 `ReloadSoundOffsetSeconds` 已移除 | **工具**：模型檢視（`v8-tool-model-viewer`）在畫面上編輯時刻 | `v8-tool-assets-anim-events`（[dev_log](../../../dev_logs/2026_10_31_tool_v8_assets_anim_events.zh-Hant.md)） |
| 模型 | **手工**：外部 DCC 製作，手寫 `character.json`、animset | 動畫事件要在畫面上調時刻時，做**工具**：模型檢視 | 3d-assets.md |
| 碰撞體 | **手工**：產品的 `v8-product-hitbox-data` 先手寫 JSON | 格式 owner 由產品規劃定；之後併進模型檢視 | 產品 v8 README |
| SpriteSheet、視頻、文字與劇情 | 沒有實際需求 | 待定，出現需求時從手工開始 | 使用者的方向 |

觀察到的壓力：

- 音效原檔沒有放處，現在找不到了（主 checkout 的 `audio/` 是空的）；要重轉只能靠記得原檔在哪。音效工具會自己記住原檔路徑，見下方「音效工具」。
  - 更正（2026-11-01，asset-layout 線告知）：四個現有音效的原檔在 master 歷史 commit `30b36bdb` 的 `assets/object_fps_pvp/audio/source/`（例如 `git show 30b36bdb:assets/object_fps_pvp/audio/source/gun_shoot.wav`），之後從 repo 移除。「找不到」是錯的；是否用工具重產，等你決定。
- 主 checkout 有未追蹤的模型原檔（`Player_V20.fbx`、`nailer_v2/source/Nailer1_FPS.fbx` 等），也一樣沒有放處；模型檢視工具做的時候一起看。

## 音效工具（`v8-tool-audio-editor`）

腳本已經確立了轉換的規格，工具接手同一個程序（AGENTS §5）。

- **讀**：任意位置的 WAV（系統選擇窗）。
- **看與聽**：播放、預覽音量、波形、取樣率／聲道／時長／位元深度。
- **改**：裁剪、增益（會影響輸出；和預覽音量分開）。
- **寫**：轉成 Engine 公開的片段格式（現在是 48 kHz PCM16；重新取樣和 `convert_wav.py` 演算法等價，見下方事前宣告），放進掛載的產品資產根目錄，並在 catalog 加或更新條目（type `sound`）。
- **可再編輯**：工具自己的工程檔記錄原檔路徑、裁剪與增益；放在使用者選的位置（不在資產根目錄）。格式由工具自己持有。
- **不做（現在）**：循環區間（`GYO::Audio` 沒有循環播放，audio.md 的 Replacement Criteria 才提到）、拼接、多軌、串流。
- 腳本保留：Data Contract 是「48 kHz PCM16 的 WAV＋catalog 條目」，腳本、手工都還能產生（AGENTS §8）。

## `v8-tool-audio-editor-convert` 的事前宣告（§14.1，2026-10-31 使用者選「(2) 演算法等價」）

- **重新取樣**：工具的重新取樣（抖動之前的浮點結果）和 `convert_wav.py` 的 `resample()` 比對。參考向量由腳本對合成的輸入（多種取樣率、位元深度、聲道數）產生並提交；每個樣本的絕對誤差 ≤ 1e-9 才算通過。
- **抖動與量化**：工具自己的固定種子亂數，不仿造 Python 的。判定：同一輸入、同一平台跑兩次，位元組相同；抖動量在 ±1 LSB 內（以 0.5 LSB 的固定輸入檢查：抖動在 (-1, 1) 內時輸出只能是 0 或 1）；飽和計數和實際被截斷的樣本數相同。另外檢查每個樣本和 `值 × 32767` 的差 < 1.5 LSB（抖動加四捨五入）。
- **輸出**：用 Engine 的 `SoundLoader` 載入、`ClipBank` 接受（Design Invariant 2）。
- 和腳本的整檔輸出不比位元組（只差在抖動，1～2 LSB）。四個現有音效的原檔已找不到，不重產。（更正 2026-11-01：原檔在 commit `30b36bdb`，見上方「觀察到的壓力」。）
- 試行與重試：測試是決定性的，不重試；失敗就停下回報。

## 實作順序（提案）

```text
v8-tool-assets-plan                     「high｜Opus 5.5｜本規劃（草稿）」
│
├── audio-tool-line                     「PR 線：音效從腳本走到工具」
│   ├── v8-tool-audio-editor-convert    「high（重新取樣局部 xhigh）｜Opus 5.5｜核心程式庫＋CLI：讀 WAV、轉換（演算法等價）、輸出並登錄 catalog」
│   ├── v8-tool-framework-shell         「medium～high｜Opus 5.5｜工具共用層 tools/framework/：從 ui_editor 搬出外殼四塊，行為不變」（PR #123，已合併）
│   └── v8-tool-audio-editor-edit       「high｜Opus 5.5｜GUI：播放、波形、資訊、裁剪、增益、工程檔」（排在 framework-rework-line 之後）
│
├── framework-rework-line               「PR #123 的整改，工具側（2026-11-01 定案）：每批等對應的架構批合併」
│   ├── v8-tool-ui-editor-path-dialog-adopt「high｜Opus 5.5｜T1：ui_editor 改呼叫 Engine 的選擇窗（等 A1）」
│   ├── v8-tool-user-files-adopt          「medium～high｜Opus 5.5｜T2：兩個工具改用 engine/io 的使用者檔案 API（等 A2）」
│   └── v8-tool-framework-shell-adopt     「high｜Opus 5.5｜T3：ui_editor 接外殼介面與 ImGui 綁定；audio_editor 改用 catalog 結構 API＋JSON 綁定（等 A3）」
│
├── anim-events-line                    「PR 線：動畫事件從無到手工，再到工具」
│   ├── v8-tool-assets-anim-events      「high｜Opus 5.5｜手工：gyo.anim_events 契約＋PvP 手寫資料＋Runtime 排程；移除過渡規則（產品批）」
│   └── v8-tool-model-viewer            「high｜Opus 5.5｜工具：模型資訊、骨骼動畫預覽、事件時刻的編輯」
│
├── （延後）v8-tool-assets-pack          「Asset build：打包成自訂二進位包」
└── （待定）SpriteSheet、視頻、文字與劇情 「出現需求時從手工開始」
```

| 代號 | 檔位｜引擎 | 估時 | 完成的樣子 |
|---|---|---|---|
| `v8-tool-audio-editor-convert` | high（局部 xhigh）｜Opus 5.5 | 6～10 h | 工具登錄（`tools.csv`、`project.json`）、核心程式庫與 CLI；照上方事前宣告通過；catalog 條目正確；突變檢查 |
| `v8-tool-framework-shell` | medium～high｜Opus 5.5 | 3～5 h | `tools/framework/` 有 ImGui＋SDL 宿主、系統選擇窗、檔案存取、唯讀 catalog 掛載、catalog 寫入（從音效工具搬入）；ui_editor 改用它、測試全過；Design Invariants＋守衛；`framework` 加進保留名 |
| `v8-tool-audio-editor-edit` | high｜Opus 5.5 | 8～12 h | 播放、波形、裁剪、增益、工程檔；使用者在 GUI 上試過。排在 T1～T3 之後；事前宣告已核准（上方），分支 `claude/v8-tool-audio-editor-edit` 只有宣告的提交（本機） |
| `v8-tool-ui-editor-path-dialog-adopt`（T1） | high｜Opus 5.5 | 2～3 h | 等 A1（`v8-engine-platform-path-dialog`）。ui_editor 改呼叫 Engine 平台層的選擇窗；用途對照（開檔、匯出、選資產根目錄、選 catalog → 請求；tag → 用途）與收件匣留在 ui_editor。守衛連回 `GYO::PlatformSDL`（放寬，§14.4 出示 1～5 並經你核准）；GUI 由你試過 |
| `v8-tool-user-files-adopt`（T2） | medium～high｜Opus 5.5 | 1～2 h | 等 A2（`v8-engine-io-user-files`）。ui_editor、audio_editor 改用 `engine/io` 的使用者檔案 API；兩個守衛的變更照 §14.4；兩個工具的測試照舊通過 |
| `v8-tool-framework-shell-adopt`（T3） | high｜Opus 5.5 | 3～5 h | 等 A3（`v8-framework-shell`）。ui_editor 接新的外殼介面與 ImGui 綁定 target（取代 `GYO::ToolFrameworkGui`；資產預覽仍經 `*Native` 取 renderer，Engine 的貼圖 API 另案）；audio_editor 改用 catalog 的結構 API＋JSON 綁定，CLI 的 `convert_taken_path` 結束碼 4 不變；若工程檔自己連 nlohmann，守衛變更照 §14.4 |
| `v8-tool-assets-anim-events` | high｜Opus 5.5 | 5～8 h | `gyo.anim_events` 的契約與 Design Invariants（Engine 多一個格式，Architecture Delta）、PvP 把事件名稱對到音效、Runtime 排程、移除過渡規則；使用者實聽 |
| `v8-tool-model-viewer` | high｜Opus 5.5 | 12～20 h | 模型資訊、骨骼動畫預覽、事件時刻的編輯 |

## 等你決定

| 代號 | 問題 | 選項與取捨 | 建議 | 會改變 |
|---|---|---|---|---|
| `v8-tool-assets-line-order` | 兩條 PR 線的順序 | 兩條線互不依賴（檔案不重疊）。先 audio-tool-line（腳本 → 工具，最接近完成）／先 anim-events-line（使用者聽到的違和感先解決）／兩條並行（各開一個會話） | 並行：一條是工具、一條是契約與產品資料，不搶同一批檔案 | 使用者何時聽到換彈聲的修正 |

## 決定紀錄

| 代號 | 決定 | 使用者原話 |
|---|---|---|
| `v8-tool-assets-per-product` | （2026-10-31）Asset 以單一產品為單位：catalog 與資產都屬於各產品 | 「assets不是管全局的，他也是針對單個產品的。」 |
| `v8-tool-assets-anim-events-format` | （2026-10-31）(b)：動畫事件用獨立的通用格式（例如 `gyo.anim_events`：片段 → 時刻＋事件名稱），契約歸 Engine 的動畫 owner，資料放在各產品；產品把事件名稱對到音效。工具只認通用格式 | 「照 (b)」 |
| `v8-tool-assets-plan` | （2026-10-31）核准本規劃與 Editor 清單（音效工具、模型檢視工具；動畫事件先手工） | 「OK，這樣也行，開始。」 |
| `v8-tool-audio-editor-acceptance` | （2026-10-31）轉換的驗收用演算法等價（上方事前宣告），不要求和腳本逐位元組相同 | 選「(2) 演算法等價（建議）」 |
| `v8-tool-audio-editor-invariants` | （2026-10-31）[docs/audio_editor/architecture.md](../../../audio_editor/architecture.md) 的 Design Invariants 1～8＋Replacement Criteria；第 2 條照使用者的意見放寬為「跟著 Engine 公開的格式」 | 「這個可以調整，不要弄太死。如果未來Engine 升級，我這個可以不用變。」「照草稿寫進去」 |
| `v8-tool-audio-editor-audio-owner` | （2026-10-31）預覽需要的 GYO::Audio 增強（停止、執行中換片段、播放位置）由音效線規劃與實作；需求已轉給音效線會話 | 「這個需要增強GYO::Audio 的功能。」選「音效線做（建議）」 |
| `v8-tool-framework-scope` | （2026-10-31）建工具共用層 `tools/framework/`（不是工具、不登錄）：只放兩個工具都用的 ImGui＋SDL 宿主、系統選擇窗、檔案存取、唯讀 catalog 掛載；catalog 寫入先留在音效工具。依賴：工具 → framework → Engine。排在 `-edit` 之前 | 「現在是否可以建立framework層？」→「照這個範圍」 |
| `v8-tool-framework-principle` | （2026-10-31）Engine 保持純粹（不放工具或整合用的功能）；整合工具的部分由 framework 提供。catalog 寫入屬於整合：`-convert` 先寫在音效工具裡的單獨檔案，`v8-tool-framework-shell` 時搬進 framework | 「Engine保持純粹，framework則提供整合工具。」 |
| `v8-tool-framework-top-level` | （2026-10-31）framework 是遊戲與工具共用的同一層，放在頂層 `framework/`（原本是 `tools/framework/`）。依賴：遊戲或工具 → framework → Engine。現在只有工具的部分；遊戲的部分照 [v7 README](../../../object_fps_pvp/plans/v7/README.md) 的啟動條件（第二個遊戲以相同方式組合 Engine）再抽出。起因：架構會話指出 owner 名 `framework` 和 v7 README 定義的 framework 層撞名 | 「這個是遊戲和工具共用的。」；選項「同一層，現在就搬到頂層」 |
| `v8-tool-framework-rework-split` | （2026-11-01）PR #123 的整改分工：framework 與下沉到 Engine 的部分由架構整改任務卡做（A1 `v8-engine-platform-path-dialog`、A2 `v8-engine-io-user-files`、A3 `v8-framework-shell`、A4 `v8-framework-invariants`）；工具側 T1～T3 由工具線做，各等對應的 A 批合併；先處理已知問題再做功能，`-edit` 排在 T1～T3 之後。catalog 規則的單一來源預設 (a)：規則留在 framework 核心，用 `tests/common/fixtures/asset_contract/cases.json` 對齊，不動 Engine（工具線沒有異議） | 經架構會話轉述：「彈窗的 OS 那一半是視窗系統的能力，和剪貼簿同級，屬 Engine；流程那一半是工具的，留在工具。」「framework在GYO中就是一個通用建置，他可以處理UI的東西，但不是直接嵌入，而是抽象出來。」「framework 可以遊戲和tool的公用層…遊戲就是一個有娛樂效果的工具…只不過方向不同。」；你：「開始吧」（登錄 T1～T3） |
| `v8-tool-assets-toolchain` | （2026-10-31）工具鏈，不是一個工具：沒有統一的來源與輸出位置；工具加工素材 → Asset 登錄；Asset build（自訂二進位包）延後；照「手工 → 腳本 → 工具」。第一版草稿的製作設定格式、處理器、Build 階段，以及 `source-root`、`output`、`processor-home`、`processor-language` 四個待決，都撤下 | 「這個是一條工具鏈而不是一個工具。……關鍵是現在的工具。（手工-腳本-工具）這個順序。」 |
