# v8-product-evict-animation-orphan 的事前宣告與 §14.4 的判定變更

更新：2026-11-03。Owner：`object_fps_pvp`。**狀態：等核准。** §14.4 的 1～5 已由檢證者（Opus 5.5，和提案者 Fable 5.1 不同引擎）以實際資料核對，指出的 4 處字句不符已改（報告在證據目錄 `verification_opus.md`）。事前宣告的內容在結果之前就寫在 [entity-model](../../entity-model.zh-Hant.md) §9.0（`cf3118f4`）。 契約：[entity-model](../../entity-model.zh-Hant.md) E-7、§9.0（決定 3 (a)）。檔位 high、上段（回歸修正）；實作者主會話 Fable 5.1、§14.4 檢證者 Opus 5.5。估 3～5 h。

## 這批修什麼

pv9 的回歸：hitbox-line A2（`4cdb5bad`）讓 Match 每 Tick 把 animation 放進 snapshot，但 host 逐出玩家的路徑只刪 snapshot 的 players 與 combat、沒刪 animation（`MatchRuntimeHost.cpp:759-760`，修正前）。Gateway 檢查 `len(in.Animation) != len(in.Players)`（`adapter.go:132`）會回錯 → `runtimeFailed`，房間裡所有人斷線。A2 加的 `RequireValidAnimation` 只在 `PvpMatch::Snapshot()` 內，蓋不到 host 之後的刪除。

## 現象的重現（§14.3 第 1 步）

- 本工作樹（`cf3118f4`，修正前）build 的 digest runner 對 `host-poor-connection/corridor` 跑 `--dump`：最後一筆 record #2399（tick 2400）是 `players=1`、`combats=1`、`animations=2`，被逐出的玩家 2 的 animation（`animPlayer=2 animLife=8 gaitPhaseQ16=4294965931 … jumpPhase=3 jumpPhaseTick=2238`）還在；同一筆的 `evicted=2 evictionReason=0 referenceAgeMillis=249`。
- 證據：主 checkout `build/evidence/v8-product-evict-animation-orphan/before_dump.txt`（2400 筆）。
- 真正 Gateway 上的 `runtimeFailed` 沒有重現（要起 Gateway＋Match＋兩個 Client 並讓一人連線品質連續三個窗口失敗；逐行核對的程式路徑就是上面的三處）。

## 守衛與修正（§14.3 第 2、3 步）

- G3 `PublicationComponentsAgree`（新檔 `tests/object_fps_pvp/EntityModelTests.cpp`，CTest 標籤 cpu）：兩人加入，玩家 2 回報的 snapshot 比實際舊 15 Tick（250 ms，超過 160 ms 的上限），每份 `TakeSnapshot()` 都斷言 combat 與 animation 的 id 序列等於 players 的；逐出一定要發生，否則測試不算通過。修正前 **Red**：4806 個斷言只有 tick 2400 的 animation 對 players 這一個失敗（`g3_red.txt`）。
- 修正（commit `9ec4442e`）：
  - `EraseEntity(WorldSnapshot&, PlayerId)`：一次從 players、combat、animation 刪掉那一列，Assert 三者刪的筆數相同且 ≤1（entity-model E-7 的唯一入口）。
  - `RequireValidSnapshot(const WorldSnapshot&)`：combat 的筆數與 id 順序等於 players 的，再呼叫既有的 `RequireValidAnimation`（animation 只檢查「同一組 id、各恰一次」，允許任意順序；順序由 G3 保證）。host 在 `Step`（由 `Advance` 呼叫）的 `publication = std::move(state)` 之前呼叫；`snapshot_` 只從 `publication` 賦值，沒有其他發布路徑。
  - host 逐出改呼叫 `EraseEntity(state, playerId)`。
- 修正後 **Green**：G3 4806／4806；產品單元測試 397／397 通過（`g3_green.txt`、`tests_all.txt`）。
- 突變（`mutations.json` batch `v8-product-evict-animation-orphan`，只跑本批）：`erase-skips-animation`（EraseEntity 不刪 animation）、`host-erases-two`（host 回到自己刪兩個）。期待 2／2 被 G3 殺；結果見 dev_log。

## 事前宣告：對權威 digest 的影響

結果出來前的宣告，核准後不改。

1. 42 個情境中**只有 `host-poor-connection/corridor` 一行 golden 改變**；其他 36 個 golden 與 5 個非 golden 情境的 digest 不變（同機兩樹比對：base＝`cf3118f4`、branch＝本批）。
2. 該情境的 record 數不變（2400＝2400）；兩樹 `--dump` 逐筆比對只有 record #2399 不同，第一個不同的欄位是 `animations`（2→1），其後只少了玩家 2 的那組 animation 欄位。
3. `DigestVersion` 仍是 4；`DigestFields` 不變；dump 的標籤不變。
4. `git diff cf3118f4 -- apps/object_fps_pvp/protocol apps/object_fps_pvp/gateway engine services` 為空（不改 wire、不改 Gateway、不改 Engine）；Client 專用原始碼不變（`match_domain` 是共用庫，多了兩個 Client 不呼叫的函式）。
5. 上面 1、2 若不成立，停下照 §14 回報，不改判定。

## §14.4 的判定變更：golden 的重生有「緩める面」

受け入れ判定＝`tests/object_fps_pvp/fixtures/authority_golden.txt` 的 `host-poor-connection/corridor` 行（檔頭寫「Regenerate only in the one batch allowed to change authority results」）。用平易的話：

