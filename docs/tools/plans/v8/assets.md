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

## `v8-tool-audio-editor-edit` 的事前宣告（§14.1，2026-10-31 使用者「照草稿核准」；2026-11-03 開工時更新）

設定是裁剪（原檔的幀）與增益（dB）。試聽與匯出用同一個 `RenderClip`，依序是裁剪 → 增益 → 重新取樣 → 量化。全部是確定性的判定，不重試。

1. **回歸**：預設設定（不裁剪、0 dB）下，匯出的位元組和改動前的 `-convert` 相同。
   - 基準：五個測試用 WAV 以 id `golden.<檔名>` 轉換的結果，SHA-256 存在主 checkout 的 `build/evidence/tool-v8-audio-editor-edit/baseline/sha256.txt`。
   - 基準由 `18f5c726` 的音效工具產生。T2、T3b 都和它比對過，5／5 相同。
2. **裁剪**：
   - 48 kHz 的原檔不重新取樣，量化前的值等於原檔的那一段，完全相等。
   - 其他取樣率的輸出幀數等於 `floor(裁剪後幀數 × 比例)`。
3. **增益**：
   - 48 kHz 的原檔，量化前的值等於 `原值 × 10^(dB/20)`，完全相等。
   - +6 dB 推到滿格時，飽和計數等於實際被截斷的樣本數。
4. **試聽等於匯出**：同一組設定下，送進 `LoadClip` 的位元組和 `ExportSound` 寫出的 data chunk 相同。
5. **工程檔**：存了再讀回內容不變；存到資產根目錄裡、或版本不認識時拒絕。
6. **波形**：合成輸入的每一欄最小值與最大值正確。
7. **播放狀態機**：不開裝置，由測試的執行緒自己呼叫 `Render`。
   - 播放後進到 Playing，`clipFrame` 前進。
   - 停止生效後是 NotPlaying，之後的區塊都是靜音。
   - 槽數 2、連續換 10 次片段，`NoClipSlot` 不會拖過下一幀。
   - 舊的 handle 每次都被停掉。
8. **GUI**：使用者在 macOS 上試用。

### 2026-11-03 開工時的更新（`v8-tool-audio-editor-edit-restart`）

- **前提**：GYO::Audio 的播放控制層（`SoundHandle`、`Stop`、`LoadClip`、`PlayheadOf` 等）要先進 master。
  - 它目前只在 audio-line 的本機分支 `claude/v8-engine-audio-control`（`1f271a32`，比 master 多 8 個提交），沒有推送，也沒有 PR。
  - 你選了建議做法：由 audio-line 會話推送、開 PR、合併之後，本批才開工實作。
- **外殼**：GUI 用 framework 的 `RunImGuiShell`，命令列照舊用 `RunHeadlessShell`。`SdlAudioOutput` 由工具自己開，外殼不管音訊。
- **守衛**（§14.4，放寬；開工實作後先跑突變、經検証者核對，再交你核准）：
  - 目標加 `GYO::ToolFrameworkImGui`、`GYO::AudioBackendSDL`、`imgui::core`、`nlohmann_json::nlohmann_json`。
  - 原本核准的 `GYO::ToolFrameworkGui` 已在 PR B 刪除，換成 `GYO::ToolFrameworkImGui`。
  - `GYO::AudioBackendSDL` 是「把 SDL 留在私有的 Engine SDL 後端」，符合 T4 的 DI 1。
  - `SDL3::` 與 `*Native` 照舊禁止。
- **工程檔**：照建議採 (a)。audio_editor 自己連 nlohmann 讀寫工程檔。
  - 理由：目前只有 audio_editor 一處用到這個格式，照「共用功能先進 framework」，要觀察到多處使用才放進 framework。
  - ui_editor 也是自己連 nlohmann。
- **模型與檔位**（領航員規則，照建議）：

| 段 | 根據 | 実装者 | 審査者 | 検証者 |
|---|---|---|---|---|
| 上段 | Design Invariants 的守衛放寬；新的資料格式（工程檔） | Opus 5.5／high（主會話） | Opus 5.5／high | Fable 5.1／high |

