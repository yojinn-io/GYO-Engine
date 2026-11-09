# 工具 v8：ve-scale-measure（1 km² 合成圖的量測）

日期：2026-11-07。分支 `claude/v8-tool-voxel-editor-measure`（從 PR #169 `1d2f9ffc` 切出）。下段。實作：主會話（Opus 5.5／medium）。
事前宣告：[voxel-editor-measure.md](../tools/plans/v8/voxel-editor-measure.md)，2026-11-07 使用者核准（「推奨どおり承認」）。

**狀態：** 量測完成，結果只描述、不設門檻。本機 `cpu|shader` 138/138；L1 三平台通過（PR #170，2026-11-07，run 42377813964）。

## 做了什麼

- **合成圖產生器**移到核心的 `Synthetic.hpp`／`.cpp`：`BuildingGrid`（從 `MapMeasure.cpp` 搬過來，產出不變：grid sha `60e3aa3c…` 照舊）、`Km2GroundGrid`、`Km2BuildingGrid`、`SyntheticByName`。量測命令與預覽量測都用它；不經檔案（km² 的格子文字超過 16 MiB）。
- **`measure-map <out-dir> --scale km2-ground|km2-building`**：量 SM1～SM5、SM7，每項 5 次，結果寫進 `scale-<map>.txt`。
- **`measure-preview --synthetic-map <name>`**：SM6。
- **CLI 測試**：加了參數檢查。

## 量測結果（只描述）

機器：本機 MacBook Pro（Intel Core i9-9980HK、64 GB、macOS 26.7.1），RelWithDebInfo。
件數の突き合わせ：
- SM1～SM5：一致，每項要求 5、量到 5、失敗 0。
- SM6 km2-ground：一致，要求 600、量到 600、失敗 0。
- SM6 km2-building：0 幀就異常結束（見下）。

| 項目 | km2-ground | km2-building |
|---|---|---|
| 區塊／填的格數 | 15,625／4,253,952 | 46,875／44,022,882 |
| SM1 格子文字 | 3.97 MB（未超過 16 MiB）；寫出中位數 155 ms | **233.8 MB（超過 16 MiB，`ParseGrid` 會拒讀）**；寫出 2.55 s |
| SM2 格子的記憶體（計算值） | 256 MB | 768 MB |
| SM2 行程的最大常駐記憶體（`/usr/bin/time -l`） | 0.60 GB | 3.09 GB |
| SM3 拖曳開始時的整份複製 | 中位數 130 ms | **中位數 394 ms**（最大 511 ms） |
| SM4 調色盤 undo 一步（前後兩份） | 512 MB；複製 241 ms | **1.54 GB**；複製 839 ms |
| SM5 整張烘焙／寫出 | 3.09 s／0.47 s；區塊化文字 4.6 MB；quads 55,814、colliders 23,625 | **46.6 s／28.2 s**；區塊化文字 242 MB；quads 4,872,510、colliders 1,032,504 |
| SM6 全畫的影格（`--no-edit`，600 幀丟前 60） | **p50 304 ms、p95 388 ms、p99 476 ms、max 650 ms**（15,625 個 mesh，沒有視錐剔除） | **異常結束**（見下） |
| SM7 拾取距離 | 1,024 格；地圖對角 2,828 格，**碰不到** | 同左 |

### SM6 km2-building 的異常結束

- **崩潰位置**：macOS 的 crash report（`~/Library/Logs/DiagnosticReports/gyo_voxel_map_editor-2026-11-07-181920.ips`）顯示 `SIGABRT`，abort 從 AMD 的 Metal 驅動裡呼叫（`ioAccelResourceListAddNewGroupAndResource`，在 `drawIndexedPrimitives` 中）。
- **原因**：一幀引用的 GPU 資源（緩衝區）太多，超過驅動的上限。預覽每個區塊有一個 mesh（自己的頂點與索引緩衝區），每幀全部提交；46,875 個區塊超過了上限。
- **定位**：這不是程式錯誤，是設計碰到上限。依 AGENTS §2「Architecture の反復を計画する条件」，記錄並回報；不在量測批裡修。
- **決定**：使用者決定照 (a)：用現有結果記錄，不再重跑（原話「OK，結果回報架構，剩下照建議執行」）。

