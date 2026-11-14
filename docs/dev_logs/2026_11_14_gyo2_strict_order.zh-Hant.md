# GYO2 重建：歧義的嚴格模式 `Registrar::RequireExplicitOrder`（`v8-arch-gyo2-strict-order`）

日期：2026-11-14。Owner：架構（GYO2 重建線，架構師會話開出的任務卡）。分支 `claude/gyo2-strict-order-48a0e6`（從 `gyo2` 的 051d686c 切出）。根據：使用者核准當作「基礎的 System 契約」的圖二 `docs/architecture/target/v2/diagrams/control-plane-revised.svg` 的歧義框「嚴格模式（選用）：啟動錯誤」；重構稿 M3b（`rebuild-concept.zh-Hant.md` 1.6 節：「升成啟動錯誤」是選用的，由要它的那一層開）；先例 `Registrar::RequireExplicitZones`（`v8-arch-gyo2-zone-default-b`）。

## 做了什麼

- `Registrar::RequireExplicitOrder()`：任何 plugin 在 Build 裡呼叫，整個 Registry 生效；呼叫兩次等於一次。打開之後，同一個 stage 裡存取衝突、又沒有順序路徑的一對 zone B System，是啟動錯誤 D-E8（rule `M3b`），不再依登記順序平手。
- 錯誤點名兩個 System、衝突的資料、stage 與 schedule、是哪個（哪些）plugin 要求的，並告訴作者寫 before／after（System 的或它所在 set 的）。subjects 依序是兩個 System、資料、schedule、要求的 plugin。一次啟動把每張 schedule 的每一對都報出來：報錯的那一對不加邊，所以後面的對不會因為前面的平手邊而被遮住。
- 有一方是 zone C 的一對，照舊是 D-E8（rule `D-3`），不重複報。
- 不打開時行為不變：平手照舊依登記順序並列在 `FrozenRegistry::TieBreaks()`。
- 開關不進 Registry 指紋與 C 指紋（與 `RequireExplicitZones` 相同）：它只增加啟動錯誤，打開後能完成的 Registry，表與 Plan 都與不打開時相同。J-18 的 golden 沒有變。
- 沒有豁免（Bevy 的 `ambiguous_with`）：嚴格模式下作者要寫 before／after。

## 經過

- 先查現況與歷史：舊規格的「(c) 區歧義＝啟動錯誤」（主張 A，D.2 第 10 步，D-E8）已經在 `Scheduler.cpp` 實作，不受開關影響；`Zone` 預設 B 之後，不用決定性區的產品碰不到它。舊規格 K.1 的對照寫「(c) 固定 Error、(b) 固定穩定平手；不可設定」、「不提供 `ambiguous_with` 豁免」；GYO2 的重構稿 M3b 把「升成啟動錯誤」改成選用，這一批照 M3b 做。
- 參考實作：Bevy 每張 schedule 一個 `ScheduleBuildSettings::ambiguity_detection`（`LogLevel` 的 Ignore／Warn／Error，預設 Ignore），另有三種豁免（`ambiguous_with(set)`、`ambiguous_with_all()`、`allow_ambiguous_component/resource`）。
- 形式比較了（甲）整個 Registry 一個開關、（乙）每張 schedule 一欄、（丙）只對某一類資料，選（甲）。比較與待決事項寫在 PR 說明。

## 驗證

本機 Intel Mac（i9-9980HK），preset `core`（RelWithDebInfo）。

- `tests/common/kernel/StrictOrderTests.cpp` 8 個案例、56 個檢查通過（ctest `kernel.strict_order`）。
- 完整 ctest：見下方「結果」。
- 突變：見下方「結果」。突變只在建置成功、而且預期的那個案例被執行並失敗時算抓到。
- 三平台的 L1：在 PR 上跑，結果記在 PR。
- 沒有做別模型的對抗檢查。

## 結果

- 完整 ctest 94／94 通過（含 `kernel.J-18` 的 golden、`kernel.guard`、`kernel.naming_guard`、骨架測試的「two writers with no order are tied by registration and listed」）。既有測試沒有改。
- 突變 11 條，11 條都由預期的那個案例失敗抓到，沒有建置失敗：開關在 Scheduler 不生效、`RequireExplicitOrder` 什麼都不記、每個 System 只報第一對（漏報一對）、每張 schedule 只報第一個錯誤、只讀的一對也報錯、不打開也嚴格、呼叫者不去重、zone C 的一對重複報、subjects 少了 schedule、訊息少了解法、有順序路徑的一對也報錯。對照組（未突變）7 個預期案例都各執行 1 個且通過。

## 證據

主 checkout（這台 Intel Mac）`build/evidence/gyo2-strict-order/`：configure／build log、完整 ctest 的 log、突變執行器 `run_strict_order_mutations.py` 與它的輸出。
