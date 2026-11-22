# 存檔版本 Module（`engine/modules/save_version`）

更新：2026-11-22（新頁，`v8-arch-gyo2-restore-impl`）。由來：[讀檔宣告](restore-entry-declaration.zh-Hant.md)修訂 12 第 19 節（使用者 2026-11-22 動工前核准）；使用者 2026-11-18 `v8-arch-gyo2-save-version-module`：「需要提供version module，默認不啟用，啟用之後過時檔會直接拒絕。這個補充不放在編排裡，但是得放計劃中。Engine需要支持。」。**Design Invariants 的最終文字要等合併前核准。**

Owner：架構。程式：`engine/modules/save_version/`（`GYO::Engine::SaveVersion`，target `GYO::SaveVersion`，登記單位 `SaveVersionModule`）。行為測試：`tests/common/save_version/`。作者的寫法：[說明頁](system-contract-page.zh-Hant.md)第 15.1 節。

## 一個例子

RPG 1.0 的存檔格式是第 3 版。1.5 改了背包，作者把現在的版本訂成 5、最舊支援訂成 4。玩家拿 1.0 的存檔（第 3 版）按「讀取」：讀檔程式讀出 3、交給 `Policy::Check`，回「太舊」；選單顯示「請開新遊戲」，現在的 World 照跑。存檔時寫上 `Policy::Current()`（5）。

## 1. 位置與依賴

```text
GYO::Base ◄── GYO::Orchestration ◄── GYO::SaveVersion
```

- 登記檔一列：`library_only=no`、`default=no`（第一個預設不帶、會進 World 的單位）、`depends` 空。
- 產品啟用：`gyo_product_units(<target> MODULES save_version)`；設定 `SaveVersionOptions{current, oldestSupported}` 經 `ProductDescription::options` 給。

## 2. 介面

| 名字 | 說明 |
|---|---|
| `SaveVersionOptions{current, oldestSupported}` | 產品的兩個數字；沒給或 `oldestSupported > current` 是 `Configure` 的回報（啟動錯誤） |
| `Policy::Current()` | 存檔時寫的版本 |
| `Policy::Check(版本)` | 收 `oldestSupported ≤ 版本 ≤ current`；否則 `OrchestrationError{PrepareFailed}`，`reporterCode` 是 `SaveVersionCodes::tooOld`／`tooNew` |
| `SaveVersionCodes` | `tooOld`、`tooNew`、`optionsMissing`、`optionsOrder` |

`Policy` 放在 Product 的共用處：runner 用 `std::as_const(product.Buffer()).Find<Policy>()`，登記回呼用 `std::as_const(setup.ProductBuffer()).Find<Policy>()`。

## Design Invariants

- **SV-1 只比數字。** Module 不讀、不寫存檔，不定格式；只收作者交來的整數（`v8-arch-gyo2-save-unaware-means-format-only`）。
- **SV-2 預設不帶、啟用也沒有每幀的成本。** `default=no`；啟用時只在 `Configure` 放一個 `Policy` 進 Product 的共用處，不登記任何 World 的元件、Resource、System。
- **SV-3 拒絕是 `Result`。** 不發事件、不寫日誌、不中斷 World；碼固定兩個（`tooOld`、`tooNew`）。檢查由作者呼叫；編排層沒有替這個 Module 多加的步驟。

### Replacement Criteria

- **RC-SV1**：有點名的 Feature 需要 Engine 讀出版本（作者選用的檔頭）或自動攔（編排層在替換前問各單位）。
- **RC-SV2**：有點名的 Feature 需要轉換舊存檔（使用者提過「或者提供存檔轉換功能」）。
- **RC-SV3**：職責要多一項（例：記住每個存檔的版本、讀檔案）→ 停下來交使用者。
