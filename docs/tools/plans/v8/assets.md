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

## framework-rework-line 的事前宣告（2026-11-01 核准）

前提：架構的 PR A（PR #127，merge `3954f7e7`）已合併；本分支已含它（`9e2002e8`）。新介面依架構會話的通知與 `docs/architecture/platform-sdl.md`「Path dialogs」、`docs/architecture/io.md`「User files」、`framework/include/gyo/framework/`、`framework/bindings/`。舊件在 PR B 前都還在，所以每批可以單獨切換。

順序：T1＋T3a → T2 → T3b，一條 PR。每批結束時寫 dev_log 並更新 HANDOFF。

模型與檔位（照 [領航員規則](../../../../agents/navigator.md)，2026-11-01 #130）：

| 批 | 段 | 根據 | 実装者 | 審査者 | 検証者（§14.4） |
|---|---|---|---|---|---|
| T1＋T3a | 上段 | 改 Design Invariants（ui_editor DI 1、2）與守衛；接新的公開介面 | Opus 5.5／high（主會話） | Opus 5.5／high | Fable 5.1／high |
| T2 | 上段 | 改守衛（audio_editor DI 1）；修缺陷（§14.3） | Opus 5.5／high（主會話） | Opus 5.5／high | Fable 5.1／high |
| T3b | 上段 | 改 Design Invariants（audio_editor DI 3）與守衛 | Opus 5.5／high（主會話） | Opus 5.5／high | Fable 5.1／high |

- 理由：上段不用小模型。検証者用 Fable，盲點和実装者不重疊；比主會話強，經你同意。它是另開的子 agent，只核對 1～5 的記述與突變結果，不改程式。
- 審査者是另開的子 agent，做 code review。
- 跑突變是 low 的定型作業，用腳本跑，不開子 agent。
- 不用 ultracode，也沒有敵対的検査者：三批都照已核准的架構計畫接介面，不新設計。

### T1＋T3a：ui_editor 改用 Engine 的選擇窗與新外殼（high，5～7 h）

改動：
- `Main.cpp` 改用 `RunHeadlessShell`：命令列錯誤時印 help，回 2。GUI 在 body 裡呼叫 `RunImGuiShell`，`options.platform` 填標題、1600×900、可調整大小。
- `EditorClient` 改實作 `ImGuiShellClient`；`SdlPathDialogs` 改收 `SdlPlatform&`。
- 資產預覽用 `NativeRenderer(context.renderer)`；CMake 把 `GYO::ToolFrameworkGui` 換成 `GYO::ToolFrameworkImGui`，另加 `GYO::PlatformSDL`、`GYO::RenderBackendSDLNative`。
- `PathDialog.hpp` 不再 include framework。

「答案不來」的恢復規則（提案，要你決定）：
- 每次 Show 給一個請求序號，tag＝序號；用途對照放在 ui_editor 的 adapter，是純函式，有單元測試。
- 只接受目前請求的答案：
  - 等待中，畫面顯示「Stop waiting」。按下後放棄這個請求、重新啟用 Browse 與選單；之後才到的答案丟掉，並記一行到 Diagnostics。
  - Failed 照舊開手動輸入框，但這個請求仍是目前請求。這樣 Windows 新式對話窗失敗後改開舊式時，舊式選的路徑仍會套用。
  - Chosen 或 Cancelled 結束請求；同一請求之後再來的答案丟掉。
- 不用逾時：對話窗可以合法地開很久。

判定（都通過才算完成）：
1. L1 的 ci-windows、ci-linux、ci-macos 三列綠；`gyo_ui_editor.*` 全部通過，含下列新測試（用 `ScriptedDialogs`）：
   - 放棄後才到的答案被丟掉；
   - Failed 之後的 Chosen 會套用；
   - 新請求讓舊答案失效；
   - 同一請求的第二個答案被丟掉；
   - 用途↔tag 對照的往返。
2. `EditorCliTests` 的結束碼不變：命令列錯誤 2、help 0、validate 的 3／4。
3. L3 實機（你操作；Windows 優先，macOS 也做）：
   - 開檔、匯出、選資料夾各選一次；
   - 取消；
   - 非 ASCII 路徑；
   - 對話窗開著時關視窗；
   - 對話窗開著時按 Stop waiting，再在對話窗裡選檔：不套用，狀態列與 log 的 `ui_editor.dialogs` 各有一行（2026-11-01 你同意，原本寫「Diagnostics 有一行」）；
   - 字型與輸入法的文字欄位照舊。
4. 範圍的限界（從架構批帶來，CI 量不到）：真實 OS 對話窗的回呼執行緒、Linux portal 要 pump、平台或父視窗銷毀時的行為。只靠第 3 項的實機觀察；Linux 沒有桌面機，這次不測。
5. 突變（全部預期 killed；file 與 find 在實作後、執行前寫進 `docs/tools/plans/v8/scripts/framework_mutations.json`）：
   - 拿掉序號檢查；
   - Failed 也放棄請求；
   - Stop waiting 不清等待；
   - 用途對照錯位；
   - 命令列錯誤不回 2；
   - `ui-guard-native-target` 改寫成 `gyo_ui_editor` 另連 `GYO::PlatformSDLNative`。

Design Invariants 的改寫（要你同意）：
- `docs/ui_toolchain.md` 的 ui_editor DI 1，改成：視窗與 ImGui 的 SDL 後端來自 framework 的 ImGui 綁定；選擇窗來自 Engine 平台層（`GYO::PlatformSDL`）；資產預覽建貼圖用原生 renderer（`GYO::RenderBackendSDLNative`），這是 platform-sdl Replacement Criteria 2 的例外，到 Engine 有貼圖 API 為止。
- DI 2 的「framework's host」改成「framework's ImGui binding」。

守衛變更（§14.4，Design Invariants 的守衛，要你核准）：
1. 守的性質：ui_editor DI 1，即不連遊戲、`GYO::Input`、SDL_GPU，依賴只走正向清單。
2. 緩的面：
   - `GYO::PlatformSDL`、`GYO::ToolFrameworkImGui`、`GYO::RenderBackendSDLNative` 從失敗變成通過；
   - 後者讓 ui_editor 直接拿到 SDL renderer handle。
