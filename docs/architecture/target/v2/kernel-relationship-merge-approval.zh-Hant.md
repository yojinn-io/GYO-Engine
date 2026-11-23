# 合併前核准：Kernel 的關係與父子（`v8-arch-gyo2-kernel-relationship`，子批 KR）

更新：2026-11-23。**狀態：草稿，待使用者合併前核准。合併要使用者另外說「合併」。** 宣告：[kernel-relationship-declaration.zh-Hant.md](kernel-relationship-declaration.zh-Hant.md)（修訂 1，使用者 2026-11-23 動工前核准）。dev_log：[2026_11_23_gyo2_kernel_relationship.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_kernel_relationship.zh-Hant.md)。PR：yojinn-io/GYO-Engine#295（draft，base `gyo2`）。

要你定的四件：

1. **第 1 節 R-1**：刪父的同一個階段又插入「同一個父」，算不算 Q3 的「拆開」。
2. **第 2 節**：條文的最終文字（Kernel DI-KS 29～33、DI-KS 9、12、15 的補句、RC 26、27；回滾頁 R-2 前提 8）。
3. **第 3 節**：和宣告的偏差（實作與審查之後）。
4. **第 4 節**：判定的變更（§14.4）——動工前已同意的清單，加上停下後補列的四處（已同意，`-golden-extended`），這裡只確認結果。

---

## 1. 要你定的設計題

### R-1 刪父的同一個階段又插入同一個父（設計審查 F1、code review N-11）

**例子**：每幀都把武器的 `ChildOf` 重設成角色的 System（同值，什麼都沒改），剛好在角色被刪的那一幀也跑了。

| | 現在（Q3 的字面：本階段有 `ChildOf` 的拿掉或覆寫紀錄就不連帶） | **建議：同值再插入不算拆開** |
|---|---|---|
| 結果 | 武器留下、沒有父，留一筆 `RejectedRelationship` | 武器跟著角色刪；那筆插入因實體不在，照既有規則算進 `DroppedMissingEntity` |
| 和 Bevy | 不同：Bevy 不論先後都會刪（先插入再刪父 → 連帶刪；先刪父 → 子已被刪，插入沒作用） | 同 |
| 和 C15（同一個父再插入只寫 `ChildOf`） | 不一致：同值再插入改變了子的生死 | 一致 |
| 改法 | — | 收 `rewired` 時，插入紀錄的目標等於子現在的父就不收；補一個案例（`[kr-same-parent-then-despawn]`）與一條突變；DI-KS 32 加半句 |

Q3 的例子只有「拿掉」「換到別的父」「新的子掛上來」，沒有談同值；所以這是 Q3 沒寫到的範圍，不是推翻 Q3。

---

## 2. 條文的最終文字

現在的草稿在：

- `docs/architecture/target/kernel.zh-Hant.md`：DI-KS 29～33（新）；DI-KS 9、12、15 的補句；RC 26、27（新）。都標「草稿，合併前核准定稿」。
- `docs/architecture/target/v2/rollback.zh-Hant.md`：R-2 前提 8。

審查之後改過的地方：

- DI-KS 29：寫明內部寫死 `ChildOf`，「一張關係表」延到 RC 27 (b) 那一批（設計審查 F4 (a)）。
- DI-KS 30：例外補「本階段拆開或換父的子在父被刪之後」；寫明同一階段拿掉又放回、結果相同時 stamp 不動（code review S-2）。
- DI-KS 32：換父的子 `ChildOf` 留到它的紀錄（換父是覆寫、不跑 hook；code review S-4）；`Restore` 留下的子中，這次也要刪的留給它自己的 despawn（code review N-3）；連帶展開只沿 `ChildOf` 確實指著它的子（code review N-2）。
- DI-KS 33 與 R-2 前提 8：存取驗證打開時，給的 `ChildOf`／`Children` 不一致會報 W-1（設計審查 F4 (d)）。
- RC 27 (a)：補已知會碰到的使用者（Bevy 的 UI widgets 的分頁重排；設計審查 F9）。

R-1 選建議時，DI-KS 32 再加半句「同一個父再插入不算拆開」。

---

## 3. 和宣告的偏差

