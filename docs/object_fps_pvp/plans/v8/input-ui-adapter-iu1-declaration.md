# IU-1 的事前宣告（framework 核心的 Input→Ui adapter）

更新：2026-11-02。**狀態：完成**（2026-11-02）。執行：v8-input-ui-adapter 推動會話（使用者 2026-11-01 決定）。底：master `d03c01e2`（PR B #134 合併後）。卡：`input-ui-adapter-card.md`。

核准前不寫程式。

## 要解決的事

| 卡的事項 | IU-1 做的部分 |
|---|---|
| 1. 同一個轉換寫兩份 | framework 核心交出唯一一份「TextEvent → UiTextInput」轉換，遊戲（IU-3）與預覽（IU-2）都呼叫它 |
| 2. 編輯器伸手進 ImGui 內部 | ImGui 綁定交出「要求文字輸入」，IU-2 改呼叫它後就能刪 `imgui_internal.h` |
| 3. 預覽沒有組字 | ImGui 綁定把它已有的 `SdlInput` 幀（含 Composition）交給 client |

## 介面（草案）

### 核心：`gyo/framework/UiTextInput.hpp`（target `GYO::ToolFramework`）

```cpp
// Which key means which edit (Ui Design Invariant 7: the product's mapping),
// shared by every product that calls the framework. Text → Insert,
// Composition → Composition (text, cursor), Ctrl+A → SelectAll,
// Ctrl+V → Insert(clipboard()), Backspace, Delete, Left, Right, Home, End.
std::vector<Engine::Ui::UiTextInput>
ToUiTextInput(std::span<const Engine::Input::TextEvent> events,
              const std::function<std::string()>& clipboard);
```

- 只依 `GYO::Input`、`GYO::Ui`，不碰 ImGui，也不碰 `SdlInput`（framework 整改線的要求）。
- 剪貼簿由呼叫者傳入：遊戲用 `platform->ClipboardText()`（`PvpApplication.cpp:279`），預覽之後用平台的剪貼簿。
- 對照表逐項照搬遊戲現在的那一份（`PvpApplication.cpp:268-289`），行為不變。
- **Ui DI 7**：DI 7 說「哪個鍵代表哪種編輯是產品的對照」。對照放在 framework，由產品選擇呼叫它；不放進 Ui。Ui 的條文不改。

### ImGui 綁定：`ImGuiShellContext` 加一個成員

```cpp
// The window's input as Engine's SDL input backend reads it, one frame per
// shell frame. Read from BeginFrame through EndFrame.
class ImGuiShellInput {
public:
    // This frame's PhysicalInputFrame: the same snapshot ImGui's composition
    // overlay reads, taken before the client's BeginFrame.
    virtual const Engine::Input::PhysicalInputFrame& Frame() const = 0;
    // Asks for text input over area (window coordinates, as SdlInput's
    // SetTextInputArea) for this frame only. Call every frame while editing.
    virtual void RequestTextInput(const Engine::Platform::Sdl::Rect& area, int cursorPixels) = 0;
};
struct ImGuiShellContext final { ...; ImGuiShellInput& input; };
```

各會話提的要求，以及草案怎麼回應：

| 要求（誰提的） | 草案 |
|---|---|
| 文字輸入只能有一個 owner；client 有要求以 client 為準（工具線 a、#132 會話） | 綁定在 `ImGui::Render()` 之後套用：這一幀 client 有要求，就呼叫 `SetTextInput(true)` 和 `SetTextInputArea`；client 上一幀有要求、這一幀沒有，就交還給 ImGui：ImGui 這幀不要文字輸入時才 `SetTextInput(false)`。誰在要求就是誰的；同一幀兩邊都要時以 client 為準 |
| 座標系寫明（工具線 b） | 視窗座標（和 `SdlInput::SetTextInputArea` 相同，不是像素）。HiDPI 的換算由呼叫者做 |
| 幀含 Composition，綁定照舊把事件交給 ImGui（工具線 c） | `Frame()` 是綁定既有的同一份 Snapshot；事件照舊送進 ImGui，client 取幀不會拿走事件 |
| BeginFrame、Draw 都能取，每幀只交一次（工具線 d） | Snapshot 在 `client_->BeginFrame()` 之前取（`ImGuiShell.cpp` 的 Update），從 BeginFrame 到 EndFrame 都是這一幀；每個 SDL 事件只出現在一幀 |
| 測試時能直接建構，不經 ImGuiShellContext（工具線 e） | `ToUiTextInput` 吃的是 `TextEvent` 的 span，測試直接建構 `PhysicalInputFrame` 就能用 |
| 不另建 `SdlInput`、共用 Snapshot（#132 會話、framework 整改線） | 照辦 |
| 覆蓋層不重複畫（#132 會話） | 覆蓋層照舊依 ImGui 回報的 `WantVisible` 判斷。IU-2 刪掉預覽寫 `PlatformImeData` 的那段之後，預覽編輯時覆蓋層就不畫 |