3. 緊的面：`GYO::ToolFrameworkGui` 從通過變成失敗。
4. 緩了以後，性質由這些判定繼續守：
   - include 仍禁止 `engine/input/`；
   - 目標仍是正向清單，沒有遊戲、Input、GPU；
   - platform-sdl 的 G3 configure 檢查限制 `*Native` 只能 `$<LINK_ONLY:>` 外傳；（検証者更正：G3 只檢查 Engine 的後端，不涵蓋 ui_editor 自己的目標。ui_editor 內部連 `GYO::RenderBackendSDLNative` 時，PUBLIC 或 PRIVATE 都只靠守衛的目標清單和 review 把關）
   - platform-sdl.md 的 RC 2 把例外限定在「ImGui 綁定與 ui_editor 的資產預覽」。
5. 根據：架構計畫 PLAN §2.4 與 §5、platform-sdl.md RC 2；突變與検証者的結果見下方「實作後的結果」。

#### T1＋T3a 實作後的結果（2026-11-01）

- 提交：`12147016`（實作）、`6391485f`（突變目錄）、審査後的修正（介面卡改成可測的自由函式並移進預覽程式庫、log 加一行、補 include）。本機 macOS 的 `build/target/_build/framework`（全部工具）：ctest 67／67。
- 件數の突き合わせ：一致。
  - 宣告 10 條、執行 10 條、killed 10 條，stale 0：
    - 實作時宣告 7 條：執行器 6 條，`ui-cli-usage-exit` 手動 1 條；
    - 審査後再宣告 3 條；
    - 修正動到 EditorApp，所以重跑 `ui-dialog-stale-applied`。
  - 證據在使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T1-T3a/`：`mutations.txt`、`mutation-ui-cli-usage-exit.txt`、`mutations-review-fix.txt`。
- 和事前宣告不同的地方：
  - 被丟掉的答案記在狀態列與 log（`ui_editor.dialogs`），不是 Diagnostics。Diagnostics 每次由文件重建，記上去會消失。L3 第 3 項的寫法已改，經你同意。
  - 「用途對照錯位」改由 `ui-dialog-kind-mapping` 與 `ui-dialog-outcome-mapping` 檢查；另加 `ui-dialog-same-tag`、`ui-dialog-current-mapping`。
  - 規則測試直接測 `PathDialogRequests`，介面卡另有測試，`ScriptedDialogs` 只測 EditorApp 怎麼處理不是目前請求的答案。
  - `ui-cli-usage-exit` 的處理：
    - 執行器跑不了它：執行器只替換整個參數是 `{exe:…}` 的情況，而 `gyo_ui_editor` 輸出到 `build/target/toolchain/bin`；
    - 所以從突變目錄移除，改成手動跑，期待不變。
- 沒有測試覆蓋的一處：
  - `SdlPathDialogs::Abandon` 只轉呼 `PathDialogRequests::Abandon`，自動測試涵蓋不到；
  - 由 L3 第 3 項確認。
- 審査者（Opus／high）：
  - 沒有嚴重的正確性問題：生命週期、Show 的呼叫位置、過時答案、結束碼、CMake 都對。
  - 指出的介面卡測試與狀態列訊息被蓋掉兩件，已修。
  - 另一件記給 L3：Failed 之後 Browse 會重新啟用，Windows 改開的舊式對話窗還開著時，使用者可以再開一個。這時舊式的答案會被丟掉；或舊式的答案在手動輸入框開著時套用。這是核准的規則本身的結果。
- 検証者（Fable／high），執行紀錄在主 checkout `build/evidence/tool-v8-framework-rework/T1-T3a/verifier/run.log`：
  - 第 1、2、3、5 項成立。
    - 三個新目標從失敗變成通過，`GYO::ToolFrameworkGui` 從通過變成失敗；
    - 其他輸入兩版結果相同：Input、`*Native`、GPU、遊戲目標、`engine/input/`、SDL_GPU 符號都失敗；
    - 沒有沒列出的放寬。
  - 第 4 項部分成立：G3 不涵蓋 ui_editor，已在上方更正。
  - 既有的限制（這次沒有放寬）：
    - 目標的正則只看 `Name::Name`，沒有 `::` 的目標名不受檢；
    - include 允許整個 `engine/`，所以 `*Native.hpp` 只靠目標清單擋。

### T2：兩個工具改用 engine/io（medium～high，2～3 h）

改動：
- ui_editor 的 `DocumentSession`：
  - `ReadTextFile` 改成 `ReadAllBytes`，`maxBytes`＝64 MB；
  - `ProbeFileStamp` 改成 engine/io 的同名函式，同樣 64 MB；
  - `WriteTextFileAtomically` 改成 `WriteFileAtomically`，`createParentDirectories = true`，舊的也會建父目錄；
  - `IsPathWithin` 改成 `IsNativePathWithin`；
  - `PathExists` 改成 `Exists`；
  - `AbsolutePath` 照舊用 framework 核心的（D-2）。
- audio_editor：
  - `SoundExport` 與 `Main` 的 `info` 裡兩份讀檔迴圈，改成 `ReadAllBytes`，`maxBytes`＝64 MB；
  - `WriteReplacing` 改成 `WriteFileAtomically`，`createParentDirectories = true`，拿掉自己的 `CreateDirectories`；
  - 暫存檔名會從 `<path>.gyo-tmp` 變成 engine/io 的名稱；
  - 「輸出不得蓋掉來源或 catalog」的檢查照舊用 `Os::Files::ResolvedPath`。

行為的變化（寫進 dev_log）：
- 兩個工具讀的檔案上限都變成 64 MB；超過時回讀檔錯誤。原本沒有上限。以 48 kHz 立體聲 WAV 算，64 MB 約 3.9 分鐘（24-bit）或 5.8 分鐘（16-bit）。這是要你確認的一項；要更長的素材時，audio_editor 可以傳更大的上限。
- ui_editor 的匯出判定有 4 種情況答案改變（`tests/framework/UserFilesParityTests.cpp` 的輸出）：
  - 根目錄以分隔符結尾；
  - 根目錄是連結；
  - 根目錄的大小寫不同；
  - 連結後面接 `..`。

判定：
1. L1 三列綠；兩個工具的測試照舊通過。
2. 缺陷依 §14.3 處理：只給 `--asset-catalog`（根目錄 `.`）時，匯出到資產根目錄應被拒絕。
   - 先用新測試在舊程式上確認失敗（Red），存紀錄；換成 engine/io 後通過（Green）。
3. audio_editor 的輸出位元組與基準相同：
   - 基準：使用者的 Mac，主 checkout `build/evidence/tool-v8-audio-editor-edit/baseline/sha256.txt`，5 個 fixture；
   - 本批的比對結果存到 `build/evidence/tool-v8-framework-rework/T2/`。
4. 突變（全部預期 killed）：
   - ui_editor 的包含判定恆回 false：第 2 項的測試失敗；
   - ui_editor 不建父目錄：現有的匯出到新子目錄測試失敗，若沒有這個測試就補上；
   - audio_editor 不建父目錄：CLI 轉換到巢狀路徑失敗；
   - audio_editor 的 CMake 另連 `GYO::PlatformSDL`：守衛失敗，證明正向清單仍有效。

守衛變更：
- ui_editor 的守衛不變：已允許 `engine/` 與 `GYO::Engine`。
- audio_editor（§14.4，Design Invariants 的守衛，要你核准）：
  1. 守的性質：audio_editor DI 1，即不連遊戲、`GYO::Input`、SDL_GPU；播放只走 `GYO::Audio`。
  2. 緩的面：include 的 `engine/io/` 與目標 `GYO::Engine` 從失敗變成通過。`GYO::Engine` 含 io、asset 的程式碼，連 base、math、time，不連 SDL、Input、GPU（`engine/CMakeLists.txt`；検証者更正：io 與 asset 不是另外的目標，原本寫「連」）。
  3. 緊的面：無。
  4. 緩了以後，性質由這些判定繼續守：目標仍是正向清單；include 不允許 `engine/input/`、`engine/render/`、`engine/platform/`。
  5. 根據：PLAN §2.3 與 §5；突變與検証者的結果見下方「T2 實作後的結果」。

#### T2 實作後的結果（2026-11-01）

- 提交：`10c55014`。本機 macOS 全部工具：ctest 67／67。
- 件數の突き合わせ：一致。
  - 宣告 4 條，另重跑 8 條：find 在改動範圍裡，或殺它的測試被改。
  - 執行 12、killed 12、stale 0。
  - 證據在使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T2/mutations.txt`。
