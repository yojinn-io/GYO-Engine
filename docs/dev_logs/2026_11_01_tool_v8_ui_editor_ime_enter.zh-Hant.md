# 工具 v8：v8-tool-ui-editor-ime-enter

日期：2026-11-01。分支 `claude/v8-tool-ui-editor-ime`（切自 `claude/v8-tool-framework-rework`＝PR #131 的 `847062ec`）。

- 檔位與模型：Opus 5.5（主會話，從 PR #131 的會話分出）。
- 用時：調查約 1 小時（19:20～20:05，包括使用者兩次實機錄 log）；參考做法的調查與修補另約 1 小時。

**狀態：** 原因已找到，有實機證據（第 4 步的解讀依使用者的觀察更正過）。使用者選了修補 SDL（方案 (甲)），已實作；兩個平台的 L3 待使用者確認。

相關文件：[PR #131 的 dev_log](2026_11_01_tool_v8_ui_editor_shell_adopt.zh-Hant.md)「L3」第 6 項。

## 現象

- 平台：macOS（使用者的 Mac）。
- 位置：ui_editor 的 Inspector，button 的文字欄位（`ImGui::InputText`）。
- 日文輸入法：用 Enter 確定變換後，文字偶爾才寫得進欄位。
- 中文輸入法：正常。
- 舊宿主（`ImGuiSdlHost`）和新外殼（`ImGuiShell`）的結果相同。

## 原因（四步，每步附證據）

1. **日文輸入法按 Enter 確定時，SDL 先送 Return 的 KEY_DOWN。**
   - SDL 3.4.0 在 macOS 上（`src/video/cocoa/SDL_cocoakeyboard.m`）的做法：
     - 按鍵先記成 pending，交給輸入法處理（`interpretKeyEvents`）。
     - 輸入法在這個呼叫裡回了組字或確定文字，pending 就清掉。
     - 呼叫回來後還在 pending 的，才送成 KEY_DOWN。
   - 實機 log（`macos-ja.log`）：
     - 日文組字到「日本語訳」（候選已選）後按 Enter，只出現 `KEY_DOWN scancode=40`（Return）和 KEY_UP。
     - 之後**完全沒有** TEXT_INPUT，也沒有組字結束的 TEXT_EDITING ""。
   - 中文：TEXT_EDITING "" 之後緊接 TEXT_INPUT「中文」，中間沒有任何 KEY_DOWN。中文輸入法在呼叫裡就交出了文字。
2. **ImGui 收到 Enter，結束單行 InputText 的編輯；之後才到的文字進不了這個輸入框。**
   - ImGui 1.92.5 的真實幀探測（`imgui-order-probe`），單行 InputText 已在編輯時注入事件：

     | 事件順序 | 結果 |
     |---|---|
     | 文字 → 下一幀才按 Enter | 寫進去 |
     | 同一幀：文字、按 Enter | 寫進去 |
     | 只有文字，沒有按 Enter | 寫進去，仍在編輯 |
     | 先按 Enter → 下一幀才來文字 | 丟掉 |
     | 同一幀：按 Enter、文字、放開 Enter | 丟掉 |

   - 同一幀也會丟，是因為 ImGui 的輸入佇列規則：同一幀裡先有非字元鍵變化，之後的文字就延到下一幀處理。
3. **編輯結束後，ImGui 的 SDL3 後端呼叫 `SDL_StopTextInput`。**
   - ImGui 不再要求輸入法時，`ImGui_ImplSDL3_UpdateIme` 會呼叫它。
   - macOS 上的 `Cocoa_StopTextInput` 會把接收輸入法文字的元件（`fieldEdit`）從視窗拿掉。
4. **之後才確定的文字，被 SDL 靜默丟掉。**
   - `SDL_SendKeyboardText` 在文字輸入已關閉時直接 return，不推事件，所以 `SDL_EVENT_LOGGING` 也看不到。
   - 驗證用的診斷版（`diag-hold-enter.diff`，只在設了環境變數時生效）：組字中不把 Return 的 KEY_DOWN 交給 ImGui。實機 log 是 `macos-ja-hold-enter.log`。
     - 三次組字的第一次 Enter 都被攔下（「今回」、「日本語字幕」、「です」）。
     - 之後都送來了 TEXT_INPUT，那時文字輸入還開著。
   - 日文輸入法本來的操作（使用者說明）：打字，空白鍵選字，第一次 Enter 把選好的字放進輸入框，第二次 Enter 才是給程式的確定。
     - 所以第一次 Enter 應該讓輸入法送出確定文字（`insertText`）。這次沒有在 `interpretKeyEvents` 之內送出，SDL 便把它當一般的 Enter 送出。
     - log 的時序對得上：組字文字（「です」）在 Enter 之後沒有變化，沒有新的 TEXT_EDITING，SDL 的 pending 一直沒被清掉。
   - 診斷版裡，TEXT_INPUT 在攔下的 Enter 之後 0.24 秒（「です」）與 1.19 秒（「日本語字幕」）才出現，中間沒有 KEY_DOWN。log 分不出下面兩種情況：
     - 第一次 Enter 的確定文字晚一點才送到；
     - 使用者又按了一次 Enter，輸入法在 `interpretKeyEvents` 之內就送出文字，SDL 便清掉這次按鍵。
   - 不論哪一種，結論與修正都一樣：組字中的 Enter 是輸入法的鍵，不該交給 ImGui。
   - 「今回」：攔下 Enter 後，使用者接著打「は」，最後與「今回は」一起確定。
   - 所以原版的經過是：第一次 Enter 的 KEY_DOWN 漏給 ImGui，ImGui 結束編輯並關掉文字輸入，之後才到的確定文字就被丟掉。
   - 「偶爾才寫得進」的情況沒有錄到 log，推測是輸入法在 `interpretKeyEvents` 之內就送出了文字（SDL 不送 KEY_DOWN）。
   - 兩次錄 log 都印出 `error messaging the mach port for IMKCFRunLoopWakeUpReliable`。和這個問題的關係不明。

## 另一個發現：組字中介面上看不到字

- 使用者觀察：用日文輸入法打字時，輸入框裡不顯示組字中的假名；變換成漢字、候選窗出現後才看得到選項。
- 機制：macOS 上組字文字（marked text）要由程式自己畫。SDL 用 TEXT_EDITING 送給程式，但 ImGui 的 SDL3 後端不處理 TEXT_EDITING，ImGui 本身也不畫組字。
- 中文輸入法的拼音顯示在輸入法自己的候選列裡，所以看得到。
- 這是 ImGui 既有的限制，和 Enter 的問題無關。要顯示組字，得在 framework 的 ImGui 綁定或 ui_editor 自己畫，另外處理。

## Windows（沒有實機，只讀了原始碼）

- SDL 3.4.0 的 `WIN_HandleIMEMessage`（`src/video/windows/SDL_windowskeyboard.c`）會攔下 `VK_PROCESSKEY`，也就是輸入法正在處理的鍵，不送 KEY_DOWN。
- 所以結構上不會發生「組字中送出 Enter」；要使用者實機確認。
- 另外看到一點，沒查證：`SdlPlatformOptions::imeComposition` 只有 PvP 打開，ui_editor 沒開。所以 Windows 上的組字由輸入法自己的視窗顯示。這和 ImGui 不畫組字文字的情況一致。

## 參考的做法（2026-11-01 讀各專案 master／main 的原始碼）

| 引擎／函式庫 | 誰判斷「這個鍵歸輸入法」 | 規則 | 日文的第一次 Enter |
|---|---|---|---|
| Godot（macOS 與 Windows 同一套） | 平台層 DisplayServer | 從開始組字到組字結束，按鍵的按下與放開都不交出；LineEdit 自己畫組字 | 不漏 |
| winit（Bevy 用的視窗庫） | 平台層（macOS 的 view） | 還有組字文字時不交出，除非輸入法呼叫 `doCommandBySelector:` | 不漏 |
| SDL 3.4.0 與上游 main | SDL 的 macOS 後端 | 輸入法處理這個鍵時沒改組字文字，就當一般按鍵送出 | 漏 |
| SDL 的 Windows 後端 | SDL | 攔下作業系統標記的 `VK_PROCESSKEY` | 不漏（結構上） |
| Dear ImGui 的 macOS 後端、GLFW | 沒有判斷 | 按鍵一律先交出 | 漏 |

SDL 上沒找到 macOS 的同類回報；最接近的是 SDL2 時代的 Windows issue（libsdl-org/SDL#1737），以攔下 `VK_PROCESSKEY` 解決。

## 修正（使用者選方案 (甲)：修補 SDL）

三個方案與取捨給了使用者：(甲) 修補 SDL 的 macOS 後端；(乙) Engine 平台層 `SdlPlatform::PumpEvents` 由組字事件推狀態再過濾，違反 [platform-sdl.md](../architecture/platform-sdl.md) 的 Design Invariant 6（不留文字輸入狀態的副本做判斷）；(丙) 只在 ImGui 綁定過濾，同樣是複製狀態，而且 GYO::Ui 不受保護。使用者選 (甲)。

- `third_party/sdl3/patches/gyo-cocoa-ime-composing-keydown.patch`（對 `src/video/cocoa/SDL_cocoakeyboard.m`）：照 winit 的規則，按鍵前已在組字，且輸入法沒呼叫 `doCommandBySelector:`，就不送這個鍵的按下。放開事件不必處理：SDL 會丟掉沒有按下的放開。
- `third_party/sdl3/ApplyPatches.cmake`：FetchContent 的 patch 步驟，依檔名順序套用，已套用的就跳過（patch 步驟可能在已修補的原始碼上再跑）。
- `third_party/sdl3/CMakeLists.txt`：設定時檢查原始碼裡有修補的記號；用 `FETCHCONTENT_SOURCE_DIR_SDL3` 指定沒修補的原始碼時，設定失敗並說明原因。
- [platform-sdl.md](../architecture/platform-sdl.md)「SDL patches」：列出修補、缺陷、移除條件（上游修好，或升級 SDL 後套不上時，先在新版重現再移植）。
- shader 工具的宿主建置（`engine/render/shaders/pipeline/cmake/Dependencies.cmake`）另外抓 SDL，不經這個修補；它不用鍵盤，不受影響。

### Architecture Delta

1. Feature：ui_editor 的輸入框能用日文輸入法輸入（macOS）；同一條路徑上的其他 SDL 程式也受益。
2. 問題：SDL 的 macOS 後端判斷「鍵歸輸入法」的規則漏了「用了鍵卻不改組字文字」的情況。
3. 變化的邊界：third_party 第一次有修補檔，建置圖多了 patch 步驟。
4. 影響：所有在 macOS 使用 SDL 文字輸入的程式（ImGui 工具、GYO::Ui）。Windows、Linux 的程式碼沒有變。
5. 依存方向：不變。
6. Ownership：修補屬 platform-sdl 的 owner（`GYO::PlatformSDL`）。
7. 更小的改法：(乙)、(丙) 要在 SDL 外面複製組字狀態，違反 Design Invariant 6 或 AGENTS.md §3「狀態的權威」；判斷放在持有組字文字的 SDL，才不必複製。

### 驗證

- 修補對 SDL `release-3.4.0`（`a962f40bb`）乾淨套上；全新設定時套用，再跑一次套用腳本則跳過（原始碼的 diff 不變）。
- 用 `FETCHCONTENT_SOURCE_DIR_SDL3` 指向沒修補的原始碼：設定失敗，訊息指向修補檔與本文件。
- Windows 的 runner 預設 `core.autocrlf=true`：本機用 `-c core.autocrlf=true` 分別 clone SDL 與本 repo（兩邊都變成 CRLF），修補照樣套上，重跑也會跳過。
- 本機 macOS（Apple Silicon）的完整 test preset，SDL 由全新抓取並套上修補：建置成功；ctest `-L 'cpu|shader'` 112／113 通過。
  - 失敗的 `build.ci`（4 項）原因在環境：建置目錄放在會話暫存區，不在 git repo 裡，測試裡的 `git rev-parse HEAD` 失敗。在 repo 裡直接跑同一組測試（`python3 -m unittest discover -s tests/common/ci`）全部通過。
- 自動回歸測試：沒有。缺陷要真的輸入法才會出現，SDL 的事件注入碰不到這條路徑。回歸的確認靠 L3：開 `SDL_EVENT_LOGGING=1`，日文組字中按 Enter，修補前會出現 Return 的 KEY_DOWN，修補後不會，文字也寫得進去。

## 證據

使用者的 Mac，主 checkout 的 `build/evidence/tool-v8-ui-editor-ime/`：

- `macos-ja.log`：舊宿主（`build/target/toolchain/bin/gyo_ui_editor`，10-23 建）。先中文、後日文，`SDL_EVENT_LOGGING=1`。
- `macos-ja-hold-enter.log`：新外殼的診斷版，加 `GYO_DIAG_IME_HOLD_ENTER=1`。
- `diag-hold-enter.diff`：診斷改動。只在工作區用過，已還原，沒有 commit。
- `imgui-order-probe/`：ImGui 探測程式與輸出。

## 範圍的限界

- Windows 的實機沒做（修補不改 Windows 的程式碼，L3 只確認沒有回歸）。
- 診斷版只錄了一次（3 次組字）。
- 修補後，組字中除了輸入法以 `doCommandBySelector:` 交回的鍵，其他鍵都不送按下。Enter 以外的鍵（Esc、方向鍵、Tab）在實機上的行為沒有逐一測。
- 組字中看不到假名（ImGui 不畫組字）不在這次的範圍。
- 沒有向 SDL 上游回報（對外發佈，要使用者同意）。