### 實作後的結果（2026-11-04）

- 前提：GYO::Audio 播放控制層已合併（PR #139，merge `8a713b1e`），公開介面和 10-31 的 `1f271a32` 相同；本分支已和 master 合併（`a9bbe1fb`）。
- 判定 1～7：本機 macOS 全部通過（`build/target/_build/framework`，ui_editor＋audio_editor）。判定 8 交你在 GUI 上試用。
  - 判定 1：五個 WAV 以 `golden.<檔名>` 轉換，SHA-256 和基準 5／5 相同（主 checkout `build/evidence/tool-v8-audio-editor-edit/regression/compare.txt`）。
  - 判定 2～7：`tests/audio_editor/AudioEditorEditTests.cpp`。
- 突變：26／26 killed，見下方「突變結果」。
- 和事前宣告不同的地方：
  - 裁剪後一幀都不剩（重新取樣前或後）時拒絕（`InvalidSettings`），所以不會寫出 Engine 的 `ClipBank` 拒收的空片段。舊的 `convert` 遇到 0 幀的 WAV 會寫出空片段，現在改成回 4。
  - 試聽的混音器沒有排程延遲、兩個片段槽（`PreviewMixerConfig`），停止與卸載在取走它們的那個區塊就生效。
  - 指令 `edit [<project.json> | <source.wav>]` 開視窗；不帶指令時照舊回 2（CLI 測試不變）。
- 範圍的限界：
  - GUI 的邏輯（欄位與工程檔之間的轉換、對話框）沒有自動測試，只靠 L3。
  - 重新取樣在 UI 執行緒上做，長的非 48 kHz 素材按下 Play 會停頓一下。
  - 沒有「未存檔」的提示；關視窗時改動會丟失。
  - 命令列給相對路徑時，工程檔照原樣存相對路徑。
- 觀察到的壓力（報告，不在本批處理）：系統選擇窗的「只認最新一次請求、Failed 不結束」規則，ui_editor（`PathDialogRequests`）和 audio_editor 各寫了一份。照「共用功能先進 framework」，兩處都用到了，可以考慮移進 framework。

§14.4（audio_editor 守衛，放寬；這是 Design Invariants 的守衛，要你核准）：

1. 守的性質：audio_editor DI 1。不連遊戲、不指名其他產品；自己不 include `SDL3/`、不呼叫 `SDL_`、不指名 SDL 或 `*Native` 的 target。SDL 只經 Engine 的 SDL 後端與 framework 的綁定。
2. 緩的面：以前判失敗、現在通過的情況：
   - CMake 指名 `GYO::ToolFrameworkImGui`、`GYO::AudioBackendSDL`、`imgui::core`、`nlohmann_json::nlohmann_json`；
   - include 前綴 `nlohmann/`：nlohmann 目錄下的每個標頭都通過（`json.hpp`、`json_fwd.hpp`、`detail/*` 等，全是 JSON 函式庫自己的檔案）。
   - `<imgui.h>` 不含 `/`，本來就不檢查。
   - 前綴比對的既有弱點：`nlohmann/../SDL3/SDL.h` 也會通過，但 `engine/../SDL3/SDL.h` 以前就通過，實質上不是新的面。
   - 突變 `ae-guard-json-target`（指名 nlohmann 時守衛失敗）因此刪除。
