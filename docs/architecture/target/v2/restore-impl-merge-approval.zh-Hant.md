# 合併前核准：讀檔的實作＋存檔版本（`v8-arch-gyo2-restore-impl`）

更新：2026-11-22。**狀態：合併前核准（使用者 2026-11-22，原話「照建議，全部核准」）。合併要使用者另外說「合併」。** PR [yojinn-io/GYO-Engine#276](https://github.com/yojinn-io/GYO-Engine/pull/276)。宣告：[restore-entry-declaration.zh-Hant.md](restore-entry-declaration.zh-Hant.md) 修訂 12 第 18、19 節（2026-11-22 動工前核准；19 節的形狀之後改成純程式庫，見開頭的註記）。dev_log：[2026_11_22_gyo2_restore_impl.zh-Hant.md](../../../dev_logs/2026_11_22_gyo2_restore_impl.zh-Hant.md)。

> **核准**（使用者，2026-11-22，本卡的對話）：原話「照建議，全部核准」。核准的是本頁的四件：SV-1～SV-3 與 RC-SV1～RC-SV3 的最終文字、「太新也拒」、突變 16 個超過門檻 15 的說明、和宣告的三處出入。比過的選項：全部核准／挑出有意見的那一件。理由：使用者沒有另外說。

要你看的有四件。

## 1. Design Invariants 的最終文字（新頁 [save-version.zh-Hant.md](save-version.zh-Hant.md)）

- **SV-1 只比數字。** 不讀、不寫存檔，不定格式；只收作者交來的整數。
- **SV-2 預設不帶、沒有登記。** 純程式庫（`library_only=yes`、`default=no`）：不登記任何東西，不 link 就沒有成本。
- **SV-3 拒絕是 `Result`。** 不發事件、不寫日誌、不中斷 World；碼固定兩個（`"save-version-too-old"`、`"save-version-too-new"`）。檢查由作者呼叫；編排層沒有替它多加的步驟。
- Replacement Criteria：RC-SV1（要 Engine 讀出版本或自動攔）、RC-SV2（要轉換舊存檔）、RC-SV3（職責多一項）。

Kernel 與編排層的 Design Invariants 沒有改。

## 2. Engine 自己多定的一條規則：太新也拒

你說的是「過時檔會直接拒絕」。卡片另外定了「比現在的版本新也拒」（玩家降版、或拿到別人新版的存檔），碼和太舊的分開，選單可以顯示不同的話。Unity 的做法是版本不相等就拒，所以也拒新版。宣告 19.3 答應在這裡列出來給你看。

## 3. 突變的數量超過 README 的門檻

`tests/common/mutation/README.md` 第 8 點：一批加的突變超過「改的 Design Invariants 條數 × 5」要說明。本批改的 Design Invariants 是 SV-1～SV-3（門檻 15），加了 16 個：
- 存檔版本 7 個（在門檻內）。
- 讀檔的 9 個（配發器 3 個改出貨的 `RangeAllotter`、Kernel 6 個）守的是宣告 18.3 的判定，不是 Design Invariants 的條文；每個都寫了它獨抓的誤り，和既有清單逐一比對過沒有同一個改法。

全部 killed；被本批改到的既有突變 7/7 killed。

## 4. 和宣告的出入

- 宣告 18.2 第 1 件寫「重新配號案用到的 Engine 部分由 Kernel 既有的突變守」：查了沒有守「`Spawn::New` 回傳配到的編號」的既有突變，補 `re-spawn-new-wrong-id`（只收緊）。
- `re-insert-before-spawn` 不是調換兩段，是在第 5 步加條件模擬「插入在生成之前」（一個突變只能改一處文字）。
- 存檔版本由登記單位改成純程式庫（設計審查 S1，你選 (B)）：判定 V4、V6、V7 拿掉，加「相等只收現在的版本」一案；突變 12 → 7（拿掉的 5 個守的東西已不存在）。

## 不需要你核准、事後報告的

- 既有測試只改標記字樣：`[or-restore-gold]` 兩處期待值不同，拆成 `[or-restore-gold-startup]`／`[or-restore-gold-in-game]`；讀兩次相同加 `[or-restore-twice-same]`。判定不變（AGENTS §14.4「緩める面がない」）。
- 舊的合成範例碼 `"save-version-unsupported"` 改成 `"save-version-too-old"`（三處，只是測試與註解裡的例子）。
- `orchestration_mutations.json` 有一個物件鍵重複（#270 解衝突留下），重新寫出時被正規化；解析結果不變（Fable 確認）。

## 驗證

- 本機 macOS Intel、preset `core`：`ctest -LE "gpu|network"` 170/170。
- L1 三平台：PR #276 的 CI（結果記進 dev_log）。
- 換模型檢查（Fable 5.2／high，唯讀）四次：宣告兩次、突變清單一次、合併前的設計審查一次。報告在主 checkout `build/evidence/gyo2-restore-impl/`。