| # | 宣告 | 實作 | 理由 |
|---|---|---|---|
| D-1 | 12.3 `kr-linked-by-id` | 沒立；改立 `kr-linked-sorted`，`[kr-linked-order]` 的附掛先後改成和編號不同 | 「先展開再依編號」無法用一處替換表達 |
| D-2 | 12.3 `kr-restore-expands` 預期 `[kr-restore-no-expand]` | 實際抓到的是 `[kr-restore-unregistered-childof]` | `RestoreSet` 給了 `ChildOf` 時，它對子的拿掉紀錄已觸發 Q3 的豁免，那個案例分不出 |
| D-3 | 5.3、DI-KS 32：Q3 的子「關係在同一步拿掉」 | 留到它的紀錄套用 | code review S-4：同一個「交給新主人」的寫法，舊主人死不死會多一對 OnRemove／OnAdd |
| D-4 | 12.1 `[kr-load]`「生成期間」 | 用 System 的延後放回 | 生成世界是編排層的；Kernel 這一側走同一條套用路 |
| D-5 | 第 6 節 C15「衝突點：無」 | 衝突點是「從 Bevy 搬來、靠 `add_child` 再插入把子移到最後的寫法」，膠水是同一個階段拿掉再插入（說明頁第 19 節寫了） | 設計審查 F4 (b) |
| D-6 | 第 6 節的表 | 少一列「淨差」：Bevy 會 Remove＋Add `Children`，GYO 不通知、只有 `Changed<Children>`；衝突點 DI-KS 9 的分階段套用，膠水 0 | 設計審查 F4 (c) |
| D-7 | 5.2 的入口表 | 多一個入口：`DiagnosticsWriter<RejectedRelationship>` 也擋（照 `RejectedSpawn` 的先例） | 狀態權威（AGENTS §3） |
| D-8 | — | 負向樣本的 CMake 函式改成收 sample 與變體清單 | 兩份 Kernel 樣本共用；world_logging、world_asset 不在範圍 |

---

## 4. 判定的變更（§14.4）

宣告第 12.5 節（含補列）的期待值全部照「Kernel 多兩個欄」換成新的固定值；沒有緩的面。另外，被本批改到 find 的既有突變照新寫法改寫（同一個改變），3 條 J-32 突變的 expect 把 ComponentId 加 2——都是 `tests/common/mutation/README.md`「尋ねずに行い、事後に報告するもの」的範圍，列在 dev_log。

---

## 5. 審查與驗證

- **宣告的敵對式檢查**：Opus／high（和作者同模型）。blocker 5、should 11、nit 12，全部採用（`build/evidence/gyo2-kernel-relationship/declaration-r0-adversarial-opus.md`）。
- **設計審查**（批次表〈每批的設計審查〉）：Opus／high，和實作者同模型（原定 Fable；使用者「暫時用 Opus」）。整體合理；F1 交你（第 1 節）；F2、F3、F4 已改；F5、F6、F9 的部分、F10 之後再看（`design-review-opus.md`）。
- **code review**：Opus／high，同模型。沒有 must-fix；S-1～S-4 已改（S-1、S-2 用臨時測試重現）；nit N-1～N-7、N-9、N-10 已改，N-8 記下（`code-review-opus.md`）。
- 處理對照：dev_log「審查之後」一節。

## 6. 判定的結果

- 本機 macOS x86_64、`core` preset：ctest 197／197（`gyo_kernel_tests` 442 案、`gyo_rollback_tests` 56 案；審查修正後的版本）。
- 突變（D184）：新加 `kr-` 33 條，33／33 killed；相關的既有突變（find 落在改動範圍內、或被本批改得對不上而照新寫法改寫）31 條＋rollback 1 條，全部 killed（審查修正後重跑：`build/evidence/gyo2-kernel-relationship/review-fix-kernel-mutants.log` 64／64、`review-fix-rollback-mutants.log` 1／1）。全清單 2209 條，本分支上不唯一的 find：0。
- §14.5 的數量：新加 33 條；本批新增或改寫的條文 9 條 ×5＝45，未超過。
- 負向編譯樣本：`kernel.relationship_compile_fail` 對照組可編譯、20 個變體以各自的標記失敗。
- L1 三平台：審查前的 999ecc6d success（run 45125895018）；審查修正後的 commit 另跑一次（結果寫在 PR）。
