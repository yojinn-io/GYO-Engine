# IU-3：遊戲宣告使用 framework，改用共用的文字轉換（v8-input-ui-adapter）

日期：2026-11-02。分支 `claude/v8-input-ui-adapter-iu3`，底是 master `a8a48243`（IU-1，PR #135 合併後）。
上段：Build Graph 的 Architecture Delta 與 framework DI 1 條文。
模型與檔位：
- 実装者：推動會話的主會話，Opus 5.5／high；
- 審査者：Opus 5.5／high 子 Agent（唯讀）；
- 不改守衛與驗收判定，所以沒有 §14.4 検証者。

時間：估 3～4 h，實際約 2 h。

相關文件：[任務卡](../object_fps_pvp/plans/v8/input-ui-adapter-card.md)、[IU-3 Delta](../object_fps_pvp/plans/v8/input-ui-adapter-iu3-build-delta.md)、[IU-3 事前宣告與執行紀錄](../object_fps_pvp/plans/v8/input-ui-adapter-iu3-declaration.md)。

**狀態：** 完成。Mac 上已做建置、全量 CTest、L2 三種組態與探測；Windows、Linux 由 CI 的 L1 驗證。

## 做了什麼

- **共通建置**：
  - `GYO_KNOWN_COMPONENTS` 加 `FRAMEWORK`；它不需要 SDL。
  - `gyo_app_link_components` 的對應表加 `FRAMEWORK → GYO::ToolFramework`。
  - `GyoBuild.cmake` 加入 `framework/` 與 `tests/framework` 的條件，改成 `GYO_ACTIVE_TOOLS OR GYO_ENABLE_FRAMEWORK`。
- **遊戲**：
  - `project.json` 宣告 `FRAMEWORK`；`app_support` 用 `PRIVATE_COMPONENTS FRAMEWORK` 連它。
  - `PvpApplication::TextInput` 改成 `ToUiTextInput(text, Ctrl, clipboard)`。被刪掉的表和 IU-1 測試 U1 照抄的參考表逐字相同，傳 Ctrl，所以行為不變。
- **文件**：
  - framework DI 1 換成使用者核准的條文：有工具，或選了的遊戲宣告 `FRAMEWORK` 時才建。
  - `framework/CMakeLists.txt`、`tests/framework/CMakeLists.txt` 開頭的註解。
  - Using it 加上遊戲怎麼宣告。
  - 卡的進度、v8 README 的 dev_log 表。
- **組件測試**：新增案例，`FRAMEWORK` 打開時 SDL3 保持關閉。

## 驗證與經緯

件數の突き合わせ：一致（探測：宣告 4・執行 4・預期的失敗 4。第一次無效的試行另外保存，不計入。）

- 建置與測試（本機 Mac）：

| 組態 | 結果 |
|---|---|
| test preset | CTest 123/123 |
| 只選 object_fps_pvp、不選工具 | CTest 108/108；有 framework 核心與 `tests/framework`，沒有 ImGui 綁定 |
| 不選產品也不選工具 | configure 成功，沒有 framework 的 target |
| 只選 audio_editor | CTest 56/56 |

- 組件測試的新案例第一次插在 SDL_GPU 那段連續檢查的中間，弄斷了後面的 GPU 斷言。移到 IMGUI 案例之後即通過，判定不變。
- **探測第一次無效（§14.1）**：
  - 原因：腳本每條跑完用 `git checkout -- <檔>` 還原。這會還原到 HEAD，而當時修改還沒提交，所以 `project.json`、`GyoBuild.cmake`、`GyoComponents.cmake` 的改動被抹掉。第 1 條之後的三條是在錯的原始碼上跑的。
  - 處理：照原文重新套用修改，做暫存提交，之後每條都還原到那個提交，並確認工作樹和它一致。
  - 重跑：遊戲沒宣告卻連了 FRAMEWORK、拿掉 GyoBuild 的新條件、`FRAMEWORK` 不在已知組件、`FRAMEWORK` 被算成需要 SDL，4/4 都出現預期的失敗。
  - 教訓：探測和突變要以提交為還原點，不用工作樹的 HEAD。
- **審査**（high）：0 blocker、0 major、2 minor、2 nit。
  - 審査者逐項確認了：打包、部署與 CI 範圍判斷只看 `SDL_*`／`IMGUI`，不會誤處理 `FRAMEWORK`；Removability 成立；所有入口在檢查條件前都已算好 `GYO_ENABLE_FRAMEWORK`；遊戲行為不變；DI 1～3 與守衛不受影響。
  - minor 與 nit 的處理見事前宣告的執行紀錄。

證據在主 checkout（本機 Mac）的 `build/evidence/v8-input-ui-adapter/IU-3/`：各組態的 configure／build／ctest、`probes_summary.txt` 與各探測的輸出、無效的第一次 `probes_run1_invalid/`。

## 範圍的限界與留下的事

- **GyoBuild 的新條件 L1 抓不到**：L1 的 preset 都選了工具，工具已經讓 framework 加入建置。守住它的只有遊戲的打包 job（不選工具）和這批的手動探測。之後的整理項目：在 ProjectComposition 之類的共通測試加一個「只選遊戲」的案例。
- 遊戲可以不宣告 `FRAMEWORK`，直接在 `PRIVATE_LIBRARIES` 寫 `GYO::ToolFramework`。宣告與連結的對應只在 `*_COMPONENTS` 路徑上檢查，SDL 後端 target 也是同一類既有問題。只記錄。
- `PvpApplication` 呼叫處傳的修飾鍵沒有自動測試，因為 `TextInput` 是私有實作。
- 遊戲的打包 job 會多建 `tests/framework` 並跑它的測試，符合新的 DI 1。