- §14.3：
  - 測試「根目錄以分隔符結尾時，匯出到它底下應被拒絕」在舊程式上失敗（`T2/red.txt`），換成 engine/io 後通過（`T2/green.txt`）。
  - 測試用 `assetRoot / ""` 掛載，和只給 `--asset-catalog` 時根目錄 `.` 的情況等價：兩者經 `AbsolutePath` 都帶結尾的 `/`。
- audio_editor 的輸出與基準相同：5／5 的 WAV 與 catalog（`T2/bytes/compare.txt`）。
- 包含判定在 ui_editor 實際改變答案的有 3 種情況，不是事前宣告寫的 4 種：
  - 實際改變的 3 種：根目錄以分隔符結尾、根目錄是連結、根目錄大小寫不同。
  - 沒有出現的「連結後面接 `..`」：
    - 原因：路徑在進入判定前，已被 `AbsolutePath`（D-2）按字面消掉 `..`；
    - 判定與寫入用的是同一條消過的路徑，所以仍然一致。（審査者指出）
- 沒列進事前宣告的行為變化（審査者指出，都很少見）：
  - 輸出檔在編輯器外被換成超過 64 MB 的檔案時，連「覆寫」也會因讀不了戳記而失敗。舊的沒有上限，能覆寫。
  - 掛載的根目錄若帶 `\\?\` 之類的前綴，每次匯出都會失敗，包括根目錄外的匯出。舊的會判「不在裡面」。
  - audio_editor 的訊息變了，結束碼不變：
    - 「cannot open」變成「cannot read」；
    - 建目錄與寫檔的錯誤都變成「cannot write」；
    - 沒有測試比對這些字串。
- os-subsystem 計畫的突變 `ui-x1`、`ui-x2` 改的是 `FileService::IsPathWithin`，ui_editor 不再呼叫它，所以現在存活（0／2，`T2/mutations-os-ui-x.txt`）。
  - 「經連結到達根目錄」這個性質，由這兩處判定守：
    - ui_editor 的測試「export rejects a path that reaches the asset root through a symbolic link」；在沒有建連結權限的 Windows 上會略過；
    - engine/io 的突變 `io-within-no-resolve`。
  - `ui-io-within-false` 只守「恆回 false」。
  - 這兩條突變要在 PR B 退役或改錨（交給架構會話）。
- 審査者（Opus／high）：沒有嚴重問題；戳記的語義、UTF-8 路徑、結束碼都不變。
- 検証者（Fable／high），紀錄在主 checkout `build/evidence/tool-v8-framework-rework/T2/verifier/run_log.txt`：
  - 第 1～5 項都成立，沒有沒列出的放寬；
  - 第 2 項的措辭已照它更正。

### T3b：audio_editor 改用 catalog 結構 API、JSON 綁定與無視窗外殼（high，2～3 h）

改動：
- `SoundExport` 改 include `gyo/framework/json/CatalogText.hpp`，呼叫新的 `RegisterAsset(text, RawCatalogEntry, {})`。
- 核心 CMake 連 `GYO::ToolFrameworkJson`。
- `Main` 改用 `RunHeadlessShell`；執行檔自己具名 `GYO::ToolFramework`。

行為的變化：更新既有條目時，保留它的其他欄位與排版（D-6）。舊的會丟掉其他欄位。

判定：
1. L1 三列綠；`gyo_audio_editor.cli` 的 `convert_taken_path` 仍回 4；其他結束碼不變。
2. 新測試：重新登錄已有額外欄位的條目時，額外欄位留下。
3. 輸出位元組與基準相同（同 T2 的第 3 項）。
4. 突變（全部預期 killed）：
   - 政策設 `allowSharedPath = true`：`convert_taken_path` 不再回 4；
   - `ae-guard-json-target` 改寫成 CMake 另連 `nlohmann_json::nlohmann_json`：守衛失敗。

Design Invariants 的改寫（要你同意）：audio_editor DI 3 的「unique ids」改成「catalog 內唯一；跨 catalog 與 shader 重疊在載入時由 ContentManifest 判定」。

守衛變更（§14.4，要你核准）：
1. 守的性質：DI 1 與 DI 3，即 catalog 只經 framework 寫入，工具不自己解析 JSON。
2. 緩的面：目標 `GYO::ToolFrameworkJson` 從失敗變成通過。
3. 緊的面：無。
4. 緩了以後，性質由這些判定繼續守：
   - `nlohmann_json` 仍不在清單；
   - JSON 綁定自己的守衛只准它具名 nlohmann。
5. 根據：PLAN §2.5 與 §5；突變與検証者的結果見下方「T3b 實作後的結果」。

#### T3b 實作後的結果（2026-11-01）

- 提交：`df24d71d`，以及審査後補的測試。本機 macOS 全部工具：ctest 67／67。
- 件數の突き合わせ：一致。
  - 宣告 4 條：執行器跑 `ae-guard-json-target`、`ae-guard-io-platform`；手動跑 `ae-catalog-policy`、`ae-catalog-old-api`。
  - 另重跑 `ae-io-no-parents`，因為 SoundExport 有改。
  - 執行 5、killed 5、stale 0。
  - 證據在使用者的 Mac，主 checkout `build/evidence/tool-v8-framework-rework/T3b/`：`mutations.txt` 與 `mutations-manual.txt`。
  - 手動的原因和 T1＋T3a 相同：只有 CLI 測試抓得到，執行器跑不了。
- `convert_taken_path` 仍回 4。
- 更新條目時保留其他欄位：新測試比對整份 catalog 文字；換回舊 API 時，這個測試失敗。
- 輸出與基準相同：5／5 的 WAV 與 catalog（`T3b/bytes/compare.txt`）。
- 沒列進事前宣告、變嚴格的行為（審査者指出）：
  - 方向和 Engine 的 `ContentManifest` 一致：舊的會寫出一個載入時就被拒絕的 catalog。
  - catalog 裡已有條目的路徑不是內容路徑時，任何登錄都回 4，舊的回 0。不是內容路徑的例子：`../x`、`/x`、`a\\b`、`a//b`、結尾 `/`、`content.json`、含控制字元。
    - 補了測試「ExportSound refuses a catalog whose existing entry has no content path」；
    - 用舊 API 跑這個測試會失敗，紀錄附在 `mutations-manual.txt`。
  - `--path` 含控制字元時回 4，舊的回 0。
  - 條目裡重複寫了 `type` 或 `path` 鍵時，無法更新，回 4，舊的回 0。這是極端情況。