3. 緊的面：無。
4. 緩的面之後，性質由誰守：
   - 本守衛的其餘規則沒動：`SDL_` 名稱、`SDL3/` 與 native 標頭的 include、`SDL3::` 與 `*Native` 的 target、名單外的 target 與 include 都照舊失敗。
   - `GYO::AudioBackendSDL` 自己把 SDL 留在私有、沒有 native target：由 Engine 的 G1～G3 守（`docs/architecture/audio.md` DI 2）。
   - ImGui 綁定把 SDL、ImGui 的 SDL 後端與 `*Native` 留在私有：寫在 `framework/bindings/imgui/CMakeLists.txt`，只靠審查守。framework 的守衛只查名稱，設定時的連結檢查不分 PUBLIC／PRIVATE，G3 也不涵蓋這個綁定。
     - 綁定經公開連結帶來的 `GYO::PlatformSDL` 與 `GYO::RenderBackendSDL`，SDL 是私有的，由 G3 守。所以現在沒有 SDL3 的 include 目錄進到 audio_editor。
     - 如果綁定哪天改成公開連結，SDL3 標頭與 native 標頭就會進到 audio_editor 的 include 路徑，這時只剩本守衛的 `SDL3/`、`*Native` 與 `SDL_` 規則在守。
     - 新的事實：SDL3 現在會（只以連結的形式）連進 GUI 執行檔，手寫一個 `SDL_` 的宣告也連得過；擋住它的只有本守衛的 `SDL_` 規則。
   - `imgui::sdl3_sdlrenderer3` 不在名單內。
   - nlohmann 是 JSON 函式庫，不帶 SDL，也不帶遊戲。
5. 根據：
   - 事前宣告（2026-11-03 的目標清單，上方）；
   - 自我測試新增兩條：視窗與工程檔的目標通過；在 SDL 後端旁再指名 `SDL3::SDL3` 或 `imgui::sdl3_sdlrenderer3` 時失敗；
   - 突變：見下方「突變結果」；
   - 検証者：見下方「検証者」。
   - 已知的一點：視窗用 `SdlPlatform` 的系統選擇窗，`GYO::PlatformSDL` 經 ImGui 綁定的公開連結取得（綁定 → 視窗外殼 → 平台，兩層），CMake 不指名它（突變 `ae-guard-io-platform` 證明指名時守衛照舊失敗）。`docs/audio_editor/architecture.md`「Editing and preview」與 CMake 註解寫明了這一點。

突變結果（主 checkout `build/evidence/tool-v8-audio-editor-edit/`）：
- 件數の突き合わせ：一致。宣告 26 條（新 22、改寫位置 3、沿用 1），最後一輪 26／26 killed，stale 0（`mutations.txt`）。
- 第一輪 22／26（`mutations-run1.txt`），survived 4 條都是核心測試的突變，不是守衛的：
  - 3 條（試聽）：測試名稱有逗號，doctest 的 `-tc` 篩選在逗號切開，一個測試都沒跑。這是測試方法的錯，改了測試名稱。
  - 1 條（`ae-edit-empty-trim-accepted`）：重新取樣後的空片段檢查擋下了同一個情況，`ValidateSettings` 本身沒有被直接測到。這是測試的缺口，補了直接的檢查。
  - 另外 `ae-preview-update-stops` 即使篩選正常也抓不到（沒有測試在播放中呼叫 `Update`），補了這個檢查。
- 刪除 1 條：`ae-guard-json-target`（第 2 點）。改寫位置 3 條（`ae-io-no-parents`、`ae-guard-native-include` 移到 `FileAccess.cpp`；`ae-guard-io-platform` 的連結行變了），判定不變。

検証者（Fable 5.1／high，2026-11-04；不改檔案，用舊守衛與新守衛對 51 個合成輸入比對）：
- 第 1、3、5 點：確認。
- 第 2、4 點：確認，附更正，已照更正改寫在上面（`nlohmann/` 的範圍；ImGui 綁定的私有連結只靠審查；SDL3 以連結的形式進到 GUI 執行檔）。
- 措辭的提醒：DI 1 的「names no SDL or `*Native` target」裡，`GYO::AudioBackendSDL` 的名字也有 SDL。這裡把「SDL target」讀成 SDL3 自己的 target，同一句的「Playback goes through `GYO::Audio` and its SDL backend」支持這個讀法。要不要改成「no SDL3 target」由你決定；改 DI 本身是 Architecture Delta。
- 既有的弱點（不是本批造成的）：不帶目錄的 `#include <SDL.h>` 兩版守衛都放過（用到時會被 `SDL_` 規則抓到）；CMake 註解裡的 `::` 名稱也會被當成 target。

## `v8-tool-audio-editor-regions` 的事前宣告（§14.1，草稿，2026-11-04，等你核准）

