# IU-1 的事前宣告（framework 核心的 Input→Ui adapter）

更新：2026-11-02。**狀態：等使用者核准**。執行：v8-input-ui-adapter 推動會話（使用者 2026-11-01 決定）。底：master `d03c01e2`（PR B #134 合併後）。卡：`input-ui-adapter-card.md`。

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