- 審査者（Opus／high）：
  - 其他行為都相同：版本檢查、重複 id、BOM、CRLF、拒絕的先後、結束碼的對應、無視窗外殼的命令列錯誤。
  - 執行檔不再連 `GYO::Os`，也沒有地方再用到它。
- 検証者（Fable／high），紀錄在主 checkout `build/evidence/tool-v8-framework-rework/T3b/verifier/run.log`：
  - 第 1～5 項都成立，沒有沒列出的放寬。
  - 在編譯層確認：`GYO::ToolFrameworkJson` 以 PRIVATE 連 nlohmann，audio_editor 的編譯命令沒有 nlohmann 的 include 目錄，`#include <nlohmann/json.hpp>` 根本找不到。
  - 既有的限制，兩版守衛都一樣：沒有斜線的 include（例如 `<json.hpp>`）不受檢；守衛沒有符號規則。

`-edit` 的工程檔要不要讓 audio_editor 自己連 nlohmann：T3b 用不到，改到 `-edit` 開工、設計工程檔時決定。

## `v8-tool-editor-engine-access`（T4，2026-11-01；已實作並核准）

起因：架構會話（例行架構掃描）轉述，使用者 2026-11-01 同意改寫 ui_editor 與 audio_editor 的 DI 1。依據：
- 「no `GYO::Input` and no SDL_GPU」沒有架構理由。
  - 這條禁令來自 10-30 的 `v8-tool-ui-invariants`，當時只寫了結論、沒寫理由。
  - 10-31 的 audio_editor DI 1 與 framework DI 2 都是照抄。
- `GYO::Input` 只連 `GYO::Base`，是中立的 Engine 能力。禁止它和「Editor → Engine」的方向相反。
- 這條禁令已經造成兩份 Input→Ui 的轉換：PvP 與 ui_editor 的預覽各一份。

使用者的規則（經架構會話轉述的原話）：「Editor 可以連結 framework 和 Engine，但是 framework 是通用層，共通功能優先 framework。如果有觀察到有多處使用，即使 Engine 有也先實作 framework，再調用。」

分工：
- 兩份工具的 DI 與兩支守衛由本線負責。
- framework 的 DI 2、它的守衛、Input→Ui 轉換器由 framework 整改卡負責。
- 三份 DI 的措辭要和整改卡對齊後，才請你核准。

模型與檔位：
- 段：上段（改 Design Invariants 與守衛）。
- 実装者 Opus 5.5／high；審査者 Opus 5.5／high；検証者 Fable 5.1／high。
- 檢驗的做法照前三批：先跑突變，再由検証者實際核對。

### 實作（`863f6325` 與審査後的收緊）

- 兩份 DI 1 的定稿是 `docs/ui_toolchain.md` 與 `docs/audio_editor/architecture.md` 的現行文字。共同句和整改卡的 framework DI 2 草稿一致。
- ui_editor 的 DI 1 另外保留舊文的三句：視窗、ImGui 後端、選擇窗由誰提供；並把守衛的內容寫進去。
- ui_editor 的程式：
  - 預覽程式庫改連 `GYO::RenderBackendSDLNative`，不再直接連 `SDL3::SDL3`；
  - `EditorWindow` 的 log 改走 `Base::Log`。
- 守衛不掃 `tests/ui_editor`，照現狀，待你決定。預覽測試直接用 SDL 與 ImGui 的 SDL renderer 後端。

### §14.4（兩支守衛都是 Design Invariants 的守衛，要你核准）

ui_editor（`tests/ui_editor/ui_editor_dependency_guard.py`）：
1. 守的性質：新的 DI 1。
   - 不連遊戲、不點名其他產品；可用任何 Engine 模組；
   - SDL 只照遊戲的路徑取得；
   - 唯一的原生使用是預覽的 renderer。
2. 緩的面：include `engine/input/` 與目標 `GYO::Input` 從失敗變成通過。
3. 緊的面：
   - 目標 `SDL3::SDL3` 失敗；
   - `SDL3/` include、Engine 原生層標頭（`/native/` 或 `Native.h*`）、任何 `SDL_` 名稱，只准出現在 4 個原生檔案：`AssetPreviewContext.hpp/.cpp`、`PreviewAdapter.cpp`、`EditorWindow.cpp`；
   - 在這 4 個檔案裡，原生標頭只准 `SdlRendererNative.hpp`，`SDL_` 名稱只准貼圖與 renderer 的 API。
   - 以前 SDL 標頭與 `SDL_` 名稱到處都能用，只有 `SDL_*GPU*` 會失敗。