## 等你決定（`v8-input-ui-adapter-iu1-frame-scope`）

卡上寫「`UiInputFrame` 也是兩份」。查證後，兩份的內容其實不同：

- 遊戲用上／下鍵移焦點（`PvpApplication.cpp:1151-1152`）。
- 預覽用 Tab／左右鍵移焦點、空白鍵也能啟動，指標還要從畫布座標換算（`PreviewAdapter.cpp:345-367`）。

這些都是「哪個鍵代表什麼」，依 Ui DI 7 屬於產品。

- **(a) 只共用文字轉換（建議）**：`UiInputFrame` 的其他欄位留給產品自己填。卡的第 1 件事改記為「重複的是文字轉換，已合為一份；其他欄位是各產品的對照，不算重複」。
- **(b) 連 `UiInputFrame` 一起共用**：framework 要提供「鍵 → 焦點／啟動」的設定，等於把兩個產品的差異寫成一套設定。只有兩個使用者就做成通用設定，違反 AGENTS §1（單一用途不建通用抽象）與 §9（不建可寫任意條件的設定）。

## 守衛（§14.4，交使用者核准）

- **改什麼**：`tests/framework/framework_dependency_guard.py` 的 `CORE["targets"]` 加 `GYO::Ui`。
- **守住的性質**：核心只連列出的 Engine target，不連第三方。
- **放寬面**：核心連 `GYO::Ui`，以前判失敗，之後通過。
- **收緊面**：無。
- **第 4 點（被放寬的性質是否仍有別的判定守著）**：`GYO::Ui` 私下連了 nlohmann（`engine/ui/CMakeLists.txt:13-16`）。PRIVATE 連結在連結檢查裡是 LINK_ONLY，不會算成核心連 nlohmann。這點要實際 configure 一次確認，並由検証者用資料核對。
- **自測**新增兩條，寫法參考 `test_input_targets_in_the_core_pass`：
  - 「核心可以連 `GYO::Ui`」；
  - 「ImGui 綁定不可以連 `GYO::Ui`」：綁定的清單不加它，綁定需要時經核心取得。

## 驗收（結果出來前宣告；判定事後不放寬）

1. **對照測試**（`tests/framework/UiTextInputTests.cpp`，標籤 `cpu`，L1 會跑）：
   - 測試裡逐字放一份遊戲現在的轉換表作為參考，用同一串 `TextEvent` 餵兩邊，要求輸出逐項相同。
   - 這串事件包含：Text、Composition（cursor 0、-1、中間）、空的 Composition、Ctrl+A、Ctrl+V（剪貼簿有字、空字串）、Backspace、Delete、方向鍵、Home、End、沒有對照的鍵（例如 F1）、AZERTY 的 layoutKey 和實體鍵不同的情形、key repeat。
   - 另加一條：沒有 Ctrl 的 A、V 是一般的鍵，不產生 SelectAll 或 Insert。
2. **ImGui 綁定測試**（`tests/framework/ImGuiShellTests.cpp`，標籤 `editor`，只在本機跑，L1 不跑，這是限界）：
   - client 在 BeginFrame 和 Draw 讀到的 `Frame()` 是同一份；
   - client 一幀都沒要求文字輸入時，綁定不呼叫 `SetTextInput`；
   - 要求、再停止之後，ImGui 不要文字輸入時會關掉。
   - 能不能在不開視窗的情況下觀察到 `SetTextInput` 被呼叫，要先查證；觀察不到就寫成限界，不硬做。
3. **守衛**：
   - 自測全過；
   - 真實守衛通過；
   - 一個探測：核心連 `GYO::Ui` 時，configure 與守衛都通過，且連結檢查不報 nlohmann。
