# v8-product-evict-animation-orphan 的事前宣告與 §14.4 的判定變更

更新：2026-11-03。Owner：`object_fps_pvp`。**狀態：等核准。** 契約：[entity-model](../../entity-model.zh-Hant.md) E-7、§9.0（決定 3 (a)）。檔位 high、上段（回歸修正）；實作者主會話 Fable 5.1、§14.4 檢證者 Opus 5.5。估 3～5 h。

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
  - `RequireValidSnapshot(const WorldSnapshot&)`：combat 的 id 序列等於 players 的，再呼叫既有的 `RequireValidAnimation`。host 在 `publication = std::move(state)` 之前呼叫（`MatchRuntimeHost::Advance`）。
  - host 逐出改呼叫 `EraseEntity(state, playerId)`。
- 修正後 **Green**：G3 4806／4806；產品單元測試 397／397 通過（`g3_green.txt`、`tests_all.txt`）。
- 突變（`mutations.json` batch `v8-product-evict-animation-orphan`，只跑本批）：`erase-skips-animation`（EraseEntity 不刪 animation）、`host-erases-two`（host 回到自己刪兩個）。期待 2／2 被 G3 殺；結果見 dev_log。

## 事前宣告：對權威 digest 的影響

結果出來前的宣告，核准後不改。

1. 42 個情境中**只有 `host-poor-connection/corridor` 一行 golden 改變**；其他 36 個 golden 與 5 個非 golden 情境的 digest 不變（同機兩樹比對：base＝`cf3118f4`、branch＝本批）。
2. 該情境的 record 數不變（2400＝2400）；兩樹 `--dump` 逐筆比對只有 record #2399 不同，第一個不同的欄位是 `animations`（2→1），其後只少了玩家 2 的那組 animation 欄位。
3. `DigestVersion` 仍是 4；`DigestFields` 不變；dump 的標籤不變。
4. `git diff cf3118f4 -- apps/object_fps_pvp/protocol apps/object_fps_pvp/gateway engine services` 為空（不改 wire、不改 Gateway、不改 Engine）；Client 端原始碟不變。
5. 上面 1、2 若不成立，停下照 §14 回報，不改判定。

## §14.4 的判定變更：golden 的重生有「緩める面」

受け入れ判定＝`tests/object_fps_pvp/fixtures/authority_golden.txt` 的 `host-poor-connection/corridor` 行（檔頭寫「Regenerate only in the one batch allowed to change authority results」）。用平易的話：

1. **這個判定守的性質**：host 在 host-poor-connection 情境裡逐出一位玩家之後，發布的 snapshot 和之前每一個 Tick 的內容都和「黃金值」逐位元相同。它守的是「權威結果不會悄悄改變」。
2. **緩める面（以前判失敗、變更後判通過的情況）**：修正後的 host 在 tick 2400 的 snapshot 少了被逐出玩家的 animation；舊 golden 判它失敗，新 golden 判它通過。只有這一筆、這一個欄位組；其他 2399 筆不變。
3. **締める面（以前判通過、變更後判失敗的情況）**：修正前的行為（逐出後 animation 留著）在新 golden 下失敗。另外 G3 讓「任何元件的 id 序列和 players 不一致」在 cpu 測試就失敗，不必等 golden。
4. **緩める面涉及的性質是否仍被其他判定守住**：是。「發布的 snapshot 各元件的 id 一致」由 G3（每份 snapshot 逐一斷言）與 `RequireValidSnapshot`（發布前的 GYO_ASSERT，CTest 的 Debug 建置會觸發）守住；「權威結果不悄悄改變」由其他 41 個情境的 0 差分與本行重生後的新值守住。
5. **根據與證據**：§14.3 的 Red→Green（`g3_red.txt`、`g3_green.txt`）；修正前後的 `--dump` 差異（`before_dump.txt`、`after_dump.txt`、`dump_diff.txt`：2400 筆只差 1 筆，少的欄位恰是玩家 2 的 animation）；兩樹比對（`compare_scale1.txt`：base `cf3118f4` 對本批，scale 1，「42 scenarios, 1 differing；host-poor-connection/corridor: first differing record 2399, field animations (2 -> 1)」，宣告的 1、2 成立）；突變 2／2（`mutations.json`、`mutations_run3.txt`）。證據都在主 checkout `build/evidence/v8-product-evict-animation-orphan/`（Intel Mac，git 管理外）。

新值（修正後的 runner，`after_run.json`）：`host-poor-connection/corridor 52a34210131788269c53b3eb8374d0b0ccf15458f96e951bb21a11ba74495209`（舊值 `68c48ca5…24a6014e`）。四平台一致要在 L1 看（這個情境是 golden，零角度子集）。

## 宣告的結果（結果出來後填，判定不改）

- 1、2：成立。兩樹比對 42 個情境 1 個不同，第一個不同的 record 是 2399、欄位 `animations` 2→1（`compare_scale1.txt`）。修正前後的 `--dump` 2400 筆只差 1 筆，少的 7 個 token 恰是 `animations=2` 與玩家 2 的 6 個 animation 欄位（`dump_diff.txt`）。
- 3：成立。`DigestVersion` 4、`DigestFields` 與標籤未改（`git diff cf3118f4 -- tests/object_fps_pvp/AuthorityDigest.cpp` 為空）。
- 4：成立。`git diff cf3118f4 --stat -- apps/object_fps_pvp/protocol apps/object_fps_pvp/gateway engine services` 為空；Client 端原始碼不變。
- 突變：2／2 killed（`mutations_run3.txt`、`mutations.json` 報告）：`erase-skips-animation` 與 `host-erases-two` 都由 G3 以 `PublicationComponentsAgree … is NOT correct` 抓到。前兩次執行的基準 ctest 逾時（60 秒上限）不是突變結果：同款命令手動跑 24～27 秒通過，推測是建置剛結束的 CPU 降頻；兩次失敗的紀錄保留（`mutations_run.txt`、`mutations_run2.txt`）。

## 核准後要做的

1. 重生 golden 的那一行（`--run` 的值直接寫進 fixture，或照 CMakeLists 的重生步驟），確認 `authority_digest` CTest 通過。
2. dev_log、README 一覽、HANDOFF 同一變更提交。
3. 進 `v8-product-match-entity-split`，它的 base＝本批的最後一個 commit（entity-model §9.0）。