4. 緩了以後，性質由這些判定繼續守：
   - 遊戲的目標與 include 仍失敗（正向清單）；
   - `SDL_*GPU*` 在任何檔案都失敗；
   - `GYO::PlatformSDLNative` 等原生目標不在清單。
5. 根據與證據：
   - 架構會話轉述你同意的方向；
   - 突變：先 9／9，審査後收緊再 8／8，全部 killed；
   - 証據在主 checkout `build/evidence/tool-v8-framework-rework/T4/mutations.txt` 與 `mutations-review-fix.txt`。

audio_editor（`tests/audio_editor/audio_editor_dependency_guard.py`）：
1. 守的性質：新的 DI 1。
   - 不連遊戲、不點名其他產品；可用任何 Engine 模組；
   - 播放經 `GYO::Audio` 與它的 SDL 後端；
   - 工具自己不碰 SDL。
2. 緩的面：任何 `engine/` 開頭的 include 與目標 `GYO::Input` 從失敗變成通過。以前只准 base、os、audio、io。
3. 緊的面：任何 `SDL_` 名稱與 Engine 原生層標頭都失敗。以前只禁 `SDL_*GPU*`。
4. 緩了以後，性質由這些判定繼續守：
   - `SDL3/` include、`SDL3::`、`*Native` 目標、遊戲的目標與 include 仍失敗；
   - 検証者確認：Engine 的公開標頭裡，原生層以外的都不 include SDL，也不帶 SDL 型別；所有 SDL 後端都以 PRIVATE 連 SDL3。
5. 根據與證據：同上。突變 `ae-guard-input-target` 因為 `GYO::Input` 已被允許，改寫成 `ae-guard-game-target`。

### 審査與檢驗

- 検証者（Fable／high），紀錄在主 checkout `build/evidence/tool-v8-framework-rework/T4/verifier/run-log.txt`：
  - 兩支守衛的第 1～5 項都成立。
  - 指出沒列出的差異：
    - `Native.h`（非 `.hpp`）結尾的標頭抓不到。這是理論上的放寬，Engine 現在沒有這種檔；已修，現在連同其他副檔名都會判失敗。
    - 註解或字串裡的 `SDL_` 也會判失敗。這是誤判的風險，屬於收緊；現有程式裡沒有，所以保留。
    - audio_editor 的 include 範圍比目標清單寬：目標清單只加了 `GYO::Input`，其他 Engine 目標要用時再加。
- 審査者（Opus／high）：
  - 沒有正確性或建置問題。
  - 原生檔案的例外原本是整個檔案，比 DI 寫的寬；已收緊成只准貼圖與 renderer 的 API，以及 `SdlRendererNative.hpp`。
  - 草稿和實際提交不一致；本節已改成實際內容。
  - `/native/` 那個判斷，在現有的 include 寫法下永遠不會成立，原生標頭靠檔名判斷；守住的其實是目標清單。
  - 守衛只看名稱，用 `auto` 可以繞過，這是既有的限制。
  - DI 裡「共用功能先進 framework」是靠審查守的規則，沒有守衛。
  - audio_editor 的 DI 說播放經 SDL 後端，但 `GYO::AudioBackendSDL` 不在目標清單裡（既有），`-edit` 需要時再加。
- 要通知整改卡：framework 的突變「CMake names GYO::Input」在 framework DI 2 改寫後會存活。

### 決定

1. 兩份 DI 1 的改寫與兩支守衛的變更：2026-11-01 核准（`v8-tool-editor-engine-access-approve`）。
2. ui_editor 守衛是否掃 `tests/ui_editor`：未定，照現狀不掃。

## `v8-tool-ui-editor-input-adapter`（IU-2）的事前宣告（2026-11-02 核准）

來源：任務卡 `v8-input-ui-adapter`（已核准 `v8-input-ui-adapter-approve`，卡在 `docs/object_fps_pvp/plans/v8/input-ui-adapter-card.md`）。IU-2 由工具線做。

前提：IU-1 已合併（PR #135，`a8a48243`）。本批從 master `a8a48243` 開分支 `claude/v8-tool-ui-editor-input-adapter`。

用到的新介面：
- `framework/include/gyo/framework/UiTextInput.hpp` 的 `ToUiTextInput(events, shortcut, clipboard)`；
- `framework/bindings/imgui/include/gyo/framework/imgui/ImGuiShell.hpp` 的 `ImGuiShellContext::input`：`Frame()` 交出這一幀的 `PhysicalInputFrame`，`RequestTextInput(area, cursorPixels)` 要求文字輸入。

模型與檔位：

| 段 | 根據 | 実装者 | 審査者 | 検証者 |
|---|---|---|---|---|
| 上段 | ui_editor DI 1 的守衛變更（收緊） | Opus 5.5／high（主會話） | Opus 5.5／high（子 agent，code review） | 只在出現放寬時開，Opus 5.5／high |

- 守衛只收緊：§14.4 規定只收緊時不必另開検証者，改用突變證明新判定抓得到。
- 這是 Design Invariants 的守衛，第 1～5 項仍照 §14.4 交你核准。

估時：4～6 h（卡上的推論）。超過 1.5 倍就停下回報。

### 改動

1. **`PreviewAdapter`：文字輸入改用 framework 的轉換器。**
   - 刪掉從 ImGui 字元佇列與 `ImGuiKey` 表拼文字輸入的那段（`PreviewTextInput`）。
   - 改成 `ToUiTextInput(frame.text, shortcut, clipboard)`：
     - `shortcut` 依 `ImGui::GetIO().ConfigMacOSXBehaviors` 選 Gui 或 Ctrl，和現在的預覽一致：macOS 用 Cmd；
     - `clipboard` 用 `ImGui::GetClipboardText`。
   - 組字事件（Composition）因此進得了預覽的 text_field。這解決了卡上第 3 點：預覽收不到組字。
2. **刪掉 `imgui_internal.h` 與 `PlatformImeData` 那段，改由預覽交出「文字輸入請求」。**
   - 請求的內容是編輯中欄位的視窗座標範圍與游標像素位置，放在 `FrameOutput`。
   - EditorApp 轉交給 EditorClient，EditorClient 在 `Draw` 裡呼叫 `context.input.RequestTextInput`。
   - 這樣 `PreviewAdapter` 不必知道 ImGui 綁定，測試可以直接建構 `TextEvent` 交給它（卡上的介面要求 e）。
