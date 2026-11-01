# 工具 v8：v8-tool-ui-editor-ime-enter

日期：2026-11-01。分支 `claude/v8-tool-ui-editor-ime`（切自 `claude/v8-tool-framework-rework`＝PR #131 的 `847062ec`）。

- 檔位與模型：Opus 5.5（主會話，從 PR #131 的會話分出）。
- 用時：調查約 1 小時（19:20～20:05，包括使用者兩次實機錄 log）；參考做法的調查與 SDL 修補約 1 小時；組字顯示、Design Invariant 的修改與守衛約 1.5 小時。
- 審查者（守衛變更）：Sonnet 5.5／檔位繼承，和實作者不同的模型。

**狀態：** 兩個修正都完成，macOS 的 L3 通過：(1) SDL 修補讓組字中的 Enter 不再漏給程式，文字不再丟失；(2) ImGui 綁定畫出組字，第一次 Enter 選定後看得到帶底線的文字。framework 與 ui_editor 的 Design Invariant 和守衛已改（使用者核准）。Windows 的 L3 未做。

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

## 另一個發現：組字中介面上看不到字（已處理，見「組字顯示」）

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

## L3（macOS，2026-11-01 20:51，使用者操作，修補版）

- log（`macos-ja-patched.log`）：組字中沒有 Return 的 KEY_DOWN。確定文字送了 4 次（「人」「日本語訳」「にほんご」「日本」），每次都是先送組字結束，再送 TEXT_INPUT，那時文字輸入還開著。組字結束後的 Return 照常交給 ImGui。
- 使用者：仍然要按兩次 Enter，文字才寫進去。
  - 修補擋下的鍵不進 SDL 的佇列，log 看不到；所以第一次 Enter 有沒有被輸入法用掉，log 證明不了。
  - 有一次（「日本語訳」，20:51:05）組字中點了兩下滑鼠，組字沒確定就消失了。點擊讓 ImGui 結束編輯，SDL 收起輸入元件，組字就被丟掉；原生程式通常會先確定。這是另一個問題，修補不處理。
- 判定：文字不丟了。「要按兩次 Enter」是日文輸入法本來的操作，原生程式也一樣（使用者說明）：
  - 第一次 Enter 選定文字，介面上已經看得到，但還帶底線（仍在組字）；
  - 第二次 Enter 才確定。
- 剩下的問題是 ImGui 不畫組字文字：第一次 Enter 後畫面沒有任何變化，使用者分不出輸入法在等確定。遊戲的 text_field（GYO::Ui）會畫組字，所以看得到。組字的顯示另外提方案給使用者。

## 組字顯示（`v8-tool-ui-editor-ime-composition-source`）

修補後文字不再丟失，但使用者說明：日文輸入法本來就要兩次 Enter（第一次選定，介面上已經看得到帶底線的字；第一次 Enter 後還能用上下鍵重新選字；第二次才確定）。ImGui 不畫組字，所以第一次 Enter 後畫面沒有變化。遊戲的 text_field（GYO::Ui）會畫組字。

- 落點：framework 的 ImGui 綁定（使用者選定）。
- 組字從哪裡讀：Engine 的 input backend（`SdlInput`）已經把 TEXT_EDITING 轉成中立的 `TextEvent::Kind::Composition`，這是唯一一處解讀 SDL 文字事件的地方。framework 的 Design Invariant 2 與 ui_editor 的 Design Invariant 1 原本不准連結 `GYO::Input`；使用者的原則：「引擎的職責就是支撐 tool 和 Game 的，framework 可以調用 Engine 但是順序不能犯規來。ui_editor 可以使用 Engine 的能力，但是不能直接碰 SDL，邏輯和 game 一樣」。`GYO::Input` 只連結 `GYO::Base`，`GYO::InputBackendSDL` 以 PRIVATE 連結 SDL、公開標頭不含 SDL 型別，所以用它不會讓呼叫端直接碰 SDL。我先前提的「綁定多准兩個 `SDL_` 名字」與「Engine 平台層加組字的解讀」都會有第二處解讀，已撤回。
- 實作（`framework/bindings/imgui/src/ImGuiShell.cpp`）：
  - 綁定把每個事件也交給一個 `SdlInput`，每幀只讀它的文字：最新的組字為準；確定的文字、組字結束、失焦、ImGui 不再要求輸入法時清掉。
  - 位置用 ImGui 交給平台的輸入法資料（`Platform_SetImeDataFn`，公開介面；綁定包住 SDL3 後端的處理函式再轉交，後端關閉時會清掉它）。
  - 在最上層繪製清單，於游標處畫：不透明底色、文字、底線；選定的區段畫粗底線，沒有選定時畫游標。只用來顯示，不參與任何判斷（AGENTS.md §3）。
  - 限制：ImGui 不能把文字暫時插進輸入框，組字疊在游標後面，會蓋住後面的字。
  - Windows：ui_editor 沒開 `imeComposition`，SDL 不送組字事件，組字照舊由輸入法自己的視窗顯示。