**功能**：從一個原檔切出多段，每段是一個片段，一次匯出、各自登錄，並可以淡入淡出。使用的情境：把換彈的錄音切成 `magazine_out`、`magazine_in`、`bolt_pull` 等，每個動作一個事件、一段聲音（聲音側的 B，`v8-tool-anim-event-sound-owner`）。

**依據**：架構評估（[請求](anim-event-sound-request.md)，結論見該文件「評估結果」）。
- 「事件 → 聲音」由 framework 持有（架構卡 `v8-arch-event-sounds-contract`）；
- audio_editor 只管片段，不知道事件，也不寫 event_sounds 檔（DI 8 不動）；
- 本批不依賴架構卡，也不依賴 PR #142（架構評估引用的條文，2026-11-04 仍未合併）。

**前提**：#141（`-edit`）合併後才開工，分支從 master 開。

**不做**：
- 拼接與時間軸：各段之間沒有先後，也不放在同一條時間上；
- 多軌與混音：Replacement Criteria 的第 2 條不碰；
- 事件名稱、對齊點、event_sounds 檔。

### 改動

- **區段**（`Edit.hpp`）：`EditSettings` 加 `fadeIn`、`fadeOut`，單位是原檔的幀（和裁剪相同）。
  - `RenderPlanes` 的順序：裁剪 → 增益 → 淡入淡出 → 重新取樣 → 量化。
  - 淡入淡出用直線。設裁剪後有 n 幀，第 i 幀（0 起算）的係數：
    - 淡入 `in(i)`：`fadeIn` 為 0 或 `i ≥ fadeIn` 時是 1，否則是 `i / fadeIn`；
    - 淡出 `out(i)`：令 `j = n − 1 − i`，`fadeOut` 為 0 或 `j ≥ fadeOut` 時是 1，否則是 `j / fadeOut`；
    - 量化前的值是 `(x × g) × in(i) × out(i)`（double，照這個順序乘）。
  - `ValidateSettings` 加一條：`fadeIn + fadeOut ≤ n`。
- **多段匯出**（`SoundExport.hpp`）：新增 `ExportSounds`，參數是一個原檔、一個目的地和多個區段（每段有 id、path 與設定）。原本的 `ExportSound` 改成呼叫它（一段）。
  1. 先全部檢查：
     - 至少一段；
     - 同一次匯出裡 id 不重複、path 不重複；
     - path 不等於原檔或 catalog；
     - 每段的設定都有效，重新取樣後也至少一幀。
  2. 全部在記憶體裡算好；catalog 的文字在記憶體裡連續套用 `RegisterAsset`，每段一次。
  3. 逐一寫片段（atomic replace），最後寫一次 catalog。
  - 第 1、2 步失敗時什麼都不寫。
  - 第 3 步寫第 k 段失敗時，catalog 不動，前 k−1 段的檔案留在資產根目錄、沒有登錄（和現在單段匯出時 catalog 寫入失敗的情形相同；範圍的限界）。
- **工程檔 version 2**（`ProjectFile.hpp`）：
  - 欄位：`format`、`version`（2）、`source`、`asset_root`、`asset_catalog`、`regions`；
  - `regions` 是陣列，每段剛好有 `id`、`path`、`trim_start`、`trim_end`、`gain_db`、`fade_in`、`fade_out`；
  - 讀到 version 1 時當成一段、淡入淡出為 0；存檔一律寫 version 2；
  - 其他規則照舊：欄位一個不多一個不少、同一個工程永遠寫出同一段文字、存進資產根目錄時拒絕。
- **命令列**：新增 `export <project.json>`，不開視窗、照工程檔匯出全部區段（手工 → 腳本 → 工具，AGENTS §5；判定也用它在無視窗下跑）。`convert` 不變。
- **視窗**：
  - 區段列表：從目前的裁剪新增一段、選取、改 id 與 path、刪除；
  - 編輯欄位作用在選取的那段；
  - 波形畫出所有區段，選取的那段畫出淡入淡出；
  - 試聽播選取的那段；
  - 「Export all」匯出全部區段。

### 判定（都通過才算完成；全部是確定性的，不重試）

