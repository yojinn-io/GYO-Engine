# PvP v8：v8-product-evict-animation-orphan（host 逐出時 snapshot 的 animation 沒刪）

日期：2026-11-03。分支 `claude/gifted-khorana-d38ac5`（entity-line）。檔位 high、上段（回歸修正）；實作者主會話 Fable 5.1、§14.4 檢證者 Opus 5.5（high，唯讀）。估 3～5 h，實際約 3.5 h（含兩次突變基準逾時的重跑與模型分派不一致的停頓）。

**狀態：** 完成。golden 的 `host-poor-connection/corridor` 一行照核准重生（`v8-product-evict-animation-orphan-approve`，使用者原話「核准重生」）。

件數の突き合わせ：一致（base 與 branch 都是 42 個情境、每個情境的 record 數相同；host-poor-connection/corridor 2400＝2400）。

## 現象與來源

- pv9 的回歸：hitbox-line A2（`4cdb5bad`）讓 Match 每 Tick 把 animation 放進 snapshot，但 `MatchRuntimeHost::JudgeConnectionQuality` 逐出玩家時只 `erase_if` snapshot 的 players 與 combat，沒刪 animation（修正前 `MatchRuntimeHost.cpp:759-760`）。Gateway 檢查 `len(in.Animation) != len(in.Players)`（`adapter.go:132`）→ `runtimeFailed`，房間裡所有人斷線。A2 的 `RequireValidAnimation` 只在 `PvpMatch::Snapshot()` 內，蓋不到 host 之後的刪除。來源由 v8 規劃會話在 master 上確認。
- 發現於 `v8-product-entity-contract` 的偵察（entity-model §9.0）。真正 Gateway 上的 runtimeFailed 沒有重現，程式路徑逐行核對。

## 做了什麼（AGENTS §14.3）

1. 重現：本工作樹（`cf3118f4`）build 的 digest runner 對 `host-poor-connection/corridor` 跑 `--dump`：record #2399（tick 2400）`players=1 combats=1 animations=2`，被逐出玩家 2 的 animation 留著。
2. 守衛 G3 `PublicationComponentsAgree`（新檔 `tests/object_fps_pvp/EntityModelTests.cpp`，CTest cpu）：每份 `TakeSnapshot()` 斷言 combat 與 animation 的 id 序列等於 players 的，且逐出一定要發生。修正前 **Red**：4806 個斷言只有 tick 2400 的 animation 那一個失敗。
3. 修正（`9ec4442e`）：`EraseEntity(WorldSnapshot&, PlayerId)`（一次刪三個元件，Assert 筆數相同；entity-model E-7 的唯一入口）、`RequireValidSnapshot`（發布前 Assert combat 的筆數與 id 順序等於 players，再 `RequireValidAnimation`）、host 逐出改用 `EraseEntity`。修正後 **Green**：G3 4806／4806；產品單元測試 397／397。
4. 事前宣告與 §14.4 的 1～5：[宣告文件](../object_fps_pvp/plans/v8/v8-product-evict-animation-orphan-declaration.md)。宣告的內容在結果之前寫在 entity-model §9.0（`cf3118f4`）。
5. golden 重生（本 commit）：只改 `host-poor-connection/corridor` 一行（`68c48ca5…` → `52a34210…`）；舊檔另存 `fixtures/authority_golden.pv9-anim-checkpoint.txt`，檔頭補一段；`authority_digest` CTest 通過（-O0 與最佳化的零角度子集一致）。

## 檢驗

- 兩樹比對：base `cf3118f4` 對本批，scale 1：「42 scenarios, 1 differing；host-poor-connection/corridor: first differing record 2399, field animations (2 -> 1)」。base 與 branch 每個情境的 records 數相同；5 個非 golden 情境相同。修正前後的 `--dump` 2400 筆只差 1 筆：`animations` 2→1 改值，並刪除玩家 2 的 8 個 animation 欄位（124 bytes）。
- 突變（D184，只跑本批 2 個）：`erase-skips-animation`、`host-erases-two` 2／2 killed。被殺的機制是推論（工具對 killed 不保存 detail）：在 G3 執行中被 `RequireValidSnapshot` 的 GYO_ASSERT 中止，以測試名比對到；G3 的 CHECK 的檢出力以修正前的 Red 證明。前兩次執行的**基準** ctest 逾時（`object_fps_pvp.cpu` TIMEOUT 60 秒），突變沒有執行，原因未切分（推測：環境，建置剛結束）；同款命令手動跑 24～27 秒通過但計時未保存；第三次正常。
- §14.4 檢證者（Opus 5.5）：1～5 的數據成立，4 處字句與證據不符已改（GYO_ASSERT 在所有建置設定都評估、差異是 1 改值＋8 刪除、突變被殺的機制、G3 的覆蓋範圍只在它的情境）。修正邏輯無實質漏洞；小建議未採（host 可 Assert `EraseEntity` 的回傳值）。
- 判定變更的揭露（§14.4 的 1～5）在宣告文件；緩める面＝record #2399 的 animation 一組；守著「發布的 snapshot 各元件 id 一致」的其他判定＝G3 與 `RequireValidSnapshot`。
- 範圍的限界：只比了 scale 1；逐出在最後一筆，逐出之後的 Tick 沒有被比到。四平台一致要在 L1 看（本情境是 golden）。正式版若再違反，Match 會 abort 而不是 Gateway 讓全員斷線。
- 證據：主 checkout `build/evidence/v8-product-evict-animation-orphan/`（Intel Mac，git 管理外）：before_dump、after_dump、dump_diff、g3_red、g3_green、tests_all、after_run.json、compare_scale1、mutations_run（1～3）與報告、authority_digest_ctest、authority_golden.before、verification_opus.md。

## 和計畫的關係

- entity-model 決定 3 (a)：排在 `v8-product-match-entity-split` 之前、同一條 entity-line PR；split 的 base＝本批的最後一個 commit。
- E-7 在 host 層自此有守衛（G3、`RequireValidSnapshot`）。

## 下一批

`v8-product-match-entity-split`：先寫事前宣告（entity-model §9.2），base commit 寫進 HANDOFF，取得同意後開 S0。
