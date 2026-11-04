# framework 整改：v8-framework-invariants（收尾：刪舊件、拿掉例外清單、DI 4 定稿）與整條線的總結

日期：2026-11-01～02。分支 `claude/framework-rework`。上段（Design Invariants：DI 4 定稿、守衛清空；公開介面：刪舊公開標頭）。実装者：主會話 Opus 5.5／high（文件與突變 medium）；審査者：Opus 5.5／high 子 Agent；§14.4 検証者：Fable 5.1／high（使用者同意）。估 7～10 h，實際約 5 h。

**狀態：** 完成（Mac 上的建置、全量 CTest、L2 兩種組態、完整突變）。Windows、Linux 由 CI 的 L1 驗證。

相關文件：[判定變更](../architecture/plans/framework-rework/invariants-judgment-change.md)、[HANDOFF](../architecture/plans/framework-rework/HANDOFF.md)、[PLAN 第 2.6 節](../architecture/plans/framework-rework/PLAN.md)。

## 做了什麼

- **B1 刪舊件**：FileService、CatalogRegistration、PathDialog（PathDialogInbox）、ImGuiSdlHost、SdlPathDialogs、`framework/Legacy.cmake`（含 `GYO::ToolFrameworkGui`）與只測它們的測試。`AbsolutePath` 依 D-2 留在核心，搬到新標頭 `gyo/framework/AbsolutePath.hpp`（行為逐行等價，新增 3 條測試）；ui_editor 的 include 照工具線同意在這批一併改。`FrameworkError` 的 `FileError` 仍有 ui_editor 使用，保留。
- **B2 守衛收尾**：例外清單與 ratchet 機制一起拿掉，守衛只剩正向清單；清單檔再出現就報錯。configure 時的連結檢查不再允許例外。
- **B3 文件**：`docs/framework/architecture.md` 的組成清單、DI 2 守衛句、DI 4（framework 不寫自己的檔案；JSON 綁定回傳新文字，由呼叫者經 engine/io 原子替換）、Using it；`docs/architecture.md` 的 framework 列。
- **B4 突變定版**：framework 目錄退役 19 條、新增 5 條（共 18）；os-subsystem 目錄退役 ui-x1、ui-x2（改的是已刪的 FileService）。

## 驗證與經緯

件數の突き合わせ：一致（完整突變：宣告 87・執行 87・killed 87（第一次 86＋重跑 1）・stale 0；Windows 限定 1 條 skipped、ASan 1 條不跑）。

- **建置與測試**：test 樹建置成功、全量 CTest 123/123；只選 audio_editor 的樹 configure 與建置成功；不選工具的樹 configure 成功、沒有 framework target。B1 時我一度誤刪 `FileError`（grep 輸出被 head 截斷），建置失敗後還原。
- **自測**：37/37。交叉：新自測對舊守衛 3 條失敗、舊自測對新守衛 12 條失敗；舊自測全部 49 次掃描逐次比對，舊失敗→新通過 0 次、舊通過→新失敗 1 次（例外清單的收緊）。
- **宣告的判定不成立兩次**（§14.1，都先回報再依使用者指示處理）：
  - framework 目錄第一次 16/18：fw-link-closure-sdl、fw-link-genex 的預期「links 2」算進了已刪的 nlohmann 例外。使用者「照建議重跑」，改成「links 1」後 2/2。之後的預檢加一項：用改動後的實際輸出核對預期訊息。
  - 完整突變第一次 framework_shell 25/26：fw-shell-binding-missing 被 ui_editor 自己的檢查（#131 加入）先擋下，framework 的檢查在選了 ui_editor 的樹裡被遮住（GyoBuild 先加工具、後加 tests/framework）。不是這批引入的。使用者「照 (a) 處理」：預期兩種訊息都接受、寫明限界，重跑 1/1。
- **審査**（Opus 5.5／high）：0 blocker、0 major、3 minor、4 nit，都已處理。
- **検証**（Fable 5.1／high）：§14.4 五項全部確認；新舊守衛各跑 243 個合成輸入，舊失敗→新通過的 33 個全是程式碼以外（註解、字串或字元字面值、include 路徑）的 SDL_GPU 名稱；3 minor、2 nit 都是文件措辭，已改。
- 使用者核准條文與判定變更（「核准」）。

## 範圍的限界與留下的事

- 選了 ui_editor 的樹裡，`tests/framework` 的「綁定該建卻沒建」檢查被 ui_editor 自己的檢查遮住，沒有突變證明它（fw-shell-binding-missing 的 what 已寫明）。
- `FileError` 現在只有 ui_editor 使用，依 DI 5 是否搬回工具，記為之後的候選。
- 本線以外失效的突變錨點：input-and-present 目錄的 m7、m-main-thread（只回報）。
- `tests/framework/CatalogStructureTests.cpp:116` 的註解點名 audio_editor（不是這批加的）。
- IU-1（Input→Ui adapter 放 framework 核心）由 v8-input-ui-adapter 推動會話在這批合併後執行。

## 整條線的總結（framework-rework）

PR A（yojinn-io/GYO-Engine#127）：守衛 ratchet、選擇窗的 OS 半進 Engine、使用者檔案進 engine/io、產品外殼與 ImGui 綁定、catalog 結構與 JSON 綁定分開、保留額外欄位。PR #133：framework 可用任何 Engine 模組、共通功能先放 framework。本 PR（PR B）：刪舊件、拿掉例外清單、DI 4 定稿。工具側的切換由工具線完成（#131，含 T4 與 #132）。結果：framework 核心不 include、不具名、不連結 SDL／ImGui／nlohmann 與 `*Native`，沒有任何例外；ImGui 與 JSON 是可替換的綁定；選擇窗與使用者檔案的 OS 部分在 Engine。

## 證據

使用者的 Mac，主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/framework-rework/v8-framework-invariants/` 與 `.../pr-b-full-mutations/`（git 不追蹤，見各資料夾的 README）。審査者與検証者的原始輸出在會話暫存區，未保存。