1. **回歸**：一段、淡入淡出為 0 時：
   - 五個測試用 WAV 匯出的位元組和基準 SHA-256 相同（`build/evidence/tool-v8-audio-editor-edit/baseline/sha256.txt`）；
   - `-edit` 存下的 version 1 工程檔，讀進來匯出，結果和 `-edit` 匯出的相同。
2. **淡入淡出**：48 kHz 的原檔，量化前的值和上面的公式完全相等。
   - 淡入大於 0 時第一幀是 0，淡出大於 0 時最後一幀是 0；
   - 淡入淡出都是 0 時和沒有這兩個欄位時相同；
   - `fadeIn + fadeOut > n` 時拒絕（`InvalidSettings`）。
3. **多段匯出**：一個原檔切三段，一次匯出：
   - 每個片段和單獨匯出那一段的位元組相同；
   - catalog 多了或更新了這三個條目；
   - 其他條目的文字位元組不變。
4. **拒絕時什麼都不寫**：下列各情形，資產根目錄裡不多任何檔案，catalog 的位元組不變：
   - id 重複、path 重複；
   - path 等於原檔；
   - 其中一段的設定無效（包括排在最後的那段）；
   - 零段。
5. **工程檔 version 2**：
   - 存了再讀回內容不變；
   - version 1 讀成一段；
   - 版本不認識、多一個欄位、少一個欄位時拒絕。
6. **試聽等於匯出**：三段中的第二段，送進 `LoadClip` 的位元組和匯出的 data chunk 相同（DI 6）。
7. **GUI**：你在 macOS 上試用：
   - 切三段、聽到淡入淡出；
   - 一次匯出；
   - 存工程檔、重開後區段都在。

**突變**（預計 12 條，加進 `framework_mutations.json`，只跑本批相關的，D184）：
- 淡入：係數差一幀；
- 淡出：方向反了；
- 淡入淡出放到重新取樣之後；
- 不檢查 `fadeIn + fadeOut ≤ n`；
- 不檢查重複的 id；
- 不檢查重複的 path；
- 檢查晚於寫入（寫完前面幾段才檢查後面的）；
- catalog 只登錄最後一段；
- version 1 轉換時丟掉增益；
- version 2 接受多出的欄位；
- 試聽固定播第一段；
- `export` 命令只匯出第一段。

### Design Invariant 3 的措辭（要你同意；§3、§14.4）

現在的字面是單數：「the output file and the catalog entry with the same id」。多段匯出時，一次匯出會寫出多個檔案與多個條目，所以照字面會違反這條。

- **改成**：「The editor writes only what it registers: for each exported clip, the output file and the catalog entry with the same id; one export may write several clips. It leaves every other entry, every other file and product data … untouched.」（後半照舊。）
- §14.4 的 1～5：
  1. **守的性質**：工具只寫它登錄的東西；其他條目、其他檔案、產品的資料都不碰。
  2. **緩的面**：一次匯出寫出多個片段與多個條目（每段一個）。以前照字面算違反，改了之後算通過。
  3. **緊的面**：
     - 同一次匯出裡 id 或 path 重複時拒絕（以前沒有這個概念）；
     - 任何一段無效時什麼都不寫。
  4. **守的性質是否仍有其他判定守著**：有。`AudioEditorCoreTests.cpp` 的「ExportSound keeps an existing entry's other fields …」與「… writes nothing for a bad source」繼續守單段；判定 3（其他條目不變）與判定 4（拒絕時什麼都不寫）把同一個性質延伸到多段。
  5. **根據**：架構評估的第 4 點（「多段匯出等於多個輸出各自登錄」）、你選的 B、本節的判定 3 與 4。
- 程序：改 DI 的字面要你同意；本批的守衛（相依性守衛）不變。開工實作後跑突變，並在交你核准前由検証者用實際的資料核對上面的 1～5。

### 範圍的限界

- 寫到一半失敗時，已寫的片段不會登錄（見「改動」第 3 步）。
- GUI 的邏輯沒有自動測試，只靠判定 7。
- 重新取樣仍在 UI 執行緒上做；多段匯出會停頓得更久。
- 每段都從原檔重新算，沒有快取。

### 模型與檔位（領航員規則）

