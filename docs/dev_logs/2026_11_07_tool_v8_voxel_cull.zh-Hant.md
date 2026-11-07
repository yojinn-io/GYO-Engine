# 工具 v8：ve-cull（預覽的繪製距離與視錐剔除）

日期：2026-11-07。分支 `claude/v8-tool-voxel-editor-measure`。上段（觸及 Engine 公開介面）。實作：主會話（Opus 5.5）。
事前宣告：[voxel-editor-cull.md](../tools/plans/v8/voxel-editor-cull.md)，2026-11-07 使用者核准，矩陣選 (b)（「承認、(b) で」）。

**狀態：** 判定 C1～C6 完成；L1 三平台通過（PR #170，2026-11-07，run 42377813964）。

## 做了什麼

- **Engine `GYO::Math`**：新增 `geometry/Frustum.hpp`，內容是 `Frustum`（六個朝內的平面）、`MakeFrustum(clip)`、`Intersects(Frustum, Aabb)`。
  - 判定是保守的：只有盒子完全在某一個平面外側時才回傳 false。
  - 觸碰算相交。
  - near 平面用 0..1 深度的 `z >= 0`。
  - `math.md` 新增「Frustum」一節，Layout 與 Types 同步更新。
- **Engine `GYO::Render`**：把既有的 view／projection 計算公開成 `CameraViewProjection(camera, aspect)`（`RenderTypes.hpp`）。
  - Renderer 自己畫世界 mesh 也改用它，所以剔除用的矩陣就是畫的矩陣。
  - 行為不變：凍結的逐位元組測試照舊通過。
- **工具**：
  - 新增核心的 `Visibility.hpp`／`.cpp`（`ChunkView`、`MakeChunkView`、`Draws`）。
  - 預覽記下每個區塊 mesh 的 AABB（取自頂點），每幀只畫「離注視點水平距離在繪製距離內，而且和視錐重疊」的區塊。
  - 介面加了繪製距離的滑桿（64～1024 格，預設 256）與「drawn N / M chunks」。
  - `measure-preview` 加 `--draw-distance`，CSV 加 `chunks_drawn` 欄。
- **測試**：
  - `gyo_math_tests` 加 Frustum 規則測試 7 案例。
  - `gyo_render_tests` 加「`CameraViewProjection` 就是世界 mesh 上傳的矩陣」（40 個隨機相機）。
  - 核心加 Visibility 6 案例。
  - CLI 加 `--draw-distance` 的範圍檢查 3 條。
  - GPU 的 pick smoke 加「方塊所在的區塊沒被剔除」。
- **突變**：catalog 加 9 個（`math-frustum-*` 5、`render-cvp-not-drawn-matrix` 1、`ve-cull-*` 3），**9/9 killed**。

## 量測

機器：本機 MacBook Pro（Intel Core i9-9980HK、64 GB、macOS 26.7.1），RelWithDebInfo，`--no-edit`，P1 的俯瞰相機。

| 代號 | 結果 | 預算 | 判定 |
|---|---|---|---|
| CB1 第 1 次（Chrome 執行中） | p95 27.42 ms（p50 10.13），畫 767／768 | p95 ≤ 16.7 ms | **不成立**（照記，不覆寫） |
| CB1 重測（Chrome 關閉） | p95 **8.12 ms**（p50 6.90），畫 767／768 | 同上 | **成立** |
| CB2 km2-ground，256 格 | p95 **8.36 ms**（p50 7.68、max 9.34），畫 **877**／15,625；最大常駐記憶體 0.43 GB | p95 ≤ 16.7 ms、畫 ≤ 2,000 | **成立** |
| CB3 km2-ground，1024 格 | 60 幀試跑：p50 173 ms、max 191 ms，畫 12,965／15,625；最大常駐記憶體 0.48 GB | 不設（只記錄） | 已記錄 |

件數の突き合わせ：
- CB1 兩次、CB2：一致，要求 600、量到 600、失敗 0、統計 540。
- 診斷的 6 次與重測的 6 次：一致。
- CB3：試跑 60／60／0。

比較：沒有剔除時，km2-ground 全畫是 p95 388 ms（ve-scale-measure 的 SM6）。

### CB1 第 1 次不成立的處理（AGENTS §14）