4. **全量**：建置成功、CTest 全過（macOS arm64）。L1 三平台以 PR 的 CI 為準。
5. **突變**：目錄 `scripts/framework_mutations.json`（D184：只跑這批相關的）。下面每一條都要 killed、stale 0：

| id | 改法 | 預期被誰抓到 |
|---|---|---|
| fw-uti-ctrl-dropped | Ctrl+A 不檢查 ctrl | 對照測試（無 Ctrl 的 A） |
| fw-uti-composition-cursor | Composition 的 cursor 寫死成 -1 | 對照測試 |
| fw-uti-layout-key | 用 `key` 取代 `layoutKey` | 對照測試（AZERTY） |
| fw-uti-clipboard-ignored | Ctrl+V 插入空字串 | 對照測試 |
| fw-uti-row-dropped | 拿掉 Home 那一列 | 對照測試 |
| fw-guard-core-ui-dropped | `CORE["targets"]` 拿掉 `GYO::Ui` | 守衛（真實樹） |
| fw-guard-binding-ui-allowed | 綁定清單加 `GYO::Ui` | 新的自測 |

- 預檢有兩項：每條 find 在檔案裡恰好出現一次；拿改動後的實際輸出核對預期訊息（framework 整改線的提醒）。
- 只在本機跑的 `editor` 測試若要加突變，宣告時再補，不在上表。

## 範圍的限界

- IU-1 不改 `PreviewAdapter.cpp`（IU-2）和 `PvpApplication.cpp`（IU-3）。合併時，核心的轉換還沒有使用者：IU-2、IU-3 接上之後才算完成卡的第 1 件事。
- 組字只驗 macOS（使用者 2026-11-01 決定）。Windows 要開 `SdlPlatformOptions::imeComposition`，不在這張卡。
- 組字中點滑鼠組字會消失（#132 記錄的 macOS 行為），本批不修。
- 選了 ui_editor 的建置樹裡，`tests/framework` 的「綁定存在」檢查會被 ui_editor 自己的檢查遮住（fw-shell-binding-missing 的說明有寫）。跑突變時用只選 audio_editor、或不選 ui_editor 的樹。

## 模型與檔位（navigator.md）

上段。根據：公開介面（新標頭、`ImGuiShellContext` 加成員）、framework DI 2 的守衛變更。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 実装者 | 主會話 Opus 5.5／high | 卡的分配 |
| 審査者 | Opus 5.5／xhigh（`xhigh-reviewer`，唯讀） | 卡的分配。文字輸入的仲裁有同一幀內的先後順序，屬於容易出錯的時序邏輯 |
| 検証者（§14.4） | Opus 5.5／high | 卡的分配。核對第 4 點，不用小模型 |

## 時間

- 估 6～8 h：設計與標頭 1 h、核心與對照測試 1.5 h、綁定仲裁與測試 2 h、守衛與突變 1.5 h、審查與検証 1～2 h。
- 停止條件：超過 12 h，或單一問題累計超過 2 h。
- 證據放主 checkout 的 `build/evidence/v8-input-ui-adapter/IU-1/`。

## 執行紀錄

