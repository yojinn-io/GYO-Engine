# PvP v8：第三人稱武器換成 nailer（Nailer2 TPS）

日期：2026-10-31。分支 `claude/pvp-nailer-tps-weapon`（從 master `16326f78`）。

**狀態：本機驗證完成，PR 開啟；大小與握法經使用者以兩個 Client 確認**。

## 為什麼

- 第一人稱已在 v7.1 換成 nailer_v2（見 [2026_10_29_pvp_v71_player_v19_nailer](2026_10_29_pvp_v71_player_v19_nailer.zh-Hant.md)），遠端玩家手上仍是 `ultimate_pistol_1`。
- 使用者交來 `nailer2_tps_v2.fbx`（主 checkout 的未追蹤資料夾 `assets/object_fps_pvp/weapons/nailer_tps_v2/`），要接成第三人稱武器。

## 做了什麼

- 新增 `weapons/nailer_v2/world/`（與第一人稱的 `nailer_v2/viewmodel/` 並列）：
  - `Nailer2_TPS.fbx`：使用者檔案原樣改名（SHA-256 `8cc35ed7…`，365,260 bytes）。單一網格、1 個材質 `Nailer2_V02_Atlas_1K`、無骨架與動畫。
  - `textures/Nailer2_D.png`：FBX 內嵌的 1024² JPG 取出後轉 PNG（引擎沒開 JPG 解碼）。
  - `character.json`：以 `material_overrides` 綁上述貼圖。
- `asset_catalog.json`：加三筆 `object_fps_pvp.{weapon,model.weapon,texture.weapon}.nailer_v2.world`。
- `players/presentation.json` 的 `weapon`：改用 nailer，掛點重算（下節）。`node` 仍是 `hand_r`。
- 刪除 PvP 的 `ultimate_pistol_1`（檔案與 catalog 兩筆）；`object_fps` 與 `object_fps_v2` 各自的副本不動。
- `PlayerPresentationTests.cpp`：模型 id 期望值改成 nailer；槍口仰角的量測軸由 `+X` 改為 `-Z`（下節「判定」）。

沒有改程式、骨架、walk／jog clip，也沒有改 `presentation.json` 中 Match 判定會用到的值（`locomotion`、`transition_seconds`、`actions`）。

## 掛點

- 握法：槍嘴朝前、握把朝下，握把中心對到舊手槍握把的位置，旋轉與舊手槍相同（只差 nailer 與手槍之間的軸向對應）。
- 大小：`scale` 0.107491＝舊手槍 scale × 手槍長 1.82 × 1.35，也就是 nailer 槍身（槍嘴到頂）約為舊手槍全長的 1.35 倍。
  - 使用者以兩個 Client 實看後決定：1.5 倍太大，1.15 倍太小，1.3 倍再大一些，定在 1.35 倍。縮放以握把對齊點為中心。
- 渲染出的 nailer 軸向：槍嘴 `-Z`、握把 `-Y`、上方 `+Z`。
  - 用引擎載入模型後，以 `weaponReferencePose` × `geometryToNode` 換算頂點量到的軸向，和渲染結果差了繞 X 的 90°（量到的槍身沿 `+Y`、握把沿 `+Z`）。
  - 依量到的軸向算出的兩版掛點，GUI 擷圖都不對；依兩張擷圖反推出的軸向，兩張都吻合，第三版就正確了。
  - 握把對齊點先依擷圖修正（約後移 0.5、下移 0.67 個槍身長），再依使用者實看把槍往前移 0.15 個槍身長，讓握把落在手中。
  - 量測與渲染為什麼差 90° 沒有追查。舊手槍沒有這個落差（其節點自帶旋轉，nailer 節點沒有）。之後若有程式依武器模型的軸向做判定（例如槍口位置），要先查清楚。

## 判定（AGENTS.md §14.4）

槍口仰角的三個測試（瞄準上下限、遠端俯仰、射擊與換彈保持俯仰）以「武器模型的某軸＝槍管」量仰角。

1. 守的性質：遠端角色的瞄準姿勢讓槍管仰角符合 `aim` 的上下限，且隨俯仰角與動作片段正確變化。
2. 放寬的面：無。
3. 收緊的面：無。新掛點是以「nailer 的 `-Z` 對到手槍的 `+X`」建構的，在手部座標裡兩者方向相差 8e-9（新舊掛點與軸各自換算比較），所以量到的是同一個物理方向，閾值不變。
4. 不適用。
5. 證據：改軸前，nailer 掛點以 `+X` 量會失敗（例：中性仰角 2.40° > 2°、上限誤差 89°），說明測試對槍管軸錯誤有檢出力；改軸後 38/38 通過。

## 驗證

本機 macOS Intel：

- `cmake --preset test`、全量建置通過；`ctest` 108/108。
- 兩個 Client 本機實跑，使用者確認大小與握法（1.35 倍版本）。
- GUI `run_player_short.py --case capture`（1.5 倍版本）：通過，人工看 `turn-upper-body`（側面）與 `idle`（正面）截圖，nailer 在右手中、槍嘴朝前。中途一次因桌面滑鼠輸入干擾而中斷（環境），重跑通過。
- 為了對照，也用舊手槍掛點在建置輸出的資產副本上擷了一張（不在分支裡）：槍管朝前、握把朝下。

## 留下的事

- 換彈動畫仍是 UAL1 的 `Pistol_Reload`（換手槍彈匣），不配合 nailer。使用者說之後再處理；UAL1 沒有其他換彈片段，需要新做。
- 量測與渲染差 90° 的原因未查（上節）。
- HUD 武器名仍是 `MARK23`。
- 主 checkout 的未追蹤原始資料夾 `weapons/nailer_tps_v2/`、`characters/player/` 沒有動。
