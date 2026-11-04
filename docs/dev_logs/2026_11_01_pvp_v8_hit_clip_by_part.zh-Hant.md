# PvP v8：v8-product-hit-clip-by-part（P1：受擊動作依部位）

日期：2026-11-01。分支 `claude/pvp-v8-after-pv8`。上段｜實装者 Opus 5.5／high；審查者 Opus 5.5／xhigh（`xhigh-reviewer`）。估時 4～6 h，實際約 2 h。

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-hit-presentation-declaration.md)的 P1：`v8-product-hit-clip-by-part` (a)、`v8-product-hit-leg-clip` (a)。

## 做了什麼

- `animations.animset.json` 加 `hit_head`（`Armature|Hit_Head`）。`presentation.json` 的 `hit_seconds` 兩個受擊 clip 共用。
- `PlayerPresentationFrame.damagePart`：最近一次受傷的部位，由 `PlayerFrameFromAuthority` 從 `CombatState.lastHitPart` 取。
- `PlayerActionPose.headHit`：上半身是受擊、而且部位是頭時為 true。時間照所選 clip 的長度縮放。
- 組合身體時，頭部受擊取 `hit_head`，軀幹、腿和部位未定時取 `hit`（`Hit_Chest`）。Match 的姿勢模型呼叫同一組函式，判定的形狀跟著同一個 clip（「畫面＝判定」）。
- `AnimationContentDigest` 的固定 clip 列表最後加上 `hit_head`。
- **不改 wire、不升版本**：`last_hit_part` 本來就在 snapshot 裡。

## 判定

- **G1：通過。** 「plays Hit_Head for a head hit and Hit_Chest otherwise」：
  - 頭 → `headHit`、時間照 `Hit_Head` 的長度；軀幹、腿、未定 → `Hit_Chest`。
  - 受擊時段之外，部位不影響姿勢。
  - 同一時間點，兩個 clip 組出的 `Head` 骨頭位置不同。
- **G2：通過。**
  - 「every kind of input」加上頭部受擊、腿部受擊兩種輸入，姿勢模型和 Client 的姿勢逐位元相同。
  - 新測試「take the head hit clip」：受擊後 5 Tick，頭部受擊的形狀比胸口受擊最多移動 1.85 cm；腿部受擊和胸口受擊逐位元相同。
- **G3：通過。**
  - Client 與 Match 兩條路徑的 digest 相同。本機的新值 **`16fdd42a3aa49d35`**（之前是 `6274bea50a363a99`）。
  - 改 `hit_head` 的一個 key、交換 `hit` 與 `hit_head` 的順序，digest 都會改變。
  - 四平台一致要在下一個 PR 的 L1 看，不同就停下回報。
- **G4：通過。**
  - 兩樹比對（base＝`ed364025`、合成模型）：scale 1、3 各 42 個情境 0 差分。golden 不重產。
  - `hit_part_cross` 通過；Match 的 log 印出 `animation_digest=16fdd42a3aa49d35`，和 Client 路徑的測試值相同。

## 突變

**19／19 killed**（只在預期的訊息失敗時才算 killed）。第一輪 17／17；照審查補了測試與 1 個突變之後，重新計算相關集合再跑，19／19。

- 本批新增 6 個：
  - 部位被忽略；
  - frame 漏掉部位；
  - 組合身體只用胸口受擊；
  - digest 漏掉 `hit_head`；
  - Match 的姿勢模型不帶戰鬥狀態（兩端不一致）；
  - 缺 `hit_head` 時退回胸口受擊（審查者構造）。
- D184 相關的既有突變 13 個：find 落在改動的行，或抓它的測試被改。
- 清單維護：`d2-hit-plays-shot` 的 find 跟著改成新的兩行，抓的東西不變。

## 會改判定的既有測試

宣告的 W1（digest 的清單加 `hit_head`）、W2（every kind of input 加兩種輸入）都只有收緊面；W3（寫死 15 的地方）不存在。照 §14.4 由我改，事後揭露。

## 審查

xhigh 審查（Opus 5.5，唯讀）：**沒有 must-fix**。clip 只由權威值決定，兩端共用同一條函式鏈（frame builder 只有 `PvpApplication.cpp:815` 與 `PlayerHitModel.cpp:30`；內插時 combat 整筆取自同一份 snapshot）。

照做的：
- **should-fix（文件過期）**：下面這些都還寫「15 個 clip」或舊的順序，已更新：
  - protocol-v9 §4.6 加一條補記，寫明 `hit_head` 在最後、共 16 個，並註明部位選 clip 的規則不在 digest 裡；
  - hitbox-data 的宣告加補記；
  - `protocol/README.md`、`PlayerPresentation.hpp`、`PlayerAnimationDigest.cpp`、digest 測試的註解；
  - protocol-v6 §6「Hit_Head 不使用」加上取代的註記。
- **測試缺口**：
  - 「reject missing clips」加上缺 `hit_head` 的案例。審查者構造出的存活突變（缺 `hit_head` 時退回胸口受擊）登記為 `p1-hit-head-fallback`，killed。
  - 腿與軀幹的比較從 2 個座標改成 6 個全比。
  - 重啟的代價加量頭部受擊：頭部受擊重啟最多 0.097 m；軀幹受擊 k Tick 後被頭部受擊取代，最多 0.139 m，和原本胸口受擊的最大值相同。只量、不設門檻。

記錄、不修的：
- `ClientConnection.cpp:653` 先把 `last_hit_part` 窄化成 `uint8_t` 才檢查。wire 上 258 會讀成 Head 而且通過檢查。正常路徑上 Gateway（`adapter.go:172`）會先擋下，不在本批範圍。
- digest 的列表位置沒有固定值的測試：把 `hit_head` 移到別處，三個測試都抓不到。後果是不同 build 被誤擋（fail-closed），不會靜默地不一致。
- G2 的兩個新案例，Client 與 Match 用的是同一個 definition。真正跨 definition 逐位元比對的 H4（`PlayerAnimationEquivalenceTests.cpp`）只有胸口受擊；要擴充 H4 就要改它的姿勢數，在宣告的 W 表之外，所以沒有做，留給使用者決定。
- `AnimBottlenecks`、`HitboxSurvey`、`action_short.hpp` 都沒有看 `headHit`。都是只回報的量測，或是 GUI 擷取，留著；`Hit_Head` 的觀感只靠 L2 試玩。

## 其他測試

- `test` preset 的全部 CTest（`-j4`，master 合併後是 123 個）：123／123 通過，含 worker、`hit_part_cross`。
- 證據：主 checkout 的 `build/evidence/v8-hit-clip-by-part/`（Intel Mac；`compare-scale1`、`compare-scale3`、`mutations*.out`、`ctest-full.log`）。

## 範圍的限界

- 受擊後 5 Tick，頭部與胸口受擊的形狀只差 1.85 cm；重啟的代價見「審查」。頭部受擊看起來對不對，要在 L2 試玩看（`v8-product-hitbox-acceptance`）。
- 腿部受擊仍用胸口受擊；有了腿部 clip 之後，只改內容與 digest。
- 新 digest 的四平台一致要等下一個 PR 的 L1。
