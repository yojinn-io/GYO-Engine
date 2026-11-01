# 工具 v8：v8-tool-ui-editor-input-adapter（IU-2）

日期：2026-11-02。分支 `claude/v8-tool-ui-editor-input-adapter`，從 master `a8a48243`（IU-1，PR #135）開出。

- 段：上段。
- 実装者 Opus 5.5／high（主會話）；審査者 Opus 5.5／high，兩次：一次審實作，一次審回歸修正。
- 守衛只收緊，沒有開検証者。
- 估時 4～6 h；從開工到第二次審查的指出處理完，（02:22～02:54，牆上時間），不含子 agent 的等待。

**狀態：**
- 程式、測試、突變、兩次審查都完成。
- 守衛的收緊（§14.4）待使用者核准。
- L3（macOS）還沒做。

相關文件：
- [任務卡](../object_fps_pvp/plans/v8/input-ui-adapter-card.md)；
- [事前宣告與結果](../tools/plans/v8/assets.md)「`v8-tool-ui-editor-input-adapter`（IU-2）」；
- [ui_toolchain.md](../ui_toolchain.md) 的 ui_editor Design Invariants。

## 做了什麼

- **預覽的文字輸入：**
  - 改用 framework 的 `ToUiTextInput` 讀 Engine 的輸入幀，和遊戲共用同一張對照；
  - 刪掉 ImGui 字元佇列與 `ImGuiKey` 表那段；
  - 快捷鍵依 `ConfigMacOSXBehaviors` 選 Gui 或 Ctrl；
  - 組字因此進得了預覽的 text_field。
- **文字輸入的請求：**
  - 刪掉 `imgui_internal.h` 與寫 `PlatformImeData` 那段；
  - 預覽改在 `FrameOutput::textInput` 交出編輯中欄位的範圍，EditorClient 在 `Draw` 轉給 `ImGuiShellInput::RequestTextInput`；
  - 輸入幀由 EditorClient 在 `BeginFrame` 交給 EditorApp。
- **守衛（收緊）：** 任何 ui_editor 檔案都不得 include `imgui_internal.h`。
- **回歸修正：**
  - EditorApp 記住上一幀預覽是否有欄位在編輯；
  - `TextInputActive()` 是 ImGui 的欄位與預覽欄位兩者合起來的判斷，用它暫停編輯器的快捷鍵與方向鍵微移；
  - 為了測試新增 `SetPreviewMode`，作用等同 View 選單的 Preview interaction。

## 驗證與經過

- 本機 macOS（`build/target/_build/framework`，全部工具）：ctest 67／67；守衛通過。
- 件數の突き合わせ：一致。
  - 宣告 11 條：
    - 實作時 6 條；
    - 回歸修正時 3 條；
    - 第二次審查後，針對呼叫處 2 條。
  - 另有 13 條受影響而重跑，每次重跑的範圍見下面列的檔案。
  - 最後一輪 `mutations-final.txt` 13／13 killed，stale 0。
  - 有兩次不是 killed 的結果，都是突變或期待寫錯，不是測試抓不到：
    1. `ui-input-request-missing` 的替換讓括號不成對，第一次執行時編譯失敗，沒有測到東西。改成能編譯的寫法後重跑，期待不變，killed。
    2. 回歸修正時，我把一條斷言拆成兩條，改了名稱；`ui-input-shortcut-always-ctrl` 重跑時，測試仍失敗，但訊息和預期的舊名稱不同（wrong_failure）。把期待改成新名稱後重跑，killed。
  - 有一次是測試真的抓不到，已補強：
    - 情況：第二次審查後新增的呼叫處突變 `ui-editor-shortcuts-ignore-preview`、`ui-editor-nudge-ignores-preview`，第一次兩條都存活（`mutations-test-strengthened.txt`）。
    - 原因（本機暫時加診斷輸出查到，查完已還原）：測試的寫法讓快捷鍵和選取都沒有真的發生。
      - macOS 上 ImGui 的 `ConfigMacOSXBehaviors` 會對調 Ctrl 和 Cmd，測試送的 Ctrl 沒有變成 `io.KeyCtrl`；
      - 滑鼠按下比移動先處理，點擊落在舊位置，沒選到欄位。
    - 修正測試：先移動游標再點擊，這個測試裡關掉對調。之後兩條都 killed（`mutations-final.txt`）。
  - 證據在 使用者的 Mac，主 checkout `build/evidence/tool-v8-ui-editor-input-adapter/`：`mutations.txt`、`mutations-request-missing-fixed.txt`、`mutations-regression-fix.txt`、`mutations-shortcut-rerun.txt`。
- 第一次審查找到回歸（依 §14 停下，使用者選「(a) 在工具內修」）：
  - **現象：** 預覽的欄位編輯中，`io.WantTextInput` 不再是 true。
  - **原因：** 舊程式寫 `PlatformImeData.WantTextInput`，ImGui 才會在下一幀設這個旗標。
  - **後果：**
    - `EditorApp.cpp` 的 Cmd／Ctrl 快捷鍵（貼上元素、復原、重做、複製、儲存）會在打字時一起觸發；
    - 方向鍵微移也一樣；
    - 文件一改，預覽就重建，編輯中的內容消失。
  - **§14.3 的處理：**
    - 寫一個實際畫出 EditorApp 的測試（`TestEditorKeysWaitWhilePreviewTextIsEdited`）；
    - 修正前它失敗（Red，`red.txt`），修正後通過（Green，`green.txt`）；
    - 這個測試留作回歸測試。
- 第一次審查另外指出的，都已補測試：
  - Engine 的方向鍵會移動游標，ImGui 的方向鍵不會；
  - Cmd 模式下 Ctrl＋A 不會全選；
  - 請求的位置精確跟著預覽的原點移動。
- 第二次審查（回歸修正）沒有嚴重問題，指出兩件，都已處理：
  - 回歸測試只檢查判斷函式 `TextInputActive()` 的值，呼叫處改回 `io.WantTextInput` 也照樣通過。
    - 已改成檢查實際效果：選取欄位、編輯中按 Ctrl＋D 和方向鍵，文件不變，編輯也不中斷。
    - 另加兩條針對呼叫處的突變。
  - Canvas 視窗沒畫出來時，「預覽欄位正在編輯」的旗標會留在舊值。已改成在提前返回時清掉。
- Red 的建置條件：
  - Red 那次，`TextInputActive` 已經加了，但只回傳 `io.WantTextInput`，也就是修正前的行為；當時的測試只檢查這個函式的值。
  - 強化後的測試抓得到回歸，由上面兩條呼叫處的突變被殺來證明。
  - `red.txt` 已補上這段說明。

## 範圍的限界

- **Windows 不驗：** Windows 的組字要開 `imeComposition`，使用者決定這張卡只驗 macOS。
- **其他輸入照舊從 ImGui 讀：** 預覽的指標、Tab、方向鍵（調整用）、Enter、Space、Escape。要改這些，要先有共用的 `UiInputFrame` 轉換。
- **候選窗不跟著游標：** `cursorPixels` 一直是 0，和舊程式相同，所以輸入法的候選窗不會隨游標或組字移動。
- **CI 不驗真的輸入法：** CI 只跑 `cpu|shader`，真實輸入法只靠 L3。