- 2026-11-02，使用者：「核准，採 (a)」。開工，底是 master `d03c01e2`。
- 實作過程中查證到的事實：綁定的 Update 在 `client_->BeginFrame()` 之前呼叫 `SdlInput::BeginFrame()`，它會清掉 events 和 text。所以 `Frame()` 交出的是每幀在 EndFrame 與 BeginFrame 之間複製的那一份，輸入法覆蓋層讀的也是同一份。
- 突變目錄的實際位置是 `docs/architecture/plans/framework-rework/scripts/` 的 `framework_mutations.json`（核心與守衛）與 `framework_shell_mutations.json`（綁定）。宣告寫的 `scripts/framework_mutations.json` 不對。
- 建置與測試：全量建置成功；framework 的測試 7/7；全量 CTest 123/123（`ctest_full.txt`）。核心連 `GYO::Ui` 的組態 configure 成功，configure 時的連結檢查沒有把 `GYO::Ui` 私下連的 nlohmann 算進核心（`configure.txt`）。守衛自測：改守衛前 39 條中 1 條失敗（`red_selftest.txt`），改後 39/39（`green_selftest.txt`）。
- 審査者（Opus 5.5／xhigh，唯讀）：1 blocker、2 major、7 minor、3 nit。
  - **B1**：宣告的突變 fw-uti-layout-key 會存活。U1 裡 AZERTY 的兩筆相鄰而且對稱，突變把 SelectAll 從第一筆移到第二筆，輸出序列不變。在跑突變之前收緊測試：兩筆之間插一筆 Text，並新增 U5 逐筆檢查。只是收緊，沒有放寬。
  - **M1（缺陷，§14.3）**：兩邊同時要文字輸入時，視窗失焦再回焦，ImGui 的 SDL3 後端會在 NewFrame 裡把輸入區域設回 ImGui 欄位，而且不經過平台 hook。綁定保存的 SDL 狀態副本（`applied_`）以為區域沒變，就不再設定。
    - 這正是 AGENTS §3「状態の権威」要防的情形：綁定持有別的元件權威狀態的副本，並拿它來判斷要不要呼叫。
    - Red：I11 新增子案例，用 `SDL_HideWindow`／`SDL_ShowWindow` 失焦再回焦，修正前失敗（`m1_red.txt`）。
    - 修正：拿掉副本與「ImGui 本幀有沒有回報」的判斷，client 要求期間每幀都 `SetTextInput(true)`（SDL 對已開啟的文字輸入不做事，審査者讀 SDL 3.4.0 原始碼確認）和 `SetTextInputArea`。交還時把 ImGui 最後的回報再送給它的後端，由後端自己放回區域，不再照抄後端的公式（審查 minor 5）。
    - Green：I11 6/6（`m1_green.txt`），framework 7/7（`m1_green_framework.txt`）。
    - 限界：Windows 上每幀呼叫 `SetTextInputArea` 的成本沒有查證（Windows 不在這張卡的驗收範圍）。
  - **M2**：`ImGuiShellInput` 字面上符合 framework Replacement Criteria 第 7 條（一個產品需要外殼裡其他產品不用的 callback 或 flag）。交使用者判斷，見下節。
  - minor 處理：
    - 標頭寫明 `RequestTextInput` 只在 BeginFrame 或 Draw 呼叫（EndFrame 的要求會被丟掉）；
    - 交還的區域也檢查寬與高；
    - 拿掉恆真的指標比較；
    - 綁定 CMake 的過時註解改寫。真實守衛也掃註解，第一次改寫時寫了 `GYO::Input` 被擋下，已重寫，守衛沒有放寬；
    - 析構子改成 protected；
    - 空的剪貼簿函式的行為寫進標頭；
    - 共通測試的註解不再點名產品。
  - **限界**：「client 從未要求時，綁定不呼叫 SetTextInput」只能檢查文字輸入的狀態，觀察不到呼叫本身。審査者確認，拿掉 `if (!owned_) return;` 的變體每幀都會呼叫 `SetTextInput(false)`，但測試照樣通過。
  - 綁定的公開標頭用到 `PhysicalInputFrame`，靠的是核心 PUBLIC 連 `GYO::Input`，綁定自己不具名。要綁定自己宣告，就得改綁定的守衛清單（§14.4），這批不做。

### 突變的事前宣告（更新，執行前）

一共 11 條，全部要 killed、stale 0：

- `framework_mutations.json` 7 條：fw-uti-ctrl-dropped、fw-uti-composition-cursor、fw-uti-layout-key、fw-uti-clipboard-ignored、fw-uti-row-dropped、fw-guard-core-ui-dropped、fw-guard-binding-ui-allowed。
- `framework_shell_mutations.json` 4 條：
  - fw-imgui-request-ignored、fw-imgui-handback-skipped；
  - fw-imgui-request-cached：M1 的缺陷形態，取代原本的 fw-imgui-request-before-imgui，因為它改的判斷已經拿掉；
  - fw-imgui-handback-silent：交還時沒有通知後端。

預檢兩項都已做：每條 find 恰好出現一次；預期訊息和改動後的實際輸出格式相符（doctest 的「is NOT correct」、守衛的「<檔案>: names <target>」、unittest 的「FAIL: <測試名>」）。

