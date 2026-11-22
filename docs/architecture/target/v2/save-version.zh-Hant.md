# 存檔版本（`engine/modules/save_version`）

更新：2026-11-22（新頁，`v8-arch-gyo2-restore-impl`；同日照合併前的設計審查 S1，使用者「選(B)，照建議改」改成純程式庫）。由來：[讀檔宣告](restore-entry-declaration.zh-Hant.md)修訂 12 第 19 節（使用者 2026-11-22 動工前核准）；使用者 2026-11-18 `v8-arch-gyo2-save-version-module`：「需要提供version module，默認不啟用，啟用之後過時檔會直接拒絕。這個補充不放在編排裡，但是得放計劃中。Engine需要支持。」。**Design Invariants 的最終文字：使用者 2026-11-22 合併前核准**（[合併前核准頁](restore-impl-merge-approval.zh-Hant.md)，原話「照建議，全部核准」）。

Owner：架構。程式：`engine/modules/save_version/`（`GYO::Engine::SaveVersion`，target `GYO::SaveVersion`；沒有登記單位）。行為測試：`tests/common/save_version/`。作者的寫法：[說明頁](system-contract-page.zh-Hant.md)第 17.1 節。

## 一個例子

RPG 1.0 的存檔格式是第 3 版。1.5 改了背包，作者寫 `constexpr SaveVersion::Policy kSaveVersion{5, 4};`（現在 5、最舊收 4）。玩家拿 1.0 的存檔（第 3 版）按「讀取」：讀檔程式讀出 3、交給 `kSaveVersion.Check(3)`，回「太舊」；選單顯示「請開新遊戲」，現在的 World 照跑。存檔時寫上 `kSaveVersion.Current()`（5）。

## 1. 位置與依賴

```text
GYO::Base ◄── GYO::Orchestration ◄── GYO::SaveVersion
```

- 登記檔一列：`library_only=yes`、`default=no`、`depends` 空。要用的產品 link `GYO::SaveVersion`；不用就沒有任何成本。
- 依賴編排層只為了拒絕的型別 `OrchestrationError`（準備函式可以原樣回傳）。

## 2. 介面

| 名字 | 說明 |
|---|---|
| `Policy{current, oldestSupported}` | 作者自己持有的一份（建議 `constexpr`）。`oldestSupported == current` 只收現在的版本 |
| `Policy::Current()` | 存檔時寫的版本 |
| `Policy::Check(版本)` | 收 `oldestSupported ≤ 版本 ≤ current`；否則 `OrchestrationError{PrepareFailed}`，`reporterCode` 是 `SaveVersionCodes::tooOld`／`tooNew`。分辨原因看 `reporterCode` |
| `SaveVersionCodes` | `tooOld`（`"save-version-too-old"`）、`tooNew`（`"save-version-too-new"`） |

## Design Invariants

- **SV-1 只比數字。** 不讀、不寫存檔，不定格式；只收作者交來的整數（`v8-arch-gyo2-save-unaware-means-format-only`）。
- **SV-2 預設不帶、沒有登記。** 純程式庫（`library_only=yes`、`default=no`）：不登記任何東西，不 link 就沒有成本。
- **SV-3 拒絕是 `Result`。** 不發事件、不寫日誌、不中斷 World；碼固定兩個（`tooOld`、`tooNew`）。檢查由作者呼叫；編排層沒有替它多加的步驟。

### Replacement Criteria

- **RC-SV1**：有點名的 Feature 需要 Engine 讀出版本（作者選用的檔頭）或自動攔（編排層在替換前問各單位）。
- **RC-SV2**：有點名的 Feature 需要轉換舊存檔（使用者提過「或者提供存檔轉換功能」）。
- **RC-SV3**：職責要多一項（例：一處設定給多個單位共用、記住每個存檔的版本、讀檔案）→ 停下來交使用者。