| 段 | 根據 | 実装者 | 審査者 | 検証者 |
|---|---|---|---|---|
| 上段 | Design Invariant 3 的措辭；工程檔的格式變更（version 2） | Opus 5.5／high（主會話） | Opus 5.5／high | Fable 5.1／high（DI 3 的 §14.4） |

估時 8～12 h（不含 L3）。

## 實作順序（提案）

```text
v8-tool-assets-plan                     「high｜Opus 5.5｜本規劃（草稿）」
│
├── audio-tool-line                     「PR 線：音效從腳本走到工具」
│   ├── v8-tool-audio-editor-convert    「high（重新取樣局部 xhigh）｜Opus 5.5｜核心程式庫＋CLI：讀 WAV、轉換（演算法等價）、輸出並登錄 catalog」
│   ├── v8-tool-framework-shell         「medium～high｜Opus 5.5｜工具共用層 tools/framework/：從 ui_editor 搬出外殼四塊，行為不變」（PR #123，已合併）
│   ├── v8-tool-audio-editor-edit       「high｜Opus 5.5｜GUI：播放、波形、資訊、裁剪、增益、工程檔」（PR #141）
│   └── v8-tool-audio-editor-regions    「high｜Opus 5.5｜一個原檔切多段、一次匯出、淡入淡出」（事前宣告草稿；#141 合併後）
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
│   └── v8-tool-model-viewer            「high｜Opus 5.5｜工具：模型資訊、骨骼動畫預覽、事件時刻的編輯」
│
├── （延後）v8-tool-assets-pack          「Asset build：打包成自訂二進位包」
└── （待定）SpriteSheet、視頻、文字與劇情 「出現需求時從手工開始」
```

