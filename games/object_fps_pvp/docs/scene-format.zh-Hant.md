# Scene 檔的格式（資料契約，version 1）

Scene 檔是 arena 的「裝修」：門之類放在地圖上的東西。地圖（`map`）是毛坯房的牆，scene 是擺在裡面的物件。
Game 只讀這個檔；之後的 scene 編輯工具只寫這個檔，兩邊只經這個格式交接（AGENTS §1.7）。Game 沒有工具也能跑：手寫的 JSON 就能玩。

出處：`v8-arch-gyo2-interact-door` 宣告第 5 節（`docs/architecture/target/v2/interact-door-declaration.zh-Hant.md`）。
讀的程式：`games/object_fps_pvp/src/world/Scene.cpp`（`ParseScene`）。

## 1. 位置

- `assets/object_fps_pvp/scenes/<名字>.json`。
- 由 arena 檔（`maps/*.json`，version 5）的選填欄位 `scene` 引用：相對 content 根的路徑，例 `"scene": "scenes/pvp_arena.json"`。
- arena 沒有 `scene` 欄＝沒有裝修。

## 2. 格式

```json
{
  "version": 1,
  "id": "pvp_arena",
  "objects": [
    { "id": 1, "set": "doors", "data": { "...": "那一組自己的欄位" } }
  ]
}
```

| 欄位 | 意思 | 讀取失敗 |
|---|---|---|
| `version` | 1 | 不是 1 |
| `id` | scene 的名字，1～64 字元 | 缺、空、超過 64 |
| `objects` | 物件的陣列，照檔內順序放進 World | 不是陣列 |
| `objects[].id` | 物件的**穩定身分**：正整數 1～99 999，scene 內唯一 | 0、超過範圍、和另一個物件相同 |
| `objects[].set` | 屬於哪個玩法組（模式配置檔 `sets` 的組名） | 遊戲沒有這一組、或這一組不收 scene 物件 |
| `objects[].data` | 那一組自己的資料（物件），由組的 `readScene` 解析 | 不是物件、或組的 reader 說的 |

- 錯誤訊息寫出是第幾個物件、它的 `id`、哪一欄：例 `object 2 (id 7): set 'doors': width is not positive`。
- scene 讀不到或解析失敗，引用它的 arena 讀取失敗（整份內容不載入）。

## 3. 物件的 `id`

- 物件的 `EntityId`＝`3 100 000`＋`id`（`world::SceneEntity`）。兩端讀同一份 scene，所以兩端同號；物件不經網路生成，每一端在第一幀前自己放。
- **刪掉的 `id` 不再重用**（像資料庫的 id）：一個 `id` 一直指同一個物件。由寫檔的人（之後是工具）負責，reader 只查唯一。
- **手寫期間的分配約定**（只是寫檔的人之間的約定；格式不規定，reader 也不檢查）：
  - 門（`doors`）從 1 起。
  - 靶子（`targets`，頭頂 UI 卡 #372）從 101 起。
  - 之後由工具負責分配。

## 4. 比對（兩端是否讀同一份）

- `MatchInfo::sceneDigest`：這一場的 scene 裡、**模式配置開著的組**的物件，每個物件的 `id`、組名、組的 reader 讀到的值（用 Kernel 的 Hasher，不 hash 檔案位元組；照 `id` 排序，檔內順序不算）。
- 沒開的組的物件不算：改它們不會讓別的模式配置判成不一致。
- Client 的值和 Match 不同：`ContentMismatch{Scene}`，帶原因回大廳。
- 沒有開著的組（例：核心 PvP）：空集合的值，兩端相同。

## 5. 放進 World

- 模式配置開著的組的物件才放（組的 reader 給的 `put`）；沒開的照樣解析、不放。
- 放的時機：World 的 `Prepare`（第一幀前），每一端都放。

## 6. 版本

- version 1：本頁。
- 改格式（加必填欄位、改意思）時版本加一，舊版本讀取失敗（Client 與 Match 從同一個 commit 建，沒有舊檔要讀）。
- 各組 `data` 的欄位由那一組定，寫在那一組的說明（門：宣告 5.1「門的 `data`」）。

## 7. 工具

之後的 scene 編輯工具自己持有這個格式的知識，加一個對齊測試：工具寫出的檔，遊戲讀得進來、`sceneDigest` 和手寫的一樣。