3. **輸入幀的傳遞：** EditorClient 在 `BeginFrame` 把 `context.input.Frame()` 交給 EditorApp，EditorApp 再交給 `PreviewAdapter::Draw`。新增一個參數，型別是 `const Engine::Input::PhysicalInputFrame*`，nullptr 表示沒有輸入。
4. **不改的部分（範圍的限界）：** 預覽的指標、Tab、方向鍵、Enter、Space、Escape 仍從 ImGui 讀。
   - 卡上 IU-1 只交出文字輸入的轉換器，沒有 `UiInputFrame` 的共用轉換。
   - 把這些也改走 Engine 的幀，要先有共用的轉換，記為之後的候選。
5. **守衛（收緊）：** ui_editor 守衛加一條具名規則，任何檔案都不得 include `imgui_internal.h`。
   - 現在守衛會略過不含 `/` 的 include，所以抓不到它。
6. **CMake：** `gyo_ui_editor_preview` 用到 `PhysicalInputFrame`（`GYO::Input`）與 `ToUiTextInput`（`GYO::ToolFramework`），兩個都已在守衛的允許清單裡。

### 判定（都通過才算完成）

1. L1 的 ci-windows、ci-linux、ci-macos 三列綠；`gyo_ui_editor.*` 全部通過。
2. 新測試，用 `TextEvent` 直接建構，不經 SDL：
   - 打字插入；
   - 組字時 text_field 顯示組字，確定後變成文字；
   - shortcut＋A 全選、shortcut＋V 貼上，Gui 與 Ctrl 兩種都測；
   - Backspace 與方向鍵；
   - 編輯中才交出文字輸入請求，範圍是欄位的視窗座標；不在編輯時不交出。
3. 既有測試 `TestPreviewTextFieldTyping` 原本用 ImGui 的鍵盤事件餵輸入，要改成餵 `TextEvent`。
   - 判定不變：送出的文字、動作紀錄、繫結值都和原本相同；只改輸入的來源。
   - 依 §14.4 在變更說明寫理由。
4. 突變（全部預期 killed；file 與 find 在實作後、執行前寫進 `docs/tools/plans/v8/scripts/framework_mutations.json`）：
   - 丟掉組字事件；
   - shortcut 固定成 Ctrl；
   - 不在編輯也交出請求；
   - 編輯時不交出請求；
   - 守衛的 `imgui_internal.h` 規則失效：自我測試失敗；
   - 預覽又 include `imgui_internal.h`：守衛失敗。
5. L3（macOS 實機，你操作；這次只驗 macOS，你已決定）：
   - 預覽模式下在 text_field 打英文；
   - 用日文輸入法組字，預覽裡顯示組字，確定後進入欄位；
   - Cmd＋A、Cmd＋V；
   - Inspector 的 ImGui 輸入框照舊能用日文輸入；預覽編輯和 Inspector 編輯切換時，輸入法不會亂跳。

### 範圍的限界

- **Windows 不驗：** Windows 的組字要開 `SdlPlatformOptions::imeComposition`，這次只驗 macOS（你的決定，見卡）。
- **預覽的其他輸入照舊從 ImGui 讀：** 指標與導覽鍵，見上面第 4 點。
- **CI 不驗真的輸入法：** CI 只跑 `cpu|shader`，真實輸入法只靠 L3。

### §14.4（守衛只收緊；實作後補結果，交你核准）

1. 守的性質：ui_editor DI 1 與卡上第 2 點「不伸手進 ImGui 內部」。
2. 緩的面：無。
3. 緊的面：任何 ui_editor 檔案 include `imgui_internal.h` 都失敗。以前不含 `/` 的 include 一律略過。
4. 不適用（沒有放寬）。
5. 根據：任務卡 IU-2；突變結果實作後補上。

### IU-2 實作後的結果（2026-11-02）

- 提交：`945f27ed`（實作）、`e1d2dcce`（回歸修正），之後是測試補強。本機 macOS 全部工具：ctest 67／67，守衛通過。
- 件數の突き合わせ：一致。
  - 宣告 11 條，全部 killed；最後一輪 13／13 killed，stale 0。
  - 經過見 dev_log：兩次是突變或期待寫錯；一次是測試的寫法讓按鍵和選取沒有真的發生，補強後 killed。
  - 證據在使用者的 Mac，主 checkout `build/evidence/tool-v8-ui-editor-input-adapter/`。
- 回歸（第一次審查找到，依 §14 停下，你選「(a) 在工具內修」）：
  - 現象：預覽欄位編輯中，編輯器的快捷鍵和方向鍵微移會一起觸發。
  - 修正：`TextInputActive()` 合起 ImGui 自己的欄位與預覽欄位兩種情況，用它暫停快捷鍵與微移。
  - §14.3：先 Red（`red.txt`）後 Green（`green.txt`）；回歸測試在第二次審查後改成檢查實際效果。
- 和事前宣告不同的地方：
  - 新增公開函式 `EditorApp::SetPreviewMode`（作用等同 View 選單的 Preview interaction）與 `TextInputActive`。前者供回歸測試使用，後者是修正本身。
  - L3 加一項：預覽欄位編輯中按 Cmd＋Z、Cmd＋D 或方向鍵，文件不會被改動。
- 範圍的限界多一項：`cursorPixels` 一直是 0，輸入法的候選窗不會隨游標移動，和舊程式相同。

§14.4（ui_editor 守衛，只收緊；這是 Design Invariants 的守衛；2026-11-02 核准，`v8-tool-ui-editor-input-guard-approve`）：
1. 守的性質：ui_editor DI 1 與卡上第 2 點「不伸手進 ImGui 內部」。
2. 緩的面：無。
3. 緊的面：任何 ui_editor 檔案 include `imgui_internal.h` 都失敗，不管有沒有路徑。以前不含 `/` 的 include 一律略過。
4. 不適用（沒有放寬）。沒有放寬的證明：舊守衛判失敗的輸入，新守衛照樣判失敗，因為只多了一條規則、其餘沒動，自我測試也全部通過。
5. 根據：
   - 任務卡 IU-2；
   - 突變 `ui-guard-imgui-internal`：預覽又 include 時，守衛失敗；
   - 突變 `ui-guard-imgui-internal-ignored`：規則失效時，自我測試失敗；
   - 兩條都 killed（`mutations.txt`）。
   - 已知漏洞：`imstb_*.h` 和巨集形式的 include 不受檢；風險低，因為要碰 ImGui 的私有狀態需要 `imgui_internal.h`。