| 代號 | 檔位｜引擎 | 估時 | 完成的樣子 |
|---|---|---|---|
| `v8-tool-audio-editor-convert` | high（局部 xhigh）｜Opus 5.5 | 6～10 h | 工具登錄（`tools.csv`、`project.json`）、核心程式庫與 CLI；照上方事前宣告通過；catalog 條目正確；突變檢查 |
| `v8-tool-framework-shell` | medium～high｜Opus 5.5 | 3～5 h | `tools/framework/` 有 ImGui＋SDL 宿主、系統選擇窗、檔案存取、唯讀 catalog 掛載、catalog 寫入（從音效工具搬入）；ui_editor 改用它、測試全過；Design Invariants＋守衛；`framework` 加進保留名 |
| `v8-tool-audio-editor-edit` | high｜Opus 5.5 | 8～12 h | 播放、波形、裁剪、增益、工程檔；使用者在 GUI 上試過。事前宣告已核准，2026-11-03 開工時更新（上方）；等 GYO::Audio 播放控制層合併後才實作 |
| `v8-tool-audio-editor-regions` | high｜Opus 5.5 | 8～12 h | 多段匯出、淡入淡出、工程檔 version 2、`export` 命令；DI 3 的措辭改成複數（要你同意）；使用者在 GUI 上試過。事前宣告在上方（草稿） |
| `v8-tool-ui-editor-path-dialog-adopt`（T1） | high｜Opus 5.5 | 和 T3a 合計 5～7 h | 等 PR A 合併（含 `v8-engine-platform-path-dialog`）；和 T3a 一起做，因為舊宿主交不出 `SdlPlatform&`（架構計畫 D-4）。`SdlPathDialogs` 改收 `SdlPlatform&`，只留用途↔tag 對照與每幀套用；`PathDialog.hpp` 不再 include framework；CMake 具名 `GYO::PlatformSDL`；ui_toolchain.md Design Invariant 1 改寫（要你同意）。`pathDialogOpen_`（`EditorApp.cpp`）加「答案不來」時的恢復規則：Engine 新的 Design Invariant 10 只保證最多一個答案。守衛加 `GYO::PlatformSDL`（放寬，§14.4 出示 1～5 並經你核准）。L3 實機（Windows 優先）：選檔、取消、非 ASCII 路徑、對話窗開著時關視窗、答案不來。事前宣告帶入架構批的範圍限界：真實 OS 對話窗的回呼執行緒、Linux portal 的 pump、平台或父視窗銷毀時的行為，CI 都量不到 |
| `v8-tool-ui-editor-shell-adopt`（T3a） | high｜Opus 5.5 | （見 T1） | 等 PR A 合併（含 `v8-framework-shell`）。GUI 用 `RunImGuiShell`，cli 用 `RunHeadlessShell`（命令列錯誤印 help、回 2）；資產預覽用 `NativeRenderer(context.renderer)`，ui_editor 自己連 `GYO::RenderBackendSDLNative`（platform-sdl Replacement Criteria 2 的新例外，Engine 的貼圖 API 另案）。守衛加 `GYO::ToolFrameworkImGui`、`GYO::RenderBackendSDLNative`，拿掉 `GYO::ToolFrameworkGui`（§14.4）；突變 `ui-guard-native-target` 改寫。GUI 由你試過 |
| `v8-tool-user-files-adopt`（T2） | medium～high｜Opus 5.5 | 2～3 h | 等 PR A 合併（含 `v8-engine-io-user-files`）。ui_editor 的 `DocumentSession` 改呼叫 engine/io；audio_editor 的讀寫改用 `ReadAllBytes` 與 `WriteFileAtomically`（暫存檔名會變）。`ProbeFileStamp` 與 `ReadAllBytes` 都明確傳 `maxBytes`（預設 64 MB）。`AbsolutePath` 留在 framework 核心（架構計畫 D-2），ui_editor 繼續用那一份。audio_editor 現在完全沒用 `FileService`，改用 engine/io 是新依賴：CMake 具名 `GYO::Engine`，守衛加 `engine/io/` 與 `GYO::Engine`（放寬，§14.4）；ui_editor 的守衛預期不變，開工前確認。兩個工具的測試照舊通過 |
| `v8-tool-audio-editor-catalog-adopt`（T3b） | high｜Opus 5.5 | 2～3 h | 等 PR A 合併（含 `v8-framework-catalog-structure` 與 `-extra-fields`）。CMake 具名 `GYO::ToolFrameworkJson`；`RegisterAsset` 的參數改 Engine 的 `RawCatalogEntry`，登錄政策用預設值；CLI 的 `convert_taken_path` 仍回 4。Main 改 `RunHeadlessShell`。守衛加 `GYO::ToolFrameworkJson`（§14.4），突變 `ae-guard-json-target` 改寫。audio_editor Design Invariant 3 的「unique ids」改寫成「catalog 內唯一；跨 catalog 與 shader 重疊在載入時由 ContentManifest 判定」（要你同意）。`-edit` 的工程檔用 JSON 要不要把 nlohmann 加回守衛，在這批一起決定 |
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
| `v8-tool-framework-rework-replan` | （2026-11-01）架構整改計畫核准後的工具側重排，取代 `v8-tool-framework-rework-split` 的批次與依賴：架構側改成 7 批（`v8-framework-guard-ratchet`、`v8-engine-platform-path-dialog`、`v8-engine-io-user-files`、`v8-framework-shell`、`v8-framework-catalog-structure`、`v8-framework-catalog-extra-fields` 在 PR A；刪舊件的 `v8-framework-invariants` 在 PR B）。工具側 T3 拆成 T3a（ui_editor）與 T3b（audio_editor），T1～T3b 都等 PR A 合併，T1 和 T3a 一起做；PR A 不動舊件，合併前工具不會壞。PR B 等 T1～T3b 都合併。catalog 規則改 (a)＋b-lite（架構計畫 D-1：Engine 公開 `IsRelativeContentPath` 給 framework 呼叫），取代 `-split` 的 (a)。工具側預計一條 PR。計畫在 `docs/architecture/plans/framework-rework/`（README 決策 D-1～D-9、PLAN 第 2 節各批末尾的「工具側」） | 經架構會話轉述（不是原話）：使用者 2026-11-01 核准該計畫的 PLAN 第 1 版與 D-1～D-9，全部採建議 |
| `v8-tool-framework-rework-predeclare` | （2026-11-01）T1＋T3a、T2、T3b 的事前宣告（本文件「framework-rework-line 的事前宣告」）核准：恢復規則、64 MB 上限、Design Invariants 改寫的方向、各批判定與突變；検証者改用 Fable 5.1／high。三個守衛的放寬仍要在突變與検証者核對後，各自再經你核准（§14.4） | 對「事前宣告和模型表照草稿核准嗎？」選「検証者改用 Fable」 |
| `v8-tool-ui-editor-guard-approve` | （2026-11-01）核准 ui_editor 守衛的放寬（§14.4：加 `GYO::PlatformSDL`、`GYO::ToolFrameworkImGui`、`GYO::RenderBackendSDLNative`，拿掉 `GYO::ToolFrameworkGui`；第 4 項依検証者更正），並同意 L3 第 3 項改成「狀態列與 log 各有一行」 | 「兩項都核准」 |
| `v8-tool-audio-editor-guard-approve` | （2026-11-01）核准 audio_editor 守衛的放寬（§14.4：加 include 前綴 `engine/io/` 與目標 `GYO::Engine`，T2） | 「核准」 |
| `v8-tool-audio-editor-json-guard-approve` | （2026-11-01）核准 audio_editor 守衛的放寬（§14.4：加目標 `GYO::ToolFrameworkJson`，T3b） | 「核准」 |
| `v8-tool-editor-engine-access-approve` | （2026-11-01）核准 T4：ui_editor 與 audio_editor 的 DI 1 改寫，以及兩支守衛的變更（§14.4 的 1～5 見 T4 一節），並在 L1 通過後合併 PR #131。ui_editor 守衛是否掃 `tests/ui_editor` 未定，照現狀不掃 | 對「核准 T4，並在 L1 通過後合併 #131 嗎？」答「核准並合併」 |
| `v8-tool-ui-editor-input-adapter-predeclare` | （2026-11-02）IU-2 的事前宣告（本文件「`v8-tool-ui-editor-input-adapter`（IU-2）的事前宣告」）核准：改動、判定、突變、L3 與範圍的限界、模型表 | 對「IU-2 的事前宣告照草稿核准嗎？」答「照草稿核准」 |
| `v8-tool-ui-editor-input-guard-approve` | （2026-11-02）核准 IU-2 的 ui_editor 守衛收緊：任何檔案都不得 include `imgui_internal.h`（§14.4 的 1～5 見 IU-2 實作後的結果） | 對「守衛的收緊（不得 include imgui_internal.h）核准嗎？」答「核准」 |
| `v8-tool-audio-editor-edit-restart` | （2026-11-03）`-edit` 開工時的更新：(1) GYO::Audio 播放控制層由 audio-line 會話推送、開 PR、合併後才實作；(2) 工程檔 audio_editor 自己連 nlohmann；(3) 模型表：上段，実装者／審査者 Opus 5.5／high、検証者 Fable 5.1／high；守衛目標的 `GYO::ToolFrameworkGui` 換成 `GYO::ToolFrameworkImGui` | 「いいです、意見を受け入れます」（接受三項建議） |
| `v8-tool-anim-event-sound-owner` | （2026-11-04）「事件 → 聲音」由 framework 持有，是組合格式（建議形狀 `gyo.event_sounds`，架構卡 `v8-arch-event-sounds-contract`）；Engine 不動；聲音側選 B（每個動作一個事件、一段聲音）。audio_editor 只管片段（DI 8 不動），下一批是多段匯出（`v8-tool-audio-editor-regions`）。PvP 的 `sfx.json` 遷移由產品會話在契約落地後做 | 經架構會話轉述（不是原話）：使用者 2026-11-04 決定 |
| `v8-tool-assets-toolchain` | （2026-10-31）工具鏈，不是一個工具：沒有統一的來源與輸出位置；工具加工素材 → Asset 登錄；Asset build（自訂二進位包）延後；照「手工 → 腳本 → 工具」。第一版草稿的製作設定格式、處理器、Build 階段，以及 `source-root`、`output`、`processor-home`、`processor-language` 四個待決，都撤下 | 「這個是一條工具鏈而不是一個工具。……關鍵是現在的工具。（手工-腳本-工具）這個順序。」 |