### 事故：量測讓整台機器卡死

- **經過**：SM6 的兩次執行，加上我為了看崩潰原因的一次重試（5 幀），讓使用者的整個系統卡住。km2-ground 是每幀約 300 ms、600 幀，GPU 被占滿數分鐘；km2-building 在上傳時崩潰。
- **檢討**：宣告寫了「GPU 不足的失敗也記下」，卻沒有寫「會占住整台機器」，也沒有事先問使用者，崩潰後還立刻重試了一次。
- **之後**：重負載的本機量測先講影響、取得同意，或請使用者挑時段（記憶 `warn-before-heavy-local-runs`）。

## 和宣告的差異

1. **SM2 的最大常駐記憶體改由外部讀取**（`/usr/bin/time -l`，macOS 報告的也是 `getrusage` 的值），沒有在工具裡呼叫 `getrusage`。
   - 原因：依賴守衛（DI 1）不讓核心 include `sys/resource.h`。要放進去，就得放寬守衛（§14.4）。
   - 量的是同一個值，只是讀法不同。這是看到結果之前就改的。
2. **`km2-building` 的產生方法**：
   - 宣告寫的是「把 `building` 的產生規則平鋪到 2000×2000（同一個種子序列，依區塊座標決定）」。
   - 實作是：每個 256×256 的瓦片都是同一張 `building`（同一個種子），依區塊座標複製，超過 2000 格的區塊不放。
   - 這是對「平鋪」的一種讀法，請確認。

## 結論給 Q3（交使用者）

- 只有地面時，v1 撐得住：格子 4 MB。
- 和合成建築一樣密時，格子 234 MB，超過 v1 的上限 16 MiB 十四倍以上，v1 撐不住。所以 `ve-grid-v2`（分檔）的觸發條件成立。
- 是否開 v2、何時開，由使用者決定。

## 觀測到的上限與對策候選（回報架構師；不在本批修）

| 問題 | 候選 | 所屬 |
|---|---|---|
| 全畫 15,625 區塊 p95 388 ms；46,875 區塊驅動崩潰 | ① 距離上限＋視錐剔除（只改工具）② 以 region 合併緩衝區（可能要改 Engine Render）③ 遠處代理（LOD，big-map 曾決定 v0.1 不做） | ① 工具；②③ 架構（產品的 Client 依 region 畫也會遇到同一個上限） |
| 拖曳開始 394 ms | 拾取時忽略本筆改過的格，不複製整份文件 | 工具 |
| 調色盤 undo 1.5 GB | 存差分（調色盤變更不改格子內容） | 工具 |
| 整張烘焙 47 s | 區塊互相獨立，可用 `GYO::Threads` 平行烘焙 | 工具 |
| 格子文字 234 MB | `ve-grid-v2` 分檔 | 工具（格式） |
| 拾取到不了對角 | 跳過空區塊的區塊級 DDA，拉長距離 | 工具 |

使用者說「剩下照建議執行」。照建議的順序是：先做 ①（另寫事前宣告），再做 `ve-feel-survey`，因為 km² 圖上的延遲量測會被全畫的 388 ms 蓋過。②③ 交架構師一起規劃。

## 證據

主 checkout `build/evidence/tool-v8-voxel-map/editor-next/scale/`：
- `scale-km2-*.txt`：SM1～SM5、SM7；
- `time-*.txt`：最大常駐記憶體；
- `sm6-*.csv` 與 `sm6-*.out`：SM6；
- `environment.txt`；
- `ctest-cpu-shader.txt`。

crash report 已複製進同一個目錄（`gyo_voxel_map_editor-2026-11-07-181920.ips`）。