建置樹 `build/target/_build/test` 選了 ui_editor 與 audio_editor，這批的突變不受「綁定存在檢查被遮住」的影響。
- 突變結果：核心與守衛 7/7 killed（`mutations_core.txt`），綁定 4/4 killed（`mutations_shell.txt`），stale 0。宣告成立。

### 守衛的判定變更（§14.4 的 1～5）

1. **守住的性質**：framework DI 2。核心只連自己正向清單上的 target：Engine 的中立 target、自己的 target，不連第三方、SDL、`*Native` 或綁定。每個綁定有自己的清單，核心的增補不會帶進綁定。
2. **放寬面**：核心的 CMake 具名 `GYO::Ui`（`tests/framework/framework_dependency_guard.py` 的 `CORE["targets"]`），以前判失敗，之後通過。
3. **收緊面**：無。舊判定失敗的其他輸入，新判定仍然失敗：清單只多了一個名字，比對規則與其他 target 不變；綁定清單不變。
4. **放寬後，性質是否仍由別的判定守住**：`GYO::Ui` 私下連了 nlohmann（`engine/ui/CMakeLists.txt:13-16`）。核心「不連 nlohmann」由兩個判定守：
   - configure 時的連結檢查（`tests/framework/CMakeLists.txt`）：經別的 target 的介面時，不追 `$<LINK_ONLY:>`。所以 `GYO::Ui` 的 PRIVATE nlohmann 不算核心的，而核心自己具名 nlohmann 仍會失敗。
   - 守衛的符號規則：核心的程式碼若出現 `nlohmann`，仍判失敗。
   - 證據：連了 `GYO::Ui` 的真實樹 configure 成功（`configure.txt`），真實守衛通過。
   - 真正的保護（検証者補充）：經別的 target 介面進來的項目，只有 `$<LINK_ONLY:>` 不追，其他照樣追、照樣比對。所以 `GYO::Ui` 若改成 PUBLIC 或 INTERFACE 連 nlohmann，連結檢查會失敗。検証者把真實的檢查函式抽出來實跑：PRIVATE 通過；PUBLIC、INTERFACE、核心自己具名 nlohmann 都失敗（`verifier/linkcheck/`）。
   - 限界：Ui 的公開標頭若 include nlohmann，核心經由它間接拿到，守衛與連結檢查都沒有明文判定，只靠編譯失敗間接擋住（nlohmann 由 FetchContent 取得，不在系統 include 路徑；PRIVATE 不把 include 路徑傳給核心）。目前 `engine/ui/include` 沒有 nlohmann。
5. **根據與證據**：卡的 `v8-input-ui-adapter-home` (a)（adapter 放 framework 核心）；framework DI 2「加非 `*Native` 的 Engine target 依本條，守衛的變更照 §14.4」。證據：
   - `red_selftest.txt`：改清單前，「核心可連 `GYO::Ui`」失敗；
   - `green_selftest.txt`：39/39；
   - 突變 fw-guard-core-ui-dropped、fw-guard-binding-ui-allowed 都 killed。
- 検証者（Opus 5.5／high，唯讀，實驗在暫存區的副本，已複製到 `verifier/`）：
  - 判定：1、2、3、5 確認；4 部分成立（結論對，說明不完整，已補上）。
  - 0 blocker、0 major、3 minor：第 4 項的說明（已補）、限界（已補）、json 綁定的自測（已加 `test_ui_target_in_the_json_binding_fails`，屬收緊，自測 40/40）。
  - 第 3 項的證據：17 個構造輸入對新舊守衛做差分，舊判定失敗、新判定通過的只有「核心具名 `GYO::Ui`」一例（`verifier/diff_fixtures.py`）。
  - 全量 CTest 在審查修正後 123/123（`ctest_full_after_review.txt`）。
- 2026-11-02，使用者對 M2 的判斷：「不算，但補條文」。Replacement Criteria 第 7 條加一句：綁定把只有它能依 DI 2 讀取的中立能力（例如視窗的輸入幀與文字輸入）交給所有 client，不算觸發。條文全文交使用者核准。
- 2026-11-02，使用者：「核准」。守衛的判定變更（核心清單加 `GYO::Ui`）與 Replacement Criteria 第 7 條的補句核准。IU-1 完成，[dev_log](../../../dev_logs/2026_11_02_framework_v8_input_ui_adapter_iu1.zh-Hant.md)。
