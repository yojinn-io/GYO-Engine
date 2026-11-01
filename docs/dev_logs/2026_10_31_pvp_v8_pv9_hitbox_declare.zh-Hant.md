# PvP v8：pv9-hitbox-declare（pv9 的 wire 需求一次推完）

日期：2026-10-31。分支 `claude/pvp-v8-after-pv8`。檔位 ultracode（三案 Opus 5.5、審查 Opus 5.5、對抗 Sonnet 5.5）。估時 7～11 h（原 5～8 h，因 `pv9-scope-hitbox-and-rewind` 加入 rewind-line），workflow 約 19 分鐘＋整理約 1 h。

**狀態：完成。** [protocol-v9](../object_fps_pvp/protocol-v9.zh-Hant.md) 定稿：第 9 節 6 項使用者照建議 (a) 核准（`pv9-final-approve`）。衝刺選 (a) 後，`PlayerAnimation` 是 8 欄，4 人 Snapshot 型別最大推論為 1022 B。

## 為什麼這樣做

pv8 設計時假設 pv7 的 `HitPart`（BODY、HEAD）夠用，把核對延到 hitbox 批（protocol-v8「不在 pv8」第 6 項）；合併後 1.5 小時才發現要加腿，只好升 pv9。這次照使用者的決定（`pv9-scope-hitbox-and-rewind`），把 hitbox-line 與 rewind-line 的 wire 需求一次推完，並要求每一項附出處（程式位置或 hitbox-plan §2 的成熟方案與 ✓／△／✗）。

## 做法

workflow `wf_12b686cf-ba0`（5 個 Agent，全部完成）：

1. 三個案，各從一個角度獨立推導：成熟方案優先、程式路徑優先、風險優先。
2. 審查者合併成一份草稿，衝突依重讀的程式碼裁定（9 項，例如動畫狀態放在 `WorldSnapshot` 的獨立訊息而不是 `PlayerState`、步伐相位不取模的 q16、權重用千分比）。
3. 對抗檢查（Sonnet 5.5）：開了約 62 條引用，5 條不符；提出 must 2、should 6、note 7。

## 對抗檢查的處理

| 嚴重度 | 指出 | 處理 |
|---|---|---|
| must | `jump_phase_tick` 的語意和現行狀態機不一致（AIRBORNE 的零點是起跳＋6 Tick，不是偵測的 Tick） | 接受：protocol-v9 §4.4 定為「clip 時間的零點」，逐階段寫明；載入時檢查跨度是 Tick 的整數倍 |
| must | 新檢查失敗時整個 Gateway 失效（不是丟一份 snapshot） | 接受，自己核對 `server.go:704-707` 屬實；寫進 §4.5，列為決定 (F) |
| should | 漏了和 `grounded` 的交叉檢查 | 接受：§4.5 |
| should | Client 端 `ClientConnection.cpp` 的解碼驗證沒列 | 接受：§2、§4.5 的改動清單兩端對稱 |
| should | 漏收時回溯姿勢與畫面姿勢不同，草稿沒寫成限制 | 接受：§5「接受的限制」，列為決定 (D) |
| should | 射手的 `lastDamage` 也要換算單位 | 接受：§3 |
| should | gait／jump 欄位掛在 Valorant ✓ 下超出標記；「CS 單一插值時刻」找不到出處 | 接受：§4.1 改成「成熟方案只證實存動畫狀態，欄位內容是我們的推論」；刪掉那句 |
| should | 逐目標夾取的上界與區段起點，結論「rewind 不需要新欄位」還不成立 | 接受：§5 改成「接受的限制」下才成立，列為決定 (D) |
| note | 號碼不同的先例引錯（MatchStatus 只是預留） | 接受：改引 `gateway_hold_us` |
| note | `animation_digest` 的出處引錯 | 接受：改引 B3 的 H-load |
| note | 本機玩家身體的引用錯 | 接受：§8 改寫 |
| note | Welcome 變長會縮短 arena id 上限 | 接受：§3、§6 |
| note | hitbox-plan 的 `ShotQuery.cpp` 行號過時（44-46 → 54-56） | protocol-v9 用新行號；計畫文件不改（當時的紀錄） |
| note | 新 map 屬 `classPerCall` | 接受：§4.5 |
| note | `jog_weight` 重生時要重算、`backward` 用到 libm | 接受：§4.4 |

主會話另外查到的事實：UAL1 有 `Sprint_Loop`，而 ② 衝刺是已排定的 v8 功能，所以把衝刺動畫的 wire 需求列為決定 (C)（三案都只提到「要決定」）。

成熟方案的查證標記：成熟方案優先的案稱重查了 Valve 官方 source-sdk-2013，CS 兩列可從 △ 升為 ✓；審查者與主會話都沒有獨立核對，hitbox-plan §2 維持 △。

## 結果

- 確定要上 wire：`HIT_PART_LEG`、千分之一點（5 個欄位改語意）、`WorldSnapshot.animation`（7 欄的新訊息）、版本值 9。
- 回溯：在 §5 的限制下沿用既有欄位。
- 大小（推論）：4 人 Snapshot 型別最大 822→998 B（衝刺選 (a) 時 1022 B），≤1200 B。
- 等你決定：(A) 動畫內容的 digest、(B) 上半身動作進不進判定、(C) 衝刺動畫、(D) 回溯的漏收限制、(E) 一次合併、(F) 檢查失敗的處理。

## 範圍的限界

- 大小是手算的推論，實測在 leg-wire 與 anim-authority 批重釘。
- 引用核對由對抗者做了約 62 條，不是全部；行號以 `57a3aace` 為準。
