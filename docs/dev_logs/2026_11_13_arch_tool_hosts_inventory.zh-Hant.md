# 架構 v8：v8-arch-tool-hosts-inventory（四個工具 host 骨架的盤點）

日期：2026-11-13。Owner：架構（架構師會話開出的任務卡）。計畫：[kernel README](../architecture/plans/kernel/README.md) 文件清單、遷移計畫 §3.24 `v8-arch-plugins-editor` 與 §3.25～3.28 工具四批（分支 `claude/arch-migration-plan`）、使用者決定 `v8-arch-framework-retire`、`v8-arch-plugin-selection-data`。
- 執行：本卡會話（Fable；卡建議 Opus，使用者在 Fable 上說「開始」；medium，讀碼與對照 high），工作樹 `.claude/worktrees/happy-jang-12009c`，分支 `claude/arch-tool-hosts-inventory`（自 origin/master `d0d8563f`）。
- 性質：只讀盤點。不改程式、守衛、契約；寫入只有報告、本 dev_log、kernel README 兩行（文件清單、dev_log 清單）、HANDOFF 現況一行。

## 起因

framework 退役、組合與第三方對接改由 plugins 層承擔（runtime-composition §4）。plugins-editor 批要決定 Editor 側的 plugin 切法，工具四批要把四個 client 改成「plugin 清單＋Run」。切之前要知道四個工具的 host 骨架今天各怎麼做、哪些是同一份、哪些是「同一件事各寫一份」（framework 的老問題），以及搬動時會撞到哪些守衛。架構師保留的一點：選擇窗記帳與關窗提示必須真的進共用 plugin。

## 做了什麼

- 報告 [tool-hosts-inventory.zh-Hant.md](../architecture/plans/kernel/tool-hosts-inventory.zh-Hant.md)：§2 骨架 10 項 × 4 工具（檔與函式、用的 framework 型別）；§3 共用程度彙總；§4 依賴邊逐工具（CMake 直接命名 vs. include 映射，附命令與命中數）；§5 性質來源與會撞到的守衛（引用 guards-inventory，不重做）；§6 工具專屬；§7 重複但內容不同的程式 15 列；§8 plugin 切法候選 9 個（候選，不是決定）；§9 範囲の限界。
- 讀碼：四工具的 `Main.cpp`、視窗 client（104／1,135／1,024／579 行）、存根、`FileAccess`、兩個量測模式、預覽與試聽的包裝、ui_editor 的對話三層與 `EditorApp` 的 host 函式；framework 的三個 shell 與 `Preview.cpp`；四工具與 framework 的 CMake；四支工具守衛與 framework 守衛的清單常數。
- 掃描：`scan_tool_hosts.py`（Python `-I`）掃 CMake 連結邊與 include 映射。正例先命中：**第一次 CMake 正例未命中**——`target_link_libraries` 的註解內有右括號、正規式提前結束，只掃出 3 個 PUBLIC 項；加去註解後 5＋3 個正例目標全命中。這是 AGENTS §14.2「使う前に既知の該当例で一度命中させる」攔下的掃描器缺陷。另：執行緒掃描 `git grep -P` 退出碼 1（正例 engine 命中 3 檔）；設定／最近檔掃描只命中 framework 關閉 ini 的兩行。

## 件数の突き合わせ

**不一致（已解釋）**：四工具都是 include 映出的 target 數大於 CMake 直接命名數——ui_editor 15／13、voxel 14／12、model_viewer 18／13、audio 12／10；多出的 11 個 target 全部是經 framework 或 `GYO::Engine` 的 PUBLIC 連結傳遞而未直接命名（`GYO::Base`、`Math`、`Time`、`PlatformSDL`、`Render`、`RenderBackendSDLGPU`、ui_editor 的直接 `SDL3/SDL.h` 一檔）；反向（連了但沒 include）0 個。覆蓋 40／40 格。

## 受理判定（看結果前宣告；數字於開始時補）

| # | 判定 | 結果 |
|---|---|---|
| 1 | 覆蓋：10 項 × 4 工具 ＝ 40 格，每格有檔與函式或「無此功能」 | 成立（40／40；「無此功能」8 格） |
| 2 | 依賴邊：每工具附 (a) CMake 去重數、(b) include 映射數、命令與命中數；(a)≠(b) 時逐 target 解釋 | 成立（不一致已逐個解釋；正例命中後才採信） |
| 3 | 零修改：`git diff --stat origin/master` 恰 4 檔 | 成立（見下） |