- 建置：綁定在建置有 `GYO::InputBackendSDL` 時才建；ui_editor 的 `project.json`（gui）加 `SDL_INPUT` 元件；`tests/framework` 的設定時檢查改成「有 SDL renderer、SDL input、ImGui 卻沒有綁定」才失敗。
- Design Invariants：framework DI 2 拿掉「不連結 `GYO::Input`」，寫明 Engine 的能力（含 input）對 framework 開放，ImGui 綁定經 input backend 讀文字與組字；ui_editor DI 1 拿掉「不連結 `GYO::Input`」。使用者核准。ui_editor 的部分後來由 PR #131 的 T4 取代（見「和 PR #131 的 T4 合併」）。

### 守衛的變更（AGENTS.md §14.4，使用者核准）

framework 的守衛（`tests/framework/framework_dependency_guard.py`）：

1. 守的性質：framework 不連結遊戲、不用 SDL_GPU，只經 Engine 的中立目標碰 SDL（ImGui 綁定連結 Native 層但不寫 `SDL_` 名字）；舊規則另外完全禁止 Engine 的 input。
2. 放寬：核心與綁定可以 include `engine/input/` 的中立標頭；ImGui 綁定的 CMake 可以寫 `GYO::InputBackendSDL`；核心的 CMake 寫 `GYO::InputBackendSDL` 仍失敗，但從硬錯變成可列白名單的軟錯（和其他新目標相同；白名單數量要相等、只能變少、`v8-framework-invariants` 後清空）。
3. 收緊：沒有。
4. 仍守住的：遊戲標頭與登錄名、SDL_GPU、綁定裡的 `SDL_` 名字、input 的 Native 標頭（核心裡是任何白名單都抵銷不了的硬錯，各綁定裡也失敗）、ImGui 綁定寫 `GYO::InputBackendSDLNative`、JSON 綁定寫 `GYO::InputBackendSDL`、核心寫 `GYO::Input`。
5. 根據：使用者的原則（上方）。

ui_editor 的守衛（`tests/ui_editor/ui_editor_dependency_guard.py`）：

1. 守的性質：editor 不連結遊戲、不用 SDL_GPU，依存都在正面清單。舊規則另外禁止 include `engine/input/`。
2. 放寬：`tools/ui_editor` 的原始碼可以 include `engine/input/`。
3. 收緊：沒有。
4. 仍守住的：遊戲標頭、SDL_GPU、目標的正面清單（editor 的 CMake 寫 `GYO::Input` 或 `GYO::InputBackendSDL` 仍失敗）。
5. 根據：同上。

檢驗：

