# PvP v8：v8-product-hitbox-shot-frame（S1：Client 的兩段換算抽成共用函式）

日期：2026-11-01。分支 `claude/pvp-v8-after-pv8`。檔位 high｜Opus 5.5。估時 2～3 h，實際約 1 h。

**狀態：完成。** 照[事前宣告](../object_fps_pvp/plans/v8/v8-product-hitbox-shot-declaration.md)的 S1（`v8-product-hitbox-frame-sharing` (a)）。這一步不改任何行為。

## 做了什麼

- **`PlayerFrameFromAuthority`**（`player_content` 的 `PlayerPose.cpp`）：把 `PvpApplication.cpp` 迴圈裡「snapshot → frame」的那一段原樣搬出來。
  - 輸入：玩家的權威狀態、同一個區段的戰鬥狀態與 Match 的動畫、呈現的 Tick。
  - 輸出：位置、yaw、pitch、命、死亡、蹲姿、grounded、`lifeStateSeconds`，以及射擊、換彈、受擊的時刻。
  - Client 專用的 `deltaSeconds`、`continuous`、`holding` 照舊由 `PvpApplication` 填。
- **`PlayerBodyPointToWorld`**：身體上的一點（模型座標）換到世界座標：先減 `anchor`，再套 `ComposeEulerXYZ(位置, {0, yaw, 0}, scale)`。這就是 Client 送給 renderer 的變換；武器掛點的觀測值改用這個函式。
- `PvpApplication` 與 `PlayerPresentation` 改呼叫這兩個函式。S2 的姿勢模型會用同樣的兩個函式。

## 判定

- **J1：通過。**
  - B3 的 H-pose 仍是 `dc7544253139d07e`／29568（這台 Intel Mac），呈現測試全部通過。
  - 新測試「PvP presentation frame takes each authority field into its own frame field」：
    - 27 種輸入，各改一個值，frame 只有對應的欄位改變；
    - 沒有動畫、沒有戰鬥狀態、射擊 id 是 0、換彈的結束不晚於開始等情況，該清掉的欄位都清掉；
    - 姿勢不讀的戰鬥欄位（hp、攻擊者）改了，frame 不變；
    - 時刻換算和原本 Client 的寫法相同（例如 `600.25 / 60`）。
  - 新測試「PvP body points reach the world as the renderer draws them」：
    - 腳底的 `anchor` 在任何 yaw 下都落在玩家的位置；
    - yaw 0 時，模型上的位移只乘上 scale；
    - 結果和 renderer 的公式逐位元相同。
  - 這也補上 A4 記下的 G1：「取樣放進 frame」原本只有 GUI 驗收看得到，現在有單元測試。

## 突變

**9／9 killed**（`batch` 為 `v8-product-hitbox-shot-frame`，7 個），在未提交的工作樹上用 `--allow-dirty` 跑。

- 本批新增 7 個：frame 少了射擊的時刻、換彈的結束、受擊次數、蹲姿、命，各一個；空的換彈仍然帶出去；世界座標少了 `-anchor`。
- D184 相關的既有突變 2 個：`16-observed-falls-back`、`ac-frames-first-column`。抓它們的測試和本批改動相鄰，所以重跑。

## 其他測試

- `test` preset 的 CTest：`-j4` 時 110／111，`presentation_evidence` 逾時（60 s）。當時主機的負載是 29。單獨重跑 24 s 通過。
  - A3 也記過它在平行負載下會逼近 60 s 的上限。這次的改動不影響它的路徑（它跑的是 Client 的呈現，行為不變，H-pose 也不變）。原因歸為環境（主機負載），沒有再追。

## 範圍的限界

- J1 比的是 Client 端算出的矩陣；GPU 實際的浮點運算不比較。
- `PvpApplication` 把取樣與 Client 專用欄位接起來的那幾行，仍然只有 GUI 驗收看得到。
