# IU-1：framework 核心的 Input→Ui adapter 與 ImGui 綁定交出的輸入（v8-input-ui-adapter）

日期：2026-11-02。分支 `claude/v8-input-ui-adapter`，底是 master `d03c01e2`（PR B #134 合併後）。
上段：公開介面（新標頭、`ImGuiShellContext` 加成員），以及 framework DI 2 的守衛變更。
模型與檔位：
- 実装者：推動會話的主會話，Opus 5.5／high；
- 審査者：Opus 5.5／xhigh 子 Agent（唯讀）；
- §14.4 検証者：Opus 5.5／high 子 Agent（唯讀）。

時間：估 6～8 h，實際約 4 h。

相關文件：[任務卡](../object_fps_pvp/plans/v8/input-ui-adapter-card.md)、[IU-1 事前宣告與執行紀錄](../object_fps_pvp/plans/v8/input-ui-adapter-iu1-declaration.md)、[framework 架構](../framework/architecture.md)。

**狀態：** 完成。Mac 上已做建置、全量 CTest 與宣告的突變；Windows、Linux 由 CI 的 L1 驗證。

## 做了什麼

- **核心**：新增 `ToUiTextInput(events, clipboard)`（`framework/include/gyo/framework/UiTextInput.hpp`）。
  - 對照表逐項照搬遊戲那一份（`PvpApplication.cpp:268-289`），只依賴 `GYO::Input` 與 `GYO::Ui`。剪貼簿由呼叫者傳入。
  - 核心改成 PUBLIC 連 `GYO::Input`、`GYO::Ui`。
  - 依使用者的決定（`v8-input-ui-adapter-iu1-frame-scope` (a)）只共用文字轉換。`UiInputFrame` 的其他欄位依 Ui DI 7 是各產品自己的對照。
- **ImGui 綁定**：`ImGuiShellContext` 加 `ImGuiShellInput& input`。
  - `Frame()` 交出每幀在 `SdlInput` 的 EndFrame 與 BeginFrame 之間複製的一份。`BeginFrame` 會清掉 events 和 text，所以不能直接交出 Snapshot。輸入法覆蓋層讀的也是這一份。
  - `RequestTextInput(area, cursorPixels)` 使用視窗座標，只在 BeginFrame 或 Draw 呼叫。綁定在 `ImGui::Render()` 之後套用：client 要求期間每幀都開啟文字輸入並設定區域。client 停止要求時，把 ImGui 最後的回報再送給它的 SDL3 後端，由後端放回 ImGui 欄位的區域；沒有欄位要用時就關掉。
- **守衛**：`CORE["targets"]` 加 `GYO::Ui`。自測加 3 條：核心可以連 `GYO::Ui`；ImGui 綁定與 json 綁定都不可以。
- **文件**：
  - `docs/framework/architecture.md` 的組成清單與 Using it。
  - Replacement Criteria 第 7 條補一句：綁定把只有它能依 DI 2 讀取的中立能力交給所有 client，不算觸發。依審查的 M2，由使用者決定「不算，但補條文」，條文已核准。
- **突變**：`docs/architecture/plans/framework-rework/scripts/` 加 11 條：
  - `framework_mutations.json`：fw-uti-* 5 條、fw-guard-core-ui-dropped、fw-guard-binding-ui-allowed；
  - `framework_shell_mutations.json`：fw-imgui-request-ignored、fw-imgui-handback-skipped、fw-imgui-request-cached、fw-imgui-handback-silent。

## 驗證與經緯

件數の突き合わせ：一致（宣告的突變：宣告 11・執行 11・killed 11・stale 0）。

- **建置與測試**（macOS arm64，test preset，選了 object_fps_pvp、ui_editor、audio_editor）：
  - 全量建置成功；
  - 全量 CTest：首次 123/123，審查修正後 123/123；
  - framework 的測試 7/7；
  - 新測試：U1～U5（`cpu`，L1 會跑）、I11 的 6 個子案例（`editor`，只在本機跑）。
- **nlohmann**：核心連 `GYO::Ui` 的真實樹 configure 成功，連結檢查不把 `GYO::Ui` 私下連的 nlohmann 算進核心。
- **守衛自測**：改清單前 39 條中 1 條失敗，改後 39/39；加上 json 綁定的那條後全過。
- **審査**（xhigh）：1 blocker、2 major、7 minor、3 nit。
  - **B1**：宣告的突變 fw-uti-layout-key 會存活。U1 裡 AZERTY 的兩筆相鄰而且對稱，突變只是把輸出的位置挪了一筆。在跑突變之前收緊測試：兩筆之間插一筆 Text，並新增 U5 逐筆檢查。
  - **M1（缺陷，§14.3）**：兩邊同時要文字輸入時，視窗失焦再回焦，ImGui 的 SDL3 後端會在 NewFrame 裡把輸入區域設回 ImGui 欄位，而且不經過平台 hook。綁定保存了 SDL 狀態的副本，以為區域沒變，就不再設定。這正是 AGENTS §3「状態の権威」要防的情形。
    - Red：I11 新增子案例，用 `SDL_HideWindow`／`SDL_ShowWindow` 失焦再回焦，修正前失敗。
    - 修正：拿掉副本，要求期間每幀都套用。SDL 對已開啟的文字輸入不做事，審査者讀 SDL 3.4.0 原始碼確認過。
    - Green：I11 6/6。
  - **M2**：Replacement Criteria 第 7 條，交使用者決定（見上）。
  - minor／nit 大多已處理，沒處理的寫在下面的限界。
  - 綁定 CMake 的註解第一次改寫時寫了 `GYO::Input`，真實守衛連註解也掃，所以擋下了。改寫了註解，守衛沒有放寬。
- **検証**（§14.4）：
  - 1、2、3、5 確認；4 部分成立，結論對、說明不完整，已補上真正的保護與限界。
  - 17 個構造輸入對新舊守衛做差分：舊判定失敗、新判定通過的只有「核心具名 `GYO::Ui`」一例。
  - 把連結檢查的函式抽出來實跑：`GYO::Ui` 私下連 nlohmann 時通過；PUBLIC、INTERFACE 連，或核心自己具名 nlohmann，都失敗。
- 使用者核准守衛的判定變更與第 7 條的補句（「核准」）。

證據在主 checkout（本機 Mac）的 `build/evidence/v8-input-ui-adapter/IU-1/`：configure、build、ctest、自測的 red／green、M1 的 red／green、兩份突變結果、検証者的實驗 `verifier/`。

## 範圍的限界與留下的事

- 核心的轉換在這批還沒有使用者。IU-2（工具線：預覽改用它、刪 `imgui_internal.h` 與 ImGuiKey 表）與 IU-3（推動會話：產品宣告 `FRAMEWORK`、`PvpApplication` 改用它）接上之後，卡的第 1、2 件事才算完成。
- I11 是 `editor` 標籤，L1 不跑。
- 「client 從未要求時，綁定不呼叫 SetTextInput」只能檢查文字輸入的狀態，觀察不到呼叫本身。
- Windows 上每幀呼叫 `SetTextInputArea` 的成本沒有查證。組字的驗收只做 macOS（使用者決定）。
- Ui 的公開標頭若 include nlohmann，核心經由它間接拿到，守衛與連結檢查都沒有明文判定，只靠編譯失敗間接擋住。目前 `engine/ui/include` 沒有 nlohmann。
- 綁定的公開標頭用到 `PhysicalInputFrame`，靠的是核心 PUBLIC 連 `GYO::Input`，綁定自己不具名。要讓綁定自己宣告，就得改綁定的守衛清單（§14.4）。