事前宣告送 parent 時 `send_message` 回「session parent not found」（`list_sessions linked:true` 回空），與 `v8-arch-guards-inventory` 同一情形；未經 parent 核定就寫了正文；**使用者 2026-11-13 於本會話核定**（PR #200）。另：PR 的第一次 CI 在 `Select CI scope` 失敗，原因是 `gh api` 讀 draft 旗標時 GitHub 回非 JSON（暫時故障），重跑即過，分支無變更。

## 主要發現（給 plugins-editor 與工具四批的宣告）

1. **四工具站在同一條 framework 鏈上**（`RunHeadlessShell` → `RunImGuiShell` → `RunWindowShell`），沒有一個工具自己 pump、自己建 `RuntimeLoop` 或碰 ImGui context／後端；例外是兩個量測模式（voxel、model_viewer）自己開平台、自己 `PumpEvents`。
2. **帶判斷狀態的重複三份**：選擇窗記帳三種（ui_editor 三層型別、audio 內嵌 `lastTag_`、model_viewer 常數 tag 無等待狀態＝CCR-1 不合規）；關窗提示三種（兩鍵／三鍵含存檔／跨幀 closure 守多種動作），**audio_editor 沒有關窗提示**；「磁碟上變了」提示兩份。統一時的行為差異要寫進 plugins-editor 的宣告：「Save and close」有無、是否守「開別的檔」、audio 是否開始追蹤 dirty。
3. **薄的逐字重複**：`InputString`（voxel、mv）、預覽的尺寸計算→`UploadFrame`→`ImGuiTextureId`→`ImGui::Image`→釋放（voxel、mv 各一份）、著色器路徑＋`--gpu-driver`＋CMake 複製 target（4 處）、`FileAccess` 的 10 行（4 份）、CLI 存根（退出碼 3／2／3／2）——framework 收了機制、沒收呼叫端。
4. **設定與最近檔四工具都沒有**：`tool-settings` 候選今天 0 個使用者，不建（AGENTS §1）；若之後要，先碰 framework DI 4／ui_editor DI 2 的「無 side file」。
5. **include 面比連結面寬 2～5 個 target**：工具守衛以連結命名守正向清單，plugin 的正向清單要以 include 面為準，否則會漏。
6. **會撞到的守衛**（引 guards-inventory）：四工具 `ALLOWED_TARGETS`／`ALLOWED_INCLUDE_PREFIXES` 無任何 plugin target 或前綴，第一個連 plugin 的 commit 就失敗；framework 守衛只掃 `framework/`、刪除後 CMake link check 硬失敗（`tests/framework` 同批刪）；PV2／PV5 的判定是 readback 形狀（重寫不搬）；`os_boundary_guard.py` 的掃描根沒有 `plugins`。

## 證據

- 主 checkout `build/evidence/tool-hosts-inventory/`（git 管理外，本機 macOS）：`scan_tool_hosts.py`、`scan.json`、四個 `*_include_hits.txt`、`cmake_edges.dot`、`greps.txt`、`migration-plan.snapshot.md`、`guards-inventory.snapshot.md`、`snapshots.rev`。
- `git diff --stat origin/master`：`docs/architecture/plans/kernel/tool-hosts-inventory.zh-Hant.md`（新）、`docs/dev_logs/2026_11_13_arch_tool_hosts_inventory.zh-Hant.md`（新）、`docs/architecture/plans/kernel/README.md`（＋2 行）、`docs/architecture/plans/kernel/HANDOFF.md`（＋1 行）。
- 時間：起點 2026-11-12 23:45（開分支）；掃描 00:07；報告、dev_log 與兩處補記約 00:30（估 3～5 h，實走約 45 分鐘；讀碼以並行工具呼叫壓縮，未執行工具與測試，見報告 §9）。

## 下一步

- ~~使用者核定事前宣告的數字（40 格；邊 13／15、12／14、13／18、10／12）與報告~~ 已核定（2026-11-13）；合併由使用者決定。
- plugins-editor 批的宣告以報告 §8 的候選與 §7 的行為差異為起點；editor-dialogs 的三個行為決定（§8）交使用者。
- 工具四批的正向清單改掛 plugin target 時，以 §4 的 include 面為清單起點。