## 實作順序（提案）

```text
v8-tool-assets-plan                     「high｜Opus 5.5｜本規劃（草稿）」
│
├── audio-tool-line                     「PR 線：音效從腳本走到工具」
│   ├── v8-tool-audio-editor-convert    「high（重新取樣局部 xhigh）｜Opus 5.5｜核心程式庫＋CLI：讀 WAV、轉換（演算法等價）、輸出並登錄 catalog」
│   ├── v8-tool-framework-shell         「medium～high｜Opus 5.5｜工具共用層 tools/framework/：從 ui_editor 搬出外殼四塊，行為不變」（PR #123，已合併）
│   └── v8-tool-audio-editor-edit       「high｜Opus 5.5｜GUI：播放、波形、資訊、裁剪、增益、工程檔」（排在 framework-rework-line 之後）
│
├── framework-rework-line               「PR #123 的整改，工具側（2026-11-01 依架構整改計畫重排）：全部等架構的 PR A 合併」
│   ├── v8-tool-ui-editor-path-dialog-adopt「high｜Opus 5.5｜T1：ui_editor 改呼叫 Engine 的選擇窗（和 T3a 一起做）」
│   ├── v8-tool-ui-editor-shell-adopt     「high｜Opus 5.5｜T3a：ui_editor 接新外殼與 ImGui 綁定」
│   ├── v8-tool-user-files-adopt          「medium～high｜Opus 5.5｜T2：兩個工具改用 engine/io」
│   ├── v8-tool-audio-editor-catalog-adopt「high｜Opus 5.5｜T3b：audio_editor 改用 catalog 結構 API＋JSON 綁定、無視窗外殼」
│   ├── v8-tool-editor-engine-access    「high｜Opus 5.5｜T4：兩個工具的 DI 1 改成『可用任何 Engine 模組、SDL 只經 Engine 或 framework』，守衛跟著改」（已實作並核准）
│   └── v8-tool-ui-editor-input-adapter 「high｜Opus 5.5｜IU-2：ui_editor 預覽的文字輸入改用 framework 的轉換器與文字輸入請求」（已實作，守衛收緊已核准，L3 macOS 通過）
│
├── anim-events-line                    「PR 線：動畫事件從無到手工，再到工具」
│   ├── v8-tool-assets-anim-events      「high｜Opus 5.5｜手工：gyo.anim_events 契約＋PvP 手寫資料＋Runtime 排程；移除過渡規則（產品批）」
│   └── v8-tool-model-viewer            「high｜Opus 5.5｜工具：模型資訊、骨骼動畫預覽、事件時刻的編輯」（事前宣告草稿在 model-viewer.md，等核准）
│
├── （延後）v8-tool-assets-pack          「Asset build：打包成自訂二進位包」
└── （待定）SpriteSheet、視頻、文字與劇情 「出現需求時從手工開始」
```