- **停下回報**：使用者選「旧版と並べて診断」。
- **診斷**（探索，不當判定）：
  - 從 P1 當時的 commit `a19458cc` 另建舊版，在同一環境、同一張圖、同一個相機，和新版交互跑兩輪。
  - 舊版也是 p95 約 23 ms（P1 當時 7.48 ms），新版 `--grid` 約 25 ms，p50 都約 10 ms。
  - 計測中 Chrome 的 Renderer 佔 CPU 50～115%、WindowServer 約 38%。
  - 結論：差距主要來自環境，不是 ve-cull。
- **重測**：使用者關掉 Chrome 後（「Chromeを閉じたら測る」），跑之前宣告「第 1 輪新版 `--synthetic-map` 是 CB1 的判定」。
  - 結果 8.12 ms，成立。
  - 對照的舊版 6.39／8.17 ms，第 2 輪新舊相同，所以 Chrome 執行中看到的約 2 ms 差是雜訊。

## 和宣告的差異

1. **距離的中心改成注視點**（量測前；使用者在問題框選「注視点から (推奨)」）：
   - 宣告寫「和相機的水平距離」。
   - orbit 相機拉遠時，相機離地圖數百格，從相機量會什麼都不畫；P1 的俯瞰相機下 CB2 也會是 0 區塊。
   - 理由的全文（含遊戲端與過場仍以相機為中心）寫在宣告裡，經 side chat 轉來。使用者原話：「這個其實我有些糾結。我想選按相機來處理，但是如果這樣，我要做過場動畫時移動相機，做鏡頭時會不會有奇怪的問題出來」；聽完說明後：「OK，雖然我選了，你把理由交過去吧。」
2. **CB3 只用 60 幀的試跑記錄**（使用者選「CB2 今、CB3 は試走で記録」）：
   - 宣告寫 600 幀。
   - 試跑每幀約 173 ms，600 幀會讓 GPU 約 2 分鐘滿載，接近上次讓機器卡死的負載。
   - CB3 本來就不設預算。
3. **CB1 的第 1 次和重測都留著**：重測的環境（Chrome 關閉）是在看到第 1 次的結果之後才指定的。預算沒有改。

## 觀察（給架構師；不在本批處理）

- **CB3 顯示視錐剔除在俯瞰時幾乎不減少區塊數**：整張圖都在畫面裡。
  - 大範圍俯瞰仍然需要 ②（以 region 合併緩衝區）或 ③（遠處代理）。
  - 繪製距離目前是唯一的上限。
- **剔除不減少上傳的 mesh 數**：km2-building 的 46,875 個 mesh 仍會全部建立，驅動崩潰（SM6）的風險沒有因本批消失，所以沒有測。
- **本機的計測容易被其他程式干擾**：
  - Chrome 執行中，p95 會從約 8 ms 漲到約 25 ms。
  - 之後的計測宣告，要寫明環境條件（關掉哪些程式），並放一個舊版的對照。

## 架構師審查（C4，2026-11-07）

- **結論**：成立，沒有阻擋合併的項目。
- **確認的項目**：
  - `MakeFrustum` 取列的方法、平面的符號、near 與 far 的公式、平面順序。
  - `Intersects` 的保守判定與 closed。
  - 歸屬：Frustum 在 Math、不知道相機；`CameraViewProjection` 有逐值比對的守衛。
  - `math.md` 的三處更新。
- **可選建議，兩條都照做了（只改註解與文件）**：
  - N1：標頭與 `math.md` 註明「只有 `MakeFrustum` 的結果有意義」，預設值不是全通的體，也不是空的體。
  - N2：`math.md` 註明改深度慣例（例如 reversed Z）時，這幾列要跟著改。
- **給 ② 的看法**：
  - 先試只上傳繪製距離內的區塊 mesh、離開範圍就釋放。這是工具側的改動，不碰 Render API，就能避開 km2-building 的驅動上限。
  - region 級合併與 Render 的部分更新，等這一步量過仍不夠才提。
  - 俯瞰時視錐剔除無效是幾何上必然的，俯瞰的上限由距離上限管。

## 證據

主 checkout `build/evidence/tool-v8-voxel-map/editor-next/cull/`：
- `cb1-building.*`：CB1 第 1 次（不成立）；
- `diag/`：新舊並排診斷（`protocol.txt` 含結果）；
- `quiet/`：關掉 Chrome 後的 CB1 重測；
- `trial/`：km2-ground 的 60 幀試跑（CB2、CB3）；
- `cb2/`：CB2；
- `environment.txt`。

舊版的建置在本會話的 scratchpad，未保存；需要時從 `a19458cc` 重建。
