# 工具 v8：v8-tool-ui-editor-ime-enter（調查）

日期：2026-11-01。分支 `claude/v8-tool-ui-editor-ime`（切自 `claude/v8-tool-framework-rework`＝PR #131 的 `847062ec`）。

- 檔位與模型：Opus 5.5（主會話，從 PR #131 的會話分出）。
- 用時：約 1 小時（19:20～20:05），包括使用者兩次實機錄 log。

**狀態：** 原因已找到，有實機證據（第 4 步的解讀依使用者的觀察更正過）。修正會落在 framework 的 ImGui 綁定，或 Engine 的平台層，屬於架構整改線。照任務的約定先停下，**沒有修正**，落點等使用者決定。Windows 的實機還沒做。

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
   - 使用者用診斷版的觀察：要按兩次 Enter 才寫進去。第一次選定漢字，第二次才把文字交給介面。
     - log 對得上：「日本語字幕」與「です」的 TEXT_INPUT 出現在第一次 Enter 之後 1.19 秒與 0.24 秒，中間沒有 KEY_DOWN。第二次 Enter 在當下就交出文字，SDL 清掉了這次按鍵，所以 log 裡看不到。
     - 「今回」：攔下第一次 Enter 後，使用者接著打「は」，最後與「今回は」一起確定。
     - 我最初把那 1.19 秒、0.24 秒讀成「輸入法延後交出文字」，這是錯的，已依使用者的觀察更正。
   - 所以原版的經過是：第一次 Enter 由輸入法處理（選定漢字），但沒有交出文字，SDL 便把它當一般按鍵送出；ImGui 因此結束編輯，關掉文字輸入，組字就成了孤兒。
   - 「偶爾才寫得進」的情況沒有錄到 log，推測是輸入法在第一次 Enter 就交出了文字（SDL 不送 KEY_DOWN）。
   - 為什麼要按兩次 Enter，原因不明，可能和 SDL 的 `NSTextInputClient` 實作不完整有關（例如 `attributedSubstringForProposedRange` 回 nil）。在 TextEdit 這類原生程式裡按一次就能確定的話，問題就在 SDL 這邊；這個對照還沒做。
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

## 修正方向（未套用）

- 組字中（最後一個 TEXT_EDITING 不是空字串），不把 Return 與 KP_Enter 的 KEY_DOWN 交給 ImGui。
- 收到 TEXT_INPUT、組字結束的 TEXT_EDITING ""，或視窗失焦時，回到非組字狀態。
- KEY_UP 照常交給 ImGui，按鍵狀態不會卡住。
- 這樣只能保證文字不丟。要按兩次 Enter 的操作感不會變，原因在輸入法或 SDL。
- 回歸測試可以照 `imgui-order-probe` 的寫法：用 ImGui 的真實幀依序注入「組字、Enter 按下、文字」，期望文字寫進去，編輯也沒有被結束。

落點有兩個，都屬架構整改線：

- framework 的 ImGui 綁定（`ImGuiShell::OnEvent`）：只影響 ImGui 工具。
- Engine 平台層（`SdlPlatform::PumpEvents`）：所有使用 SDL 事件的程式都會受影響。
  - macOS 上 GYO::Ui 的 `text_field` 有沒有同樣的問題，沒有查證（Windows 已在 PR #111 通過）。

## 證據

使用者的 Mac，主 checkout 的 `build/evidence/tool-v8-ui-editor-ime/`：

- `macos-ja.log`：舊宿主（`build/target/toolchain/bin/gyo_ui_editor`，10-23 建）。先中文、後日文，`SDL_EVENT_LOGGING=1`。
- `macos-ja-hold-enter.log`：新外殼的診斷版，加 `GYO_DIAG_IME_HOLD_ENTER=1`。
- `diag-hold-enter.diff`：診斷改動。只在工作區用過，已還原，沒有 commit。
- `imgui-order-probe/`：ImGui 探測程式與輸出。

## 範圍的限界

- Windows 的實機沒做。
- 診斷版只錄了一次（3 次組字）；要兩次 Enter 是使用者的觀察。
- 沒有和原生程式（TextEdit）比對，要按幾次 Enter 才確定。
- Esc（取消組字）與其他鍵有沒有同樣的延遲，沒有測。
