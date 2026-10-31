# v8-product-hit-rules-leg-wire 的事前宣告

更新：2026-10-31。Owner：`object_fps_pvp`。**狀態：草稿，等你核准。** 契約：[protocol-v9](../../protocol-v9.zh-Hant.md) §2（定稿，`pv9-final-approve`）。計畫：[hitbox-plan](hitbox-plan.md) 第 8 節。檔位 high｜Opus 5.5（繼承主會話），估時 3～5 h（超過 7.5 h 停下回報）。

## 範圍

這批之後，wire 上與兩端的檢查都認得「腿」，但 **Match 仍然只產生 Body**（部位判定在 `v8-product-hitbox-shot`）。所以權威結果不變，這批只改「誰接受什麼值」。

1. **proto**：`client_v9.proto`、`runtime_v9.proto` 的 `enum HitPart` 加 `HIT_PART_LEG = 3`；重產 Go 綁定（protoc 36.2、protoc-gen-go v1.36.11，和 pv9-rename 相同）。C++ 在建置時產生。
2. **C++ 列舉與檢查**（`Combat.hpp`）：`HitPart` 加 `Leg = 3`；`ValidDecisionHitPart` 與 `ValidDamageRecord` 的「玩家部位」改成 Body、Head、Leg 三個；`ShotDamage` 對 Leg 用 `legPermille`（Head→head，Leg→leg，其他→body）。
3. **對照**：`IpcHost.cpp:177-178` 的 `static_assert` 加 `Leg == HIT_PART_LEG`，`HitPart_ARRAYSIZE` 由 3 改 4。
4. **Gateway**（`adapter.go:169-171`）：`playerPart` 加 `HIT_PART_LEG`；註解改成 pv9 §2。
5. **Client 解碼**（`ClientConnection.cpp:446`、`:644`）：照舊呼叫 `ValidDecisionHitPart`／`ValidDamageRecord`，跟著第 2 項自動接受腿；程式本身不改。
6. **文件**：`protocol/README.md` 的 `HitPart` 段落；`Combat.hpp:46`、`adapter.go` 的註解。

不做：Match 產生 Leg、BODY 的範圍改成「軀幹含骨盆」、射線穿過手臂（都在 `v8-product-hitbox-shot`）；依部位的受擊呈現（`v8-product-hit-presentation`）。

## 受入判定（看結果之前宣告）

### L1：權威結果不變（兩樹比對）

- base＝`e54a74b9`（本批之前）、branch＝本批，同一台 Intel Mac，`compare_authority_trees.py`（一般模式，不加 `--scaled`），scale 1、3 各 1 次。
- 判定：**42 個情境全部 0 差分**；`authority_golden.txt` 一個位元組都不改，`object_fps_pvp.authority_digest` 通過。
- 根據：Match 不產生 Leg，digest 也沒有新欄位，所以位元組必須完全相同。有任何差分就停下回報。

### L2：白名單（兩端對稱）

C++（`CombatTests.cpp`）與 Go（`hitbox_test.go`、`hit_record_test.go`）各自檢查同一張表：

| 情況 | Body | Head | Leg | Unspecified | 4 | 99 |
|---|---|---|---|---|---|---|
| 命中玩家的 decision | 接受 | 接受 | **接受** | 拒絕 | 拒絕 | 拒絕 |
| 打到世界、打空、裝填的 decision | 拒絕 | 拒絕 | **拒絕** | 接受 | 拒絕 | 拒絕 |
| 受擊紀錄（四欄都有值） | 接受 | 接受 | **接受** | 拒絕 | 拒絕 | 拒絕 |

- Go 的 `EqualDecision` 能分辨 Leg 與 Body。
- `ShotDamage(PvpCombatRules, HitPart::Leg, 100000)`＝18750；hp 不足時取 hp。

### L3：往返（每一段都有守衛）

| 段 | 守衛 |
|---|---|
| C++ `HitPart` → runtime proto（`IpcHost` 用 `static_cast`） | 編譯期 `static_assert`：四個值逐一對照，`HitPart_ARRAYSIZE == 4` |
| runtime proto → client proto（Gateway 照抄） | Go 測試：Leg 的 decision 與受擊紀錄經 `ResultsForClient`／`SnapshotForClient` 之後，值仍是 `HIT_PART_LEG` |
| client proto → Client 的 `HitPart`（`ClientConnection`） | worker probe 加一個 Leg 的情況：mock Gateway 送出 `HIT_PART_LEG` 的 decision 與受擊紀錄，Client 讀到 `HitPart::Leg`；原本 Body、Head 的情況保留 |

