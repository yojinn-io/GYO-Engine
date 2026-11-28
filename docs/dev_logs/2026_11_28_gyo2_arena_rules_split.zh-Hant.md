# 2026-11-28 arena 規則拆出（`v8-arch-gyo2-arena-rules-split`）

Owner：「任務卡」arena 規則拆出。分支 `claude/gyo2-arena-rules-split`（PR #379，開向 `gyo2`）。作者 Opus 5.5／high。宣告：[arena-rules-split-declaration.zh-Hant.md](../architecture/target/v2/arena-rules-split-declaration.zh-Hant.md)，使用者的決定在第 9.1 節，實作結果在第 11 節。

## 前提（使用者的決定，經主會話轉達）

1. 原則：「遊戲規則也不應該綁定地圖。地圖可以適應規則而不應該決定規則。」（HANDOFF `v8-arch-gyo2-mode-config-u3`）
2. 「照推薦，A、B、C 都核准，開工」：A (a) 拿掉 arena 的 `sets`（推翻 #367 的那部分）、B (a) 速度與跳高進 `movement` 表（和別家不同）、C (b) 蹲下的身體進角色的 `body`；卡片自判的項目沒有推翻。

## 做了什麼

- **arena 檔 version 5**（`sim/Arena.hpp`）：只留 `id`、`width`／`depth`、`map`、`floor`、`kill_height`、`spawns`，加選填 `scene`（`data::ContentPath`，和模式配置檔 `rules` 同一個檢查，從 `Modes.cpp` 搬到 `data/JsonFields`）。拿掉規則類、身體、`cell_size`（從來沒被讀）。`BelowKillHeight` 搬到 `Arena.hpp`；`SpawnInBounds(arena, spawn, radius)` 給讀檔（半徑 0）與內容讀取（角色的半徑）共用。
- **`movement` 表 version 3**（`sim/MovementRules.hpp`）：`StanceRules` 改名 `MovementRules`；加 `movement_speed`、`jump_height`、`gravity`（選填，預設和舊 arena 相同）；蹲下的身高與眼高拿掉（C (b)）。倍率的檢查（蹲下 (0, 1]、衝刺 (0, 3]）從跨檔的 `StanceFits` 移到讀檔；`StanceFits` 拿掉。
- **`damage` 表 version 1**（新，`sim/DamageRules.hpp`）：`fall_damage`；`LandingDamage(movement, damage, speed)` 用 `movement` 表的重力。模式配置的 `damage` 是選填的規則種類。
- **角色的身體**（`sim/BodySize.hpp`，`character.json` version 2 的 `body`）：站姿與蹲姿的身高、半徑、眼高；`CharacterRig::bodySize`；`LoadCharacterRig` 不再收身高參數，開始檢查 `character.json` 的 version。
- **內容讀取**（`LoadGameContent`）：拿掉「所有 arena 同一身高」；讀完角色後逐張 arena 檢查出生點留得下角色的半徑。
- **比對值**：`rulesDigest` 收 `MovementRules` 全部欄位與 `DamageRules`；`characterDigest` 收 `BodySize`；`arenaDigest` 改成 arena 自己的值（`map`、邊界、`floor`、`kill_height`、spawns；不含 `id` 與 `scene`），`MatchInfo` 的 wire 與 Codec 版本不變。`ArenaData` 的原因字串跟著改。
- **A：拿掉 arena 的 `sets`**：`GameplaySet::readArena`、`ReadArenaSets`、`ReadSetBlocks` 的 `arena` 參數、`GameContent::arenaSets`、`Prepare` 放 arena 區塊的那段。
- **assets**：六張 arena 改 v5（值逐欄比對過：`id`、邊界、`map`、`floor`、`kill_height`、spawns 不變）；`rules/movement_rules.json` v3 明寫速度 3、跳高 0.6、重力 18；新增 `rules/damage_rules.json`（`{4.0, 15}`）與 catalog 的一列；`core_pvp.json` 加 `damage`；`character.json` v2 加 `body`。
- 主 checkout 的 S5 合成內容 `build/evidence/gyo2-big-map/s5/content.py`（不在 git）改寫 v5 的 arena；舊版留成 `content.py.v4.bak`。**沒有跑 S5**（讀圖與建牆的程式沒改）。

## 檢驗

- 既有測試**不改期望值**；改的只有「從哪裡讀這個值」（`RulesTests`、`WorldTests` 的邊界與墜落、`ClientTests` 的 `hold_jump`）與測試資料的格式（arena v5、`movement` v3）。拿掉的測試只有被測的程式已不存在的（arena 的組區塊：`[pvp-set-on-arena]`、`[pvp-set-arena-digest]` 那一案、`ReadArenaSets` 的三個斷言）。兩個測試名把「arena's」改成「rules'」（速度、跳高不再是 arena 的）。
- 新測試：arena v5 的讀取與每種錯誤、`scene` 的路徑；`movement` v3、`damage` v1、`body` 的讀取與錯誤；出貨的三張表與身體＝舊 arena 的值；World 照模式配置的表與角色的身體跑（速度、跳高＋重力、半徑、沒有墜落傷害）；`rulesDigest`／`characterDigest`／`arenaDigest` 逐欄跟著變；arena 只差一個出生點時是 `ArenaData`；出生點離邊界比角色的半徑近時內容讀不進來。
- 本機（macOS x86_64，preset `test`）：`object_fps_pvp.*` 16 項全過（cpu、network、gpu、go，含 `core_script`、`gateway_client`）；`asset.*`、`world_asset.*` 12 項全過。
- **A/B（macOS）**：base `d2329a80`（這條線分出的 `gyo2`）與 head，`ab.py`（主 checkout `build/evidence/gyo2-arena-rules-split/`）：**權威 digest 1257 行逐位元組相同；report（net 登記、協議 hash、決策資料集）沒有差異**。
- **Linux A/B**（Ryzen 7 6800H，g++ 13.3）：權威 digest 1257 行與 report 66 行整個檔逐位元組相同（`linux.sh`，重用 #367 在主機上的建置樹）。
- 突變（D184：新的、find 在改動範圍的、殺它的測試被改到的）：第 1 次 58 個（新增 24、改寫 find 6、拿掉 2；`pvp-mode-unknown-rule-kind` 一次 `wrong_failure`，`expect` 改成實際的失敗訊息）；設計審查處置後加 4 個，62 個重跑 **62/62 killed**。

## 設計審查與純粹的 Code Review

- 設計審查（Opus／high，唯讀）：沒有 blocker；兩處和宣告不同的字串（出生點半徑的錯誤、`ArenaData` 的原因）、S1（角色檔 → rig 的身體沒有測試）、S2（arena 的錯誤不是每種都有一案）、五個 nit。處置在宣告 11.4：D1、D2、S1、S2、N1、N3、N5 採用，N2、N4 不採（理由在表）。處置後 `object_fps_pvp.*` 16 項全過，A/B（macOS）仍逐位元組相同。
- 純粹的 Code Review（Fable／high）：見宣告 11.4。