| 代號 | 檔位｜引擎 | 估時 | 完成的樣子 |
|---|---|---|---|
| `v8-tool-audio-editor-convert` | high（局部 xhigh）｜Opus 5.5 | 6～10 h | 工具登錄（`tools.csv`、`project.json`）、核心程式庫與 CLI；照上方事前宣告通過；catalog 條目正確；突變檢查 |
| `v8-tool-framework-shell` | medium～high｜Opus 5.5 | 3～5 h | `tools/framework/` 有 ImGui＋SDL 宿主、系統選擇窗、檔案存取、唯讀 catalog 掛載、catalog 寫入（從音效工具搬入）；ui_editor 改用它、測試全過；Design Invariants＋守衛；`framework` 加進保留名 |
| `v8-tool-audio-editor-edit` | high｜Opus 5.5 | 8～12 h | 播放、波形、裁剪、增益、工程檔；使用者在 GUI 上試過。排在 T3b 之後（T1～T3b 都合併）；事前宣告已核准（上方），分支 `claude/v8-tool-audio-editor-edit` 只有宣告的提交（本機） |
| `v8-tool-ui-editor-path-dialog-adopt`（T1） | high｜Opus 5.5 | 和 T3a 合計 5～7 h | 等 PR A 合併（含 `v8-engine-platform-path-dialog`）；和 T3a 一起做，因為舊宿主交不出 `SdlPlatform&`（架構計畫 D-4）。`SdlPathDialogs` 改收 `SdlPlatform&`，只留用途↔tag 對照與每幀套用；`PathDialog.hpp` 不再 include framework；CMake 具名 `GYO::PlatformSDL`；ui_toolchain.md Design Invariant 1 改寫（要你同意）。`pathDialogOpen_`（`EditorApp.cpp`）加「答案不來」時的恢復規則：Engine 新的 Design Invariant 10 只保證最多一個答案。守衛加 `GYO::PlatformSDL`（放寬，§14.4 出示 1～5 並經你核准）。L3 實機（Windows 優先）：選檔、取消、非 ASCII 路徑、對話窗開著時關視窗、答案不來。事前宣告帶入架構批的範圍限界：真實 OS 對話窗的回呼執行緒、Linux portal 的 pump、平台或父視窗銷毀時的行為，CI 都量不到 |
| `v8-tool-ui-editor-shell-adopt`（T3a） | high｜Opus 5.5 | （見 T1） | 等 PR A 合併（含 `v8-framework-shell`）。GUI 用 `RunImGuiShell`，cli 用 `RunHeadlessShell`（命令列錯誤印 help、回 2）；資產預覽用 `NativeRenderer(context.renderer)`，ui_editor 自己連 `GYO::RenderBackendSDLNative`（platform-sdl Replacement Criteria 2 的新例外，Engine 的貼圖 API 另案）。守衛加 `GYO::ToolFrameworkImGui`、`GYO::RenderBackendSDLNative`，拿掉 `GYO::ToolFrameworkGui`（§14.4）；突變 `ui-guard-native-target` 改寫。GUI 由你試過 |
| `v8-tool-user-files-adopt`（T2） | medium～high｜Opus 5.5 | 2～3 h | 等 PR A 合併（含 `v8-engine-io-user-files`）。ui_editor 的 `DocumentSession` 改呼叫 engine/io；audio_editor 的讀寫改用 `ReadAllBytes` 與 `WriteFileAtomically`（暫存檔名會變）。`ProbeFileStamp` 與 `ReadAllBytes` 都明確傳 `maxBytes`（預設 64 MB）。`AbsolutePath` 留在 framework 核心（架構計畫 D-2），ui_editor 繼續用那一份。audio_editor 現在完全沒用 `FileService`，改用 engine/io 是新依賴：CMake 具名 `GYO::Engine`，守衛加 `engine/io/` 與 `GYO::Engine`（放寬，§14.4）；ui_editor 的守衛預期不變，開工前確認。兩個工具的測試照舊通過 |
| `v8-tool-audio-editor-catalog-adopt`（T3b） | high｜Opus 5.5 | 2～3 h | 等 PR A 合併（含 `v8-framework-catalog-structure` 與 `-extra-fields`）。CMake 具名 `GYO::ToolFrameworkJson`；`RegisterAsset` 的參數改 Engine 的 `RawCatalogEntry`，登錄政策用預設值；CLI 的 `convert_taken_path` 仍回 4。Main 改 `RunHeadlessShell`。守衛加 `GYO::ToolFrameworkJson`（§14.4），突變 `ae-guard-json-target` 改寫。audio_editor Design Invariant 3 的「unique ids」改寫成「catalog 內唯一；跨 catalog 與 shader 重疊在載入時由 ContentManifest 判定」（要你同意）。`-edit` 的工程檔用 JSON 要不要把 nlohmann 加回守衛，在這批一起決定 |
| `v8-tool-assets-anim-events` | high｜Opus 5.5 | 5～8 h | `gyo.anim_events` 的契約與 Design Invariants（Engine 多一個格式，Architecture Delta）、PvP 把事件名稱對到音效、Runtime 排程、移除過渡規則；使用者實聽 |
| `v8-tool-model-viewer` | high｜Opus 5.5 | 12～20 h（`-event-sounds` 另加 3～5 h） | 模型資訊、骨骼動畫預覽、事件時刻的編輯。2026-11-04 事前宣告草稿：[model-viewer.md](model-viewer.md)，分成 `-core`、`-preview`、`-events`、`-event-sounds`（等 PR #145）、`-trial`；等你核准，沒有實作 |

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
| `v8-tool-framework-rework-replan` | （2026-11-01）架構整改計畫核准後的工具側重排，取代 `v8-tool-framework-rework-split` 的批次與依賴：架構側改成 7 批（`v8-framework-guard-ratchet`、`v8-engine-platform-path-dialog`、`v8-engine-io-user-files`、`v8-framework-shell`、`v8-framework-catalog-structure`、`v8-framework-catalog-extra-fields` 在 PR A；刪舊件的 `v8-framework-invariants` 在 PR B）。工具側 T3 拆成 T3a（ui_editor）與 T3b（audio_editor），T1～T3b 都等 PR A 合併，T1 和 T3a 一起做；PR A 不動舊件，合併前工具不會壞。PR B 等 T1～T3b 都合併。catalog 規則改 (a)＋b-lite（架構計畫 D-1：Engine 公開 `IsRelativeContentPath` 給 framework 呼叫），取代 `-split` 的 (a)。工具側預計一條 PR。計畫在 `docs/architecture/plans/framework-rework/`（README 決策 D-1～D-9、PLAN 第 2 節各批末尾的「工具側」） | 經架構會話轉述（不是原話）：使用者 2026-11-01 核准該計畫的 PLAN 第 1 版與 D-1～D-9，全部採建議 |
| `v8-tool-framework-rework-predeclare` | （2026-11-01）T1＋T3a、T2、T3b 的事前宣告（本文件「framework-rework-line 的事前宣告」）核准：恢復規則、64 MB 上限、Design Invariants 改寫的方向、各批判定與突變；検証者改用 Fable 5.1／high。三個守衛的放寬仍要在突變與検証者核對後，各自再經你核准（§14.4） | 對「事前宣告和模型表照草稿核准嗎？」選「検証者改用 Fable」 |
| `v8-tool-ui-editor-guard-approve` | （2026-11-01）核准 ui_editor 守衛的放寬（§14.4：加 `GYO::PlatformSDL`、`GYO::ToolFrameworkImGui`、`GYO::RenderBackendSDLNative`，拿掉 `GYO::ToolFrameworkGui`；第 4 項依検証者更正），並同意 L3 第 3 項改成「狀態列與 log 各有一行」 | 「兩項都核准」 |
| `v8-tool-audio-editor-guard-approve` | （2026-11-01）核准 audio_editor 守衛的放寬（§14.4：加 include 前綴 `engine/io/` 與目標 `GYO::Engine`，T2） | 「核准」 |
| `v8-tool-audio-editor-json-guard-approve` | （2026-11-01）核准 audio_editor 守衛的放寬（§14.4：加目標 `GYO::ToolFrameworkJson`，T3b） | 「核准」 |
| `v8-tool-editor-engine-access-approve` | （2026-11-01）核准 T4：ui_editor 與 audio_editor 的 DI 1 改寫，以及兩支守衛的變更（§14.4 的 1～5 見 T4 一節），並在 L1 通過後合併 PR #131。ui_editor 守衛是否掃 `tests/ui_editor` 未定，照現狀不掃 | 對「核准 T4，並在 L1 通過後合併 #131 嗎？」答「核准並合併」 |
| `v8-tool-ui-editor-input-adapter-predeclare` | （2026-11-02）IU-2 的事前宣告（本文件「`v8-tool-ui-editor-input-adapter`（IU-2）的事前宣告」）核准：改動、判定、突變、L3 與範圍的限界、模型表 | 對「IU-2 的事前宣告照草稿核准嗎？」答「照草稿核准」 |
| `v8-tool-ui-editor-input-guard-approve` | （2026-11-02）核准 IU-2 的 ui_editor 守衛收緊：任何檔案都不得 include `imgui_internal.h`（§14.4 的 1～5 見 IU-2 實作後的結果） | 對「守衛的收緊（不得 include imgui_internal.h）核准嗎？」答「核准」 |
| `v8-tool-assets-toolchain` | （2026-10-31）工具鏈，不是一個工具：沒有統一的來源與輸出位置；工具加工素材 → Asset 登錄；Asset build（自訂二進位包）延後；照「手工 → 腳本 → 工具」。第一版草稿的製作設定格式、處理器、Build 階段，以及 `source-root`、`output`、`processor-home`、`processor-language` 四個待決，都撤下 | 「這個是一條工具鏈而不是一個工具。……關鍵是現在的工具。（手工-腳本-工具）這個順序。」 |