### L4：大小

- enum 值 3 的 varint 仍是 1 B，所以任何大小的釘都不變（`network_main.cpp:159` 706 B、`worker_main.cpp` 748 B、Go 的 capacity 測試）。不重釘；大小變了就停下回報。

### L5：突變

只登記和本批有關的，只在預期失敗時才算 killed：
- Gateway 的 `playerPart` 不認 Leg。
- C++ 的 `ValidDecisionHitPart` 不認 Leg。
- C++ 的 `ValidDamageRecord` 不認 Leg。
- `ShotDamage` 對 Leg 用 body 的倍率。
- C++ 的 `Leg` 改成 4（`static_assert` 擋下，由 build 檢查抓）。
- Client 把 Leg 讀成 Body（worker probe）。

D184 相關的既有突變（find 落在改動的行上，或殺它的測試被改）全部要 killed。

### L6：既有測試

`test` preset 的全部 CTest 與 `go test ./...` 通過。四平台 L1 照 `pv9-single-merge`，在整條 hitbox-line 的 PR 上跑。

## 會改判定的既有測試（AGENTS §14.4）

| # | 測試 | 守的性質 | 改法 | 寬鬆面 | 收緊面 |
|---|---|---|---|---|---|
| V1 | `CombatTests.cpp:1006`：玩家命中的無效部位 `{0, 3, -1}` | 命中玩家只能是已知的部位 | 3 移到有效那一組，無效改成 `{0, 4, -1}` | **有**：部位 3 從拒絕變接受 | 4 改為明確檢查 |
| V2 | `CombatTests.cpp:809`：受擊紀錄的 `static_cast<HitPart>(3)` 是錯的 | 受擊紀錄只能是已知的部位 | 改用 4；另加「Leg 接受」 | **有**：同上 | 4 改為明確檢查 |
| V3 | `hitbox_test.go` `TestResultsCheckTheHitPart`、`hit_record_test.go` | Gateway 的部位白名單 | 加 Leg 的接受與 miss 拒絕、加 4 的拒絕；原本的 99 保留 | **有**：同上（原本沒有測 3，但 3 會被拒絕） | 4 改為明確檢查 |

V1～V3 的寬鬆面，照 §14.4 的 1～5 揭露：

1. **守的性質**：wire 上只會出現契約定義的部位；未知的部位會讓整批 results 或整個 snapshot 失效。
2. **寬鬆面**：值 3 由「未知、拒絕」變成「腿、接受」。只有 3 這一個值；在打到世界、打空、裝填的 decision 上，3 仍然被拒絕。
3. **收緊面**：4 由「沒有測」變成「明確拒絕」。
4. **由誰守住**：性質沒有變，只是已知部位多了一個，由同一組測試（C++ 的 `CombatTests` 兩個 TEST_CASE、Go 的兩個測試）以 4 與 99 繼續守住。
5. **根據**：protocol-v9 §2（`pv9-final-approve`）、hitbox-plan §1（`v8-product-hit-rules-three-parts`）。

**§14.4 要求的審查順序**：有寬鬆面的判定變更，要先做突變檢查，並由另一位審查者核對 1～5，之後才能請你核准。這批的做法：
- 這份宣告的核准，涵蓋寬鬆面「只有值 3」這個範圍。
- 實作之後、commit 之前，跑兩項檢查：
  - V1～V3 的突變：未知部位的檢查拿掉之後，4 與 99 的測試要失敗。
  - 審查：交給一個 Sonnet 5.5 的子 agent。它只讀 diff 與測試，用實際的測試輸入核對上面的 1～5。
- 兩項的結果寫進 dev_log。有任何一項不符，就不 commit，停下回報。

## 等你決定

### `v8-product-leg-wire-worker-probe`：Client 端的往返要怎麼測

`ClientConnection` 的解碼沒有可以單獨呼叫的入口，只能從 worker probe（mock Gateway → 真的 Client）走。

- (a) 在 worker probe 加一個 Leg 的情況，原本 Body、Head 的情況不動（建議）。Client 端三個部位都會實際解碼過。
- (b) 不測 Client 端，靠「Client 呼叫的就是 C++ 單元測試測過的那兩個檢查函式」。成本較低，但 Client 解碼後 `static_cast<HitPart>` 的那一步沒有直接的證據。

## 範圍的限界

- Match 不產生 Leg，所以「Match 判出腿 → Client 看到腿」的完整路徑，要到 `v8-product-hitbox-shot` 才有證據。這批只證明每一段都接受、也照抄腿。
- L1 只在這台 Intel Mac 上比。
