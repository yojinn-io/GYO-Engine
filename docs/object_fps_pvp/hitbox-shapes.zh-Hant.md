# 玩家的命中形狀（Data Contract，版本 1）

Owner：`object_fps_pvp`。檔案：`assets/object_fps_pvp/characters/player/hitbox.json`（asset-layout 的標準，[assets](assets.zh-Hant.md)；2026-11-01 由 `players/` 搬來）（asset id `object_fps_pvp.player.hitbox`，text）。讀者：`ReadPlayerHitbox`（`player_content`，`PlayerHitbox.hpp`）。Match 啟動時載入，不讀貼圖；Client 不讀。來源：[hitbox-plan](plans/v8/hitbox-plan.md) R4、[事前宣告](plans/v8/v8-product-hitbox-data-declaration.md) D1。

## 欄位

| 鍵 | 說明 |
|---|---|
| `version` | 必須是 1 |
| `character_asset_id` | 形狀綁定的角色（骨架） |
| `shapes` | 形狀的陣列 |

每個形狀：

| 鍵 | 說明 |
|---|---|
| `part` | `head`、`body`、`leg`（protocol-v9 §2 的三個部位）。三種都至少有一個形狀；手臂沒有形狀，射線會穿過去 |
| `kind` | `sphere`（球）或 `capsule`（膠囊） |
| `bone` | 球：中心所在的骨頭 |
| `offset` | 球：中心在該骨頭局部座標裡的位置，三個有限值，單位和骨架相同（經骨頭的全域變換換到模型空間） |
| `from`、`to` | 膠囊：兩個端點分別是這兩根骨頭的原點，兩者不同 |
| `radius` | 公尺，量在呈現的身體上（（模型座標 − anchor）× `scale`），範圍 [0.01, 0.5] |

## 驗證（不合就是載入錯誤，Match 啟動失敗）

- 鍵不能多，也不能少。
- 骨頭名稱在角色的骨架裡要剛好出現一次。
- 數值都是有限值，且在上表的範圍內。

## 目前的內容（2026-11-01）

- **頭**：球，綁在 `Head`，半徑 0.184。
- **軀幹**：`spine_01→spine_03`，半徑 0.181。
- **骨盆**：`pelvis→spine_01`，半徑 0.179。
- **腿**：左右各兩段。
  - `thigh→calf`，半徑 0.15；
  - `calf→foot`，半徑 0.105。
- 半徑取自 hitbox-survey 的量測（`v8-product-hitbox-survey`）。頭球的 `offset` 是 survey 的頭部頂點重心，換到 `Head` 的局部座標：站姿與蹲姿相差 1e-6 以內。
