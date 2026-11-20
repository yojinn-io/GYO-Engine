# 異常分級（`v8-arch-gyo2-error-levels`）

日期：2026-11-20。分支 `claude/gyo2-error-levels-170217`（從 `gyo2` 8ec005f2）。**狀態：實作完成，守衛判定 G1～G3 已核准（2026-11-20）；等合併。**

這一批由任務卡做（GYO2 主會話開出）。事前宣告：[error-levels-declaration.zh-Hant.md](../architecture/target/v2/error-levels-declaration.zh-Hant.md)（修訂 7 收窄範圍，以第 19 節為準；之前各節保留作紀錄）。

## 做了什麼

**範圍**（修訂 7，使用者指正後收窄）：分級是資訊——回給上層的錯誤自帶「發生了什麼、多嚴重」，照原路經 `Result` 帶出去，不替外面決定或強制處理。程式只做讓 Engine 不在回傳之外再多記一筆的兩件，加上這批查出的兩個缺陷。

| 項目 | 提交 | 內容 |
|---|---|---|
| D1 | f38e5691（D8 的部分由 62c2f293 撤回） | Kernel 拿掉錯誤處理器：不再往 stderr 印；`Err` 照舊由最外層 `Run`／`Trigger` 回傳一次。`WorldExecutor::SetErrorHandler`、`Product::SetErrorHandler` 拿掉。`kernel_guard.py` K6 拿掉 Executor 的豁免 |
| D1 的測試與突變 | b814ccab | 測試台從回傳值記錄；退役 2（`ns-nested-calls-handler`、`or-handler-not-kept`）、改寫 4 |
| D2 | 8bb2bbab | 編排層拿掉五處 `Base::Log` Warning；被拒的錯誤訊息帶 owner；stderr 探針共用化（`tests/common/support/SilentProbe.cmake`）並加到編排層 |
| P15（D10） | 6ee653be | `BuildNewFirst` 替換第一個 World、遊戲初始化在新 World 失敗時不算跑過；先 Red 再 Green |
| P12（D4） | 33f94d20 | `DropOldFirst` 丟掉舊 World 之後的全部步驟丟例外時，先記 failure 再往外拋；記錄用的 `WorldFailure` 先組好；替換中的狀態明示；先 Red 再 Green |
| 條文草稿 | bfd88c6a、98589624、7d823463 | `target/error-handling.md`「Error levels」一節與 DI 1～5；DI-KS 2／17／18／26、CCR-2／3、O-1／3／4／9／11、命名規範 N25、Legacy 標記 |

**撤回**（`git revert`，見宣告第 19 節）：D5 編譯錯誤（269c6058）、D3 拆型別與改名（3c67aec3、ebd4c01e）、D8（72442db2、62c2f293）、D9（808d4a53）。

## 驗證與經過

- 本機（macOS，AppleClang 21）：`kernel|orchestration|world_time|base|scenario` 95～96 個 ctest 全過；`kernel.el-silent`、`orchestration.el-silent`（stderr 全空，正例證明抓得到）過；`kernel_guard` 乾淨。
- 突變（相關的，HANDOFF D184 的規則）：編排層 23、Kernel 4，共 27 個全殺（含改寫的 `or-failure-logged`：D2 之後 `Product.cpp` 不含 `Base::Log`，原突變編不過，改成往 stderr 寫、由探針抓）。P12 的重構讓 15 個既有突變的 find 對不上，照新程式改寫，全殺。
- 守衛判定 G1～G3 的 §14.4 驗證：驗證者 Opus 5.5／high（和實作者不同的 Agent），三組都成立；報告存主 checkout `build/evidence/gyo2-error-levels/guard-change-verification.md`（本機 macOS，git 不追蹤）。驗證者另外用臨時突變（補回 4 處 `Base::Log` Warning）確認 7 個「記 0 筆」判定都會失敗，原始輸出未保存。
- 三平台 L1：run 44755670680（7d823463）。
- 敵對式檢查：宣告修訂 0、修訂 2 各一次（Fable 5.2／high），報告在主 checkout `build/evidence/gyo2-error-levels/`。

## 突變的增減（§14.5）

- 新增：`el-kernel-print-back`、`hf-limit-err-not-returned`（由 `hf-limit-handler-twice` 改寫）、`or-gi-kernel-twice`（由 `g8-handler-set-late` 改寫）、`el-insert-owner-dropped`（由 `g8-insert-warning-text` 改寫）、`el-replace-fail-warn-back`、`el-insert-warn-back`、`el-replace-catch-warn-back`、`el-p15-old-behaviour`、`el-p12-unrecorded`。每個都寫了只有它捕的誤與 `guards`。
- 退役：`ns-nested-calls-handler`（它要捕的「巢狀層也呼叫處理器」隨處理器消失；改法現在由 `[ns-again]` 捕，等同 `ns-left-error-not-kept`）、`or-handler-not-kept`、`or-insert-no-warning`、`or-insert-two-warnings`、`g8-insert-build-no-warning`、`g8-beforeswap-failure-no-warning`、`or-d3-throw-no-warn`（要找的 `Warn` 不在了）。
- 已知缺口：7 個「Engine 記 0 筆」（`*-silent`）判定沒有專屬突變——補回 `Base::Log` 要同時加 include 與呼叫，執行器一次只改一處；`el-*-warn-back` 補的是直接寫 stderr，由探針捕。驗證者已用臨時突變確認 7 個判定會失敗。
- G1 的 46 處 `recorded` 判定在新測試台下和同案的 `REQUIRE_FALSE`／`HasCode` 冗餘（驗證者）；簡化不在本批。

## 證據的所在

本機 macOS，主 checkout `build/evidence/gyo2-error-levels/`：參考引擎偵察、Java／Go 原文研究、兩次敵對式檢查、守衛判定驗證、CI log（run 44677942143、44729348324）。

**守衛判定 G1～G3 的核准**（2026-11-20）：
- 原話：使用者在主會話說「G1～G3 核准」。
- 比過的選項：核准／某一組不核准。
- 理由：使用者沒有另外說。主會話給使用者的說明是「三組都是舊機制拿掉後，測試從看舊機制改成看新概念，只檢查 Engine 自己的行為」。
- 出處：主會話對話 2026-11-20，經主會話轉來。
- 另記：AGENTS §14.2 要求的「和一覽作者不同模型的敵對式檢查」取消。使用者原話：「我改的是概念和機制，你用對抗1000%會有問題，現在這個機制不欄會寫垃圾代碼的人。」（「不欄」照原文。）讀法：這次是概念的改變（錯誤是資訊、Engine 不強制），敵對式檢查會把「擋不住寫垃圾代碼的人」當成缺口，把方向推回強制。§14.4 的驗證者核對（Opus 5.5／high）照做，三組都成立。
