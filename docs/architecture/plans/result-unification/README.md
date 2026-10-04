# Assert／Result 統一：分批計畫與進度

更新：2026-10-04。Owner：Engine（`engine/base`，R1 起為獨立 target `GYO::Base`）。
**R0 本機驗收完成，PR [#30](https://github.com/yojinn-io/GYO-Engine/pull/30) 待 CI。**

GYO 從 2026-01 起就有 `Base::Result<T, E>` 與 `Base::Error<Code>`，但這套慣例從未寫成文件，
後來的模組各自用例外、`std::string`、手寫結果型別或 out-param 表達失敗。
本計畫依「Assert = Programmer Error、Result = Runtime Error」建立統一的失敗處理，
目標是可讀性、Apps 以外的風格統一，以及單一套處理規則。

先讀 [核准版計畫](PLAN.md)（概念、規格、批次與驗收）和 [交接](HANDOFF.md)。

## 已定案方針（2026-10-04）

1. 核心概念：Assert 表示 Programmer Error，Result 表示 Runtime Error。optional（單純沒有）、diagnostics、遊戲結果都不是錯誤；例外不是第三種錯誤機制。
2. 範圍：`engine/`、`tools/ui_editor`、`tests/common`、`tests/ui_editor` 完整統一。Apps 只做被迫的修改；未啟用產品不修改。
3. 保留兩個參數的 `Result<T, E>`；E 符合 `CodedError`（enum `code` 加 `message`）。不做全域 error enum、error chain，不升 C++23。
4. 資料驗證型 API（RenderQueue、ClipSprite 等）的輸入錯誤屬於 Runtime Error，維持回傳 Result。API 誤用屬於 Programmer Error，改用 Assert；只有 SdlGpu device 的狀態錯誤例外。
5. Assert 適用於整個 engine，包括 Math；`GYO::Base` 是比 Math 更低的 leaf。只在 debug 生效的 `GYO_DEBUG_ASSERT` 等第一個昂貴檢查出現時再加。
6. 舊名稱完全移除，不留別名。

## 進度

| 批次 | 建議檔位 | 狀態 | 交付邊界 |
|---|---|---|---|
| R0 任務校正 | medium（盤點用 ultracode；characterization 測試 high） | 本機驗收完成，PR [#30](https://github.com/yojinn-io/GYO-Engine/pull/30) 待 CI | 計畫文件、基線、分類表、稽核腳本、Vfs 與 KeepOldIfAny 的 characterization |
| R1 概念文件與 Assert | high（`Assert.hpp` xhigh） | 未開始 | `GYO::Base`、`Assert.hpp`、`error-handling.md`、Collision／FixedTickRuntime／`Math::Clamp` 改用 Assert |
| R2 Result 的寫法 | high（`Result.hpp` xhigh；codemod medium） | 未開始 | `Base::Err`、隱式成功、移除 `Ok`／`Err`／`ok()` |
| R3 Error 語意與 asset | high | 未開始 | `CodedError`、`Describe`、移除 `None`、別名收斂、AssetRecord；asset 的 API 誤用改 Assert |
| R4 Model、ui／render | high | 未開始 | Model／ModelRenderer 的 E、保留 code 的轉換；ui／render 的 API 誤用改 Assert |
| R5 ui_editor | high | 未開始 | 結果型別、out-param、assert、`Describe` |
| R6 收尾 | medium（最終審查 high） | 未開始 | 文件定稿、稽核、破損清單、Architecture Report |

```text
R0 -> R1 -> R2 -> R3 -> R4 -> R5 -> R6
```

## 執行規則

- 每次只執行使用者指定的批次；一批一個 PR，commit 與 PR 用日語。
- 每批開始、里程碑、停止時更新本表、HANDOFF 與 dev log，然後停止，不自動開始下一批。
- 主對話檔位由使用者決定；表中檔位是建議值。使用 ultracode 前先詢問使用者。
- 出現非預期回歸或範圍超出 R0 分類表時，停下回報並重新規劃。