- 守衛自測全部通過；對 repo 的掃描乾淨。
- 守衛突變 9／9 killed：綁定不准 input backend、核心或 JSON 綁定准它、核心准 input 的 Native 標頭或讓它可列白名單、不查 SDL_GPU、綁定准 `SDL_` 名字、editor 准 `GYO::Input`、editor 恢復舊的禁令。
- 審查者用新舊兩版守衛跑同一批合成的樹，並做 1500 份隨機樹的差異比對：沒有「以前通過、現在失敗」的情況；所列的每一條都成立。審查者另外找到一處我漏列的放寬：核心 include input 的 Native 標頭，從任何白名單都抵銷不了的硬錯變成可列白名單的軟錯。這不是本意，已改回硬錯，並補了測試與突變（m9）。
- 已知且接受：ui_editor 的連結封閉經由 ImGui 綁定（PRIVATE、靜態）帶進 `GYO::Input`；editor 的守衛只看原始碼文字，不查連結封閉，DI 1 的文字已允許。

### 和 PR #131 的 T4 合併（2026-11-01）

- PR #132 的 base（`claude/v8-tool-framework-rework`）之後加入了 T4：ui_editor 與 audio_editor 的 Design Invariant 1 改寫成「可以用 Engine 的任何模組，SDL 只走和遊戲相同的路徑」，ui_editor 的守衛也一起改寫，允許 `GYO::Input`。
- 合併時，`docs/ui_toolchain.md` 和 ui_editor 的守衛、自測、`tests/ui_editor/CMakeLists.txt` 有衝突，一律採用 T4 的版本。上面「ui_editor 的守衛」那段變更，以及突變 m7、m8（針對那個版本），因此不再適用。
- framework 的 DI 2 與守衛 T4 沒有改，保留本批的修改。
- 合併後：三支守衛（framework、ui_editor、audio_editor）的自測與 repo 掃描都通過；framework 與 ui_editor 的 ctest 19／19 通過。

### 回歸測試與突變

- `tests/framework/ImGuiShellTests.cpp` 的 I10：ImGui 的真實幀（SDL 的 dummy 視訊驅動與軟體渲染器），輸入框編輯中時注入組字事件，檢查最上層清單畫了組字、畫在輸入框的範圍內；再分別注入確定文字、空的組字、失焦、輸入框不再繪製（ImGui 結束編輯），檢查組字消失。
- Red：拿掉繪製時，I10 在 `run.vertices[6] > 0` 失敗。Green：恢復後 I10 的 4 個子情境 24 個斷言全過，其他 ImGui 外殼測試照常通過。
- 突變 6／6 killed：確定不清、失焦不清、ImGui 停止編輯不清、畫錯位置、沒接位置回呼、沒畫。
- I10 的標籤是 `editor`，L1 的 CI 只跑 `cpu|shader`，所以只在本機跑。

### L3（macOS，2026-11-01，使用者操作）

- 組字顯示：使用者確認沒問題。log（`macos-ja-composition.log`）：「日本語字幕」選定後確定，確定前沒有 Return 的 KEY_DOWN，確定後的 Return 照常交給 ImGui。

## 證據

使用者的 Mac，主 checkout 的 `build/evidence/tool-v8-ui-editor-ime/`：

- `macos-ja.log`：舊宿主（`build/target/toolchain/bin/gyo_ui_editor`，10-23 建）。先中文、後日文，`SDL_EVENT_LOGGING=1`。
- `macos-ja-hold-enter.log`：新外殼的診斷版，加 `GYO_DIAG_IME_HOLD_ENTER=1`。
- `diag-hold-enter.diff`：診斷改動。只在工作區用過，已還原，沒有 commit。
- `imgui-order-probe/`：ImGui 探測程式與輸出。
- `macos-ja-patched.log`、`macos-ja-composition.log`：修補版與組字顯示版的 L3。
- `guard-change/`：守衛變更的說明（給審查者的版本）、diff、突變腳本與輸出、審查者的對照腳本。
- `composition/`：組字顯示的突變腳本、Red／Green 的 ctest log。

## 範圍的限界

- Windows 的實機沒做（修補不改 Windows 的程式碼，L3 只確認沒有回歸）。
- 診斷版只錄了一次（3 次組字）。
- 修補後，組字中除了輸入法以 `doCommandBySelector:` 交回的鍵，其他鍵都不送按下。Enter 以外的鍵（Esc、方向鍵、Tab）在實機上的行為沒有逐一測。
- 沒有向 SDL 上游回報（對外發佈，要使用者同意）。