1. **這個判定守的性質**：`host-poor-connection/corridor` 情境的 2400 筆 host record（每筆含 snapshot、control、eviction、action results；最後一筆含逐出）編碼後，在四個平台都逐位元等於 golden。它守的是「權威結果不會悄悄改變，而且跨平台一致」（golden 檔頭第 1～3 行）。
2. **緩める面（以前判失敗、變更後判通過的情況）**：修正後的 host 在 tick 2400（record #2399）的 snapshot 少了被逐出玩家 2 的 animation；舊 golden 判它失敗，新 golden 判它通過。只有這一筆：`animations` 2→1 改值，並刪除玩家 2 的 8 個 animation 欄位（124 bytes）；其他 2399 筆與其他 41 個情境不變。
3. **締める面（以前判通過、變更後判失敗的情況）**：修正前的行為（逐出後 animation 留著）在新 golden 下失敗。另外 G3 讓它的情境（兩人加入、tick 2400 逐出一人）裡「任何元件的 id 序列和 players 不一致」在 cpu 測試就失敗，不必等 golden；各情境的全面覆蓋靠 `RequireValidSnapshot`，每次發布都執行。
4. **緩める面涉及的性質是否仍被其他判定守住**：是。「發布的 snapshot 各元件的 id 一致」由 G3（每份 snapshot 逐一斷言，含 animation 的順序）與 `RequireValidSnapshot`（發布前的 GYO_ASSERT；GYO_ASSERT 在所有建置設定都評估，含 CTest 的 RelWithDebInfo 與正式版，`engine/base/Assert.hpp:21`）守住。副作用要寫明：正式版若再違反，失敗方式從「Gateway runtimeFailed、全員斷線」變成「Match 程序 abort」，和 `Snapshot()` 內既有的 `RequireValidAnimation` 同一方針。「權威結果不悄悄改變」由其他 41 個情境的 0 差分與本行重生後的新值守住。
5. **根據與證據**：§14.3 的 Red→Green（`g3_red.txt`、`g3_green.txt`）；修正前後的 `--dump` 差異（`before_dump.txt`、`after_dump.txt`、`dump_diff.txt`：2400 筆只差 1 筆，少的欄位恰是玩家 2 的 animation）；兩樹比對（`compare_scale1.txt`：base `cf3118f4` 對本批，scale 1，「42 scenarios, 1 differing；host-poor-connection/corridor: first differing record 2399, field animations (2 -> 1)」，宣告的 1、2 成立）；突變 2／2（`mutations.json`、`mutations_run3.txt`）。證據都在主 checkout `build/evidence/v8-product-evict-animation-orphan/`（Intel Mac，git 管理外）。

新值（修正後的 runner，`after_run.json`）：`host-poor-connection/corridor 52a34210131788269c53b3eb8374d0b0ccf15458f96e951bb21a11ba74495209`（舊值 `68c48ca5…24a6014e`）。四平台一致要在 L1 看（這個情境是 golden，零角度子集）。

## 宣告的結果（結果出來後填，判定不改）

- 1、2：成立。兩樹比對 42 個情境 1 個不同，第一個不同的 record 是 2399、欄位 `animations` 2→1；base 與 branch 每個情境的 records 數相同；5 個非 golden 情境也相同（`compare_scale1.txt`、`compare_scale1/comparison.json`）。修正前後的 `--dump` 2400 筆只差 1 筆：`animations=2` 改成 `animations=1`，並刪除玩家 2 的 8 個 animation 欄位 `animPlayer=2 animLife=8 gaitPhaseQ16=4294965931 moveWeight=1000 jogWeight=400 jumpPhase=3 jumpPhaseTick=2238 sprintWeight=0`（`dump_diff.txt`）。範圍的限界：只比了 scale 1；逐出發生在最後一筆，逐出之後的 Tick 沒有被比到。
- 3：成立。`DigestVersion` 4、`DigestFields` 與標籤未改（`git diff cf3118f4 -- tests/object_fps_pvp/AuthorityDigest.cpp` 為空）。
- 4：成立。`git diff cf3118f4 --stat -- apps/object_fps_pvp/protocol apps/object_fps_pvp/gateway engine services` 為空；Client 端原始碼不變。
- 突變：2／2 killed（`mutations_run3.txt`、`mutations.json` 報告）。被殺的機制（推論，`run_mutations.py` 對 killed 不保存 detail）：兩個突變都讓 animation 留在 snapshot，在 G3 執行中被 `RequireValidSnapshot` 的 GYO_ASSERT 中止，doctest 的崩潰報告印出測試名，expect 以 `PublicationComponentsAgree` 比對到；G3 的 CHECK 本身的檢出力以修正前的 Red 證明。前兩次執行的基準 ctest 逾時（`object_fps_pvp.cpu` 的 TIMEOUT 60 秒），突變沒有執行；原因未切分（推測：環境，建置剛結束），同款命令手動跑 24～27 秒通過但計時未保存；兩次失敗的紀錄保留（`mutations_run.txt`、`mutations_run2.txt`）。

## 核准後要做的

1. 重生 golden 的那一行（`--run` 的值寫進 fixture），照檔頭的慣例：舊檔另存 checkpoint（`authority_golden.<批次>-checkpoint.txt`）並存進證據目錄，檔頭補一行說明本批改了什麼；確認 `authority_digest` CTest 通過。
2. dev_log、README 一覽、HANDOFF 同一變更提交。
3. 進 `v8-product-match-entity-split`，它的 base＝本批的最後一個 commit（entity-model §9.0）。
