# 工具 v8：v8-tool-model-viewer-core

日期：2026-11-04。分支 `claude/v8-tool-model-viewer-77f310`，從 master `4918b1fa` 開出。

- 段：上段（新 Component 的 Design Invariants、寫出 Data Contract 的檔）。
- 實作者 Opus 5.5／high（主會話）；審查者 Opus 5.5／high（子 agent，一次）。沒有放寬，沒有開檢驗者。
- 估時 4～6 h；從開工到審查的指出處理完，約 2 h（牆上時間，含建置與兩輪突變）。

**狀態：**
- 程式、測試、突變、審查都完成。
- L1（CI）通過（見下方「驗證與經過」）。
- 這批沒有 L2、L3；P1、P2 在 `-trial`。

相關文件：
- [事前宣告與結果](../tools/plans/v8/model-viewer.md)：「`-core`」與「`-core` 實作後的結果」；
- [docs/model_viewer/architecture.md](../model_viewer/architecture.md) 的 Design Invariants；
- [animation-events.md](../architecture/animation-events.md)（Engine 的契約，沒有改）。

## 做了什麼

- **登錄新工具 `model_viewer`：**
  - `engine/config/tools.csv` 加一列：enabled、default 為 true，release 為 false；
  - `project.json` 的變體 `cli`，元件 `UFBX`；
  - 文件 `docs/model_viewer/architecture.md`，Design Invariants 1～6 與 Replacement Criteria 1～4，照核准的草稿寫入。
- **核心程式庫 `gyo_model_viewer_core`：**
  - `ModelContent`：用 framework 的唯讀 catalog 掛載內容；模型的位元組經 `engine/io` 讀進來，交給 Engine 的 `UfbxModelLoader`；列出某個模型的事件檔。
  - `WriteAnimationEvents`：照手寫檔的排版寫出 version 1；秒數用最短往返表示（`std::to_chars`），負零寫成 `-0.0`。
  - `CheckedText`：寫出的文字交給 Engine 的 `ParseAnimationEvents` 與 `BindAnimationEvents`，再逐欄比對（秒數比位元），全部通過才交出文字。工具沒有自己的規則副本（DI 2）。
  - `EventDocument`：新增、移動、改名、刪除；只負責保持片段內的秒數不減，其餘交給 Engine 判斷。
  - `OpenEvents`、`SaveEvents`、`CreateEvents`：
    - 讀檔前取 stamp，用雜湊確認讀到的位元組就是 stamp 對應的內容；
    - 存檔前檢查檔案還是開啟時那份；
    - 存檔後把 stamp 和寫入的位元組比對；
    - 新建時先寫事件檔，最後才寫 catalog，id 要是新的，路徑上不能已有檔案，模型 id 要相同。
- **CLI `gyo_model_viewer`：** `models`、`info --model`、`roundtrip --events`，都不寫任何檔。
- **依賴守衛：**
  - 檢查 include 前綴與 `Name::Name` 目標的正面清單；
  - `target_link_libraries` 裡出現的每個名稱都要在允許清單內；
  - 不得 include `imgui_internal.h`；
  - 不得提到 `tools.csv`／`projects.csv` 裡的其他 id（DI 6）。

## 驗證與經過

- 本機 macOS（`build/target/_build/mv`，`GYO_TOOLS=model_viewer`，`BUILD_TESTING=ON`）：
  - `gyo_model_viewer.*` 4／4 通過；core 的測試 14 個 case、157 個 assertion。
  - 整棵樹的 `cpu|shader`：56／56 通過。
  - 第一次跑整棵樹時，`os.boundary_guard` 失敗：「catalog 寫失敗」的測試在 OS 層以外用了 `_WIN32`（docs/architecture/os.md 的 R2）。
    - 改成執行時探測：目錄改成唯讀後，如果仍寫得進去（Windows、root），就跳過並印出訊息。
    - 守衛沒有改；改之後整棵樹 56／56。
- 件數の突き合わせ：一致。
  - 宣告 7 條，全部 killed。
  - 第一輪 13／13 killed；第二輪（審查後）18／18 killed，stale 0。
- 第一次跑測試時有一個 case 失敗，原因是測試的預期寫錯，實作沒有錯：
  - framework 的掛載依 id 排序列出資產，不是照 catalog 原本的順序；
  - 被 Engine 拒絕的檔，原因是「缺欄位」，不是版本號；
  - 改了預期和標頭註解，實作沒動。
- L1（[PR #146](https://github.com/yojinn-io/GYO-Engine/pull/146)，手動 dispatch，run 41805066180）：
  - linux-x64、macos-arm64 綠；windows-x64 的 `gyo_model_viewer.cli` 失敗：`roundtrip` 回報「讀回相同，但排版不同」。
  - 原因切分（§14.1）：Test 自身。CMake 的 `file(WRITE)` 以文字模式開檔，Windows 上 `\n` 變成 `\r\n`，事件檔就不是寫出程式給的 LF 位元組；工具回報「排版不同」是對的。
  - 根據：同一個比對在 core 測試（C++ 字串）裡，Windows 也通過了。
  - 修正：CLI 測試改用 `file(CONFIGURE … NEWLINE_STYLE LF)` 寫事件檔，「沒寫檔」的檢查改用 SHA-256 比對。判定不變（逐位元組相同）。
  - 照 D184 只重跑殺它的突變 `mv-info-without-binding`：killed（`mutations-ci-fix.txt`）。
  - 修正後重新 dispatch（run 41810410842，`4712ccff`）：L1 的 linux-x64、macos-arm64、windows-x64 與 Quick acceptance 全部 success。
- 探索：PvP 的 `object_fps_pvp.anim_events.nailer_v2.viewmodel` 用 `roundtrip` 往返後逐位元組相同；`info` 讀到 nailer 的 5 個片段（`Armature|Reload` 3.733 s）。這不是判定 P2，P2 在 `-trial` 照宣告做。
- 審查的指出與處理：見 [model-viewer.md](../tools/plans/v8/model-viewer.md)「`-core` 實作後的結果」。全部在本批修正，並加了突變。
- 證據：使用者的 Mac，主 checkout `build/evidence/tool-v8-model-viewer/core/`，有 `mutations.txt`（第二輪）、`mutations-round1.txt`、`mutations-ci-fix.txt`、`ctest.txt`、`ctest-all.txt`、`pvp-explore.txt`、`base-commit.txt`。

## 和事前宣告不同的地方

- CLI 改成選項的形式，命令是 `models`、`info`、`roundtrip`；`roundtrip` 不寫暫存檔，在記憶體裡比對。理由寫在 model-viewer.md。
- 多一個工具自己的 fixture：`tests/model_viewer/fixtures/animated_triangle.fbx`。

## 發現（不在本批範圍）

- audio_editor 的依賴守衛只看 `Name::Name` 形式的目標，抓不到遊戲的一般目標名（審查順帶指出）。
