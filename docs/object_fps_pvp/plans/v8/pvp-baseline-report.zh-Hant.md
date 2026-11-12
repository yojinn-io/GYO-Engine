件数の突き合わせ：一致（重放：每個 golden 情境的計時次數＝digest 記錄數或重算的 tick 數、37 個 digest 全等於 fixture、long4 計時 3,600／記錄 4,200；真實行程：5 場 305 個統計視窗逐窗「advance 次數＝overrun 次數＝wake 次數」，破口 0；載入 10／10、冷啟動 10／10 都 exit 0）

# v8-product-pvp-baseline 報告：今天的 Match 量多少

更新：2026-11-13。Owner：`object_fps_pvp`。[事前宣告](v8-product-pvp-baseline-declaration.md)（使用者「開始」即核定，架構師 2026-11-12 備查無異議）。本報告**只記錄、不判**：不判 G2a／G2b，不對遷移計畫 §6 的候選預算比較。

## 停止理由（先讀這段）

**重放的 tick 時間（M1）、重放載入（M3）、真實行程冷啟動（M7）的 5 次（或 10 次）散佈兩次都超過 10%，照宣告 §5 是「量不穩、不出基準」。** 第一次量完後，等 5 場真實場次結束、負載平均從 3～4 降到約 2.5～3.4，重量一次（只改時機，不改方法與門檻），散佈仍超過 10%。兩次的中位數卻幾乎相同（下表 BU 列的括號是重量的值）。宣告寫明看結果後不放寬，所以本報告的 M1、M3、M7 數字**只是參考值，不是基準**。真實行程的 RSS（M6）散佈 9.4%，在門檻內。

使用約 1.8 h（估 4～6 h）。單一指標未超過 2 h。

**要決定的事**（架構師與使用者）：

1. **(a) 新宣告、重量**：維持「5 次 p95 散佈 ≤10%」，改量法提高每次的樣本（例如 `--scale 10`、每次重複內多遍），或改成在負載受控時量。要新寫宣告再量。
2. **(b) 先看門檻對這個尺度有沒有意義**：今天一個 tick 是 0.4～4 µs、載入 2.5 ms、冷啟動 1.2 s。§6 的相對係數與絕對加值對這些尺度的分母，10% 散佈是否影響判定，由架構師判斷；若認為 10% 規則對 µs 級分母不適用，改規則要經使用者核准（§14.1），之後依新規則重量。
3. **(c) 改前提（範圍）**：在 Linux 或更安靜的機器量（U8 的 (a)）；本機是代理值，散佈的一部分可能來自筆電本身。

## 給 §6 每個 BU 的「今天的值」

數字是**參考值**（見停止理由）；第一次的 5 次 p95 中位數，括號是重量的值。單位寫在格內。

| BU | 今天的值（Intel Mac 代理） | 形式 | 樣本 | 散佈（第一次／重量） | 狀態 |
|---|---|---|---|---|---|
| BU-1 | **合成內容**（見限界 10）。golden 37 全體：p50 0.445、**p95 3.23**（3.17）、p99 5.64、max 59.8 µs；match 組 p95 1.58（1.58） µs；host 組（`MatchRuntimeHost::Advance`）p95 4.40（4.29） µs；long4 p95 3.39（3.35） µs | 重放 | 每次 14,820＋3,600 tick × 5 次 | p95：全體 20.9%／10.2%，match 12.3%／10.8%，host 37.6%／38.4%，long4 10.5%／25.9% | 不穩、參考值 |
| BU-1′ | Advance p50 ≤250、p95 ≤250、p99 ≤500 µs（格上界）；max 7.2～36.1 ms（中位 17.8 ms）；Advance ≥16 ms 共 10 次（5 場各 5、4、0、0、1）；tick 間隔 stall（>1.5 tick）各 50、46、9、6、7 次；間隔超出名目值 max 64～112 ms | 真實 | 5 場 × 600 s，每場約 36,600 次 Advance | p95 0%（格上界，無檢出力）；max 162% | 只有格上界；max 參考 |
| BU-2 | 不適用：今天沒有 kernel，沒有派發或同步點的開銷 | — | — | — | — |
| BU-3 | 不適用：今天沒有 Registry／Scheduler | — | — | — | — |
| BU-4（World） | **合成內容**（見限界 10）。4 人：實體欄（`bodies_`＋`combats_`＋`animations_`）**1,696 B**（元素 `PlayerState` 128＋`CombatState` 112＋`AnimationTrack` 184 B／人；預留容量同）；加輸入來源（密集 1,088 B＋巢狀估計 300～600 B）與 tick 快取（合成膠囊的 shapes，584～720 B）峰值 **4,104 B**。範圍只含這四類，不含 `arena_`、`hitModel_`、`rules_` 等 | 重放（long4） | 每 tick 量一次 | 0%（決定性） | 參考值（同次重放；數字不隨計時變） |
| BU-4（RSS） | Match 行程穩態 RSS **180 MiB**（5 場穩態中位數的中位數 184,276 KiB；範圍 177,832～195,064 KiB）；峰值中位 184,776 KiB | 真實 | 每場約 550 個穩態取樣 | 9.4% | **≤10%，可當基準**（Mac 代理） |
| BU-5 | 不適用：Client 側，本卡不量 | — | — | — | — |
| BU-6（重放） | **合成內容**（不載 hitbox、pose、動畫內容；見限界 10）。讀 product arena→建 Match＋4 人 Join→第一個 Tick：**2.54 ms**（2.54）；其中讀檔 2.50 ms、建構＋Join 34 µs、第一個 Tick 9 µs | 重放 | 10 個行程 | 25.9%／32.0% | 不穩、參考值 |
| BU-6（真實） | spawn→ready 行：**1.19 s**（1.21）；範圍 1.13～1.42 s（重量 1.08～1.35 s） | 真實 | 10 次冷啟動 | 24.5%／22.0% | 不穩、參考值 |
| BU-7 | 不適用：Client 幀，本卡不量 | — | — | — | — |
| BU-8 | 不適用：今天沒有頁保護 | — | — | — | — |

## 重放（M1、M2、M3）細節

建置：`test` preset、RelWithDebInfo、Apple clang 21、x86_64。量測檔 `gyo_object_fps_pvp_baseline_record`（不進 ctest）。計時只包 authority step：match 組＝`PvpMatch::Tick`，host 組＝`MatchRuntimeHost::Advance`（＝`Step` 全段）；snapshot 編碼、SHA-256、腳本輸入在計時外。每個行程先跑一遍不計時的 golden 37，long4 前 600 tick 不計。百分位用最近秩。

| 組 | tick／次 | p50 µs | p95 µs | p99 µs | max µs | mean µs |
|---|---|---|---|---|---|---|
| golden-match（35 情境） | 11,520 | 0.406 | 1.580 | 3.393 | 51.0 | 0.640 |
| golden-host（2 情境） | 3,300 | 2.422 | 4.400 | 9.626 | 59.8 | 2.796 |
| golden-all（37） | 14,820 | 0.445 | 3.229 | 5.640 | 59.8 | 1.120 |
| long4/product（4 人） | 3,600 | 0.982 | 3.391 | 5.328 | 34.7 | 1.278 |

（5 次的中位數；第一次。重量的中位數：golden-match p95 1.581、golden-host 4.290、golden-all 3.168、long4 3.353。）

**long4 情境**：product arena（`pvp_arena.json`）、4 人各自相位移動、跳、蹲，分段向下一位開瞄準槍，每 400 tick 換彈；4,200 tick 內送出 557 個射擊要求（不管彈藥、冷卻或死亡，不是實際擊發數）、38 次死亡。digest `60b4eafc26f18f780d87802297dad6e88f4a20c302a714a00694eb539dda8b41`，10 次重複全同（後續重放 host 跑同一情境時以它證明）。

**計畫的 8 人長情境做不到**：C++ `MaxPlayers = 4`（`Movement.hpp:21`），Gateway 也是 4，所以改 4 人。§6 BU-4 的「400 實體」與今天的 4 人差兩個數量級；上表 World 位元組是 4 人的值，沒有外推。

## 真實行程（M4～M7）細節

每場：Match（`--arena pvp_arena.json`）→Gateway（本機 `go build`）→`gyo_object_fps_pvp_quad_probe`（4 個 headless Client、60 FPS、互打的 bot、600 s）。數字來自 Match 自己每 10 s 一行的 `network statistics`（`advance_*`＝取樣時鐘後到開始等待前，正是計畫 §3.2 的口徑）與每秒一次的 `ps` RSS。

| 場 | 視窗 | Advance 數 | 60 Hz 期望 | 比值 | max µs | ≥16 ms | stall | overrun max µs | RSS 穩態 KiB |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 61 | 36,632 | 36,705 | 0.9980 | 36,140 | 5 | 50 | 111,861 | 177,832 |
| 1 | 61 | 36,620 | 36,694 | 0.9980 | 31,281 | 4 | 46 | 102,524 | 188,492 |
| 2 | 61 | 36,672 | 36,694 | 0.9994 | 7,195 | 0 | 9 | 73,468 | 195,064 |
| 3 | 61 | 36,683 | 36,694 | 0.9997 | 10,759 | 0 | 6 | 64,447 | 184,276 |
| 4 | 61 | 36,688 | 36,702 | 0.9996 | 17,844 | 1 | 7 | 97,945 | 180,088 |

5 場的 advance p50／p95 都在 ≤250 µs 那一格，p99 在 ≤500 µs 那一格。第 0、1 場的 stall 與 max 明顯多，原因未證實。第 0 場開始時 5 分鐘負載平均 14.6（本會話的建置在其前結束），但第 1 場與第 2 場的 1 分鐘負載平均同在 3～4，stall 卻是 46 對 9；場次期間沒有記 `ps` 前幾名（`real/loadavg-sessions.txt`）。

冷啟動 10 次（ms）：1206、1154、1129、1268、1137、1290、1294、1422、1183、1140。含 `match_main` 開頭對執行檔算 SHA-256 與載入動畫、hitbox 內容；不含 Gateway 連上後的 Ready 交換與第一個 join。

## 散佈 >10% 的原因候選

1. **尺度**：一個 tick 0.4～4 µs，`steady_clock` 兩次讀取的成本（數十 ns）與快取、分支預測的狀態就占幾個百分點；host 組只有 3,300 個樣本，p95 落在幾個樣本上。
2. **CPU 頻率**：Intel Turbo Boost 與省電狀態的切換（i9-9980HK，筆電）；沒有關掉的手段（不改系統設定）。`pmset -g therm` 沒有熱節流紀錄。
3. **背景負載**：同時有其他會話與桌面程式（WindowServer、Claude、Finder、mdworker 在 `ps` 前幾名）；第一次重放時 5 分鐘負載平均 17～18（剛建置完）。但重放第 5 次（負載最低）的 host 組 p95 反而最高，負載平均解釋不了全部。
4. **載入（M3）**：2.5 ms 幾乎全是讀檔與 JSON 解析，單次樣本受檔案快取與配置器狀態影響（第一次有一筆 3.03 ms）。
5. **冷啟動（M7）**：含行程建立、dyld、讀資產與算 SHA-256；log 輪詢 1 ms。

## 範囲の限界

1. **Intel Mac 代理**（MacBookPro16,1、i9-9980HK 8 核 16 執行緒、64 GB、macOS 26.7.1、接電源）。沒有 Linux 數字：無實機，`cross-platform.yml` 的 dispatch 沒有能跑量測檔的入口（不改 `.github/`）。計畫說真實行程基準只在 Linux 有判定效力。
2. **真實行程的 tick 百分位是格上界**（250／500／1000… µs），不改產品量不到更細；p95 散佈因此沒有檢出力。
3. **4 人**，不是計畫的 8 人。
4. M3 不含行程啟動；M7 不含 Ready 交換與第一個 join。
5. OS 檔案快取不清；冷啟動只代表新行程。
6. RSS 是 `ps` 每秒取樣，不是 VmHWM。
7. 統計視窗涵蓋 Match 整個生命（bot 加入前約 1～2 s 在內）；最後一個未滿 10 s 的視窗不印。
8. Gateway 是本機 `go build` 的（macOS 不在 CMake 的平台清單）；不是量測對象。
9. Windows 不量。
10. **重放用的是合成內容**：`AuthorityDigest` 的情境與 `--load` 只把 arena 交給 `PvpMatch`，其餘走預設（`DefaultStanceRules`、`SyntheticPlayerAnimationRules`、`SyntheticPlayerHitModel`）；`match_main` 則傳入載入的 stance、動畫規則與 posed 命中模型（`MakePosedPlayerHitModel`）。所以 BU-1、BU-4（World）、BU-6（重放）不含 posed 命中模型與內容動畫的成本，tick 快取是合成膠囊的 shapes；只有真實行程（M4～M7）是產品組態。遷移後的重放 host 要用同一合成內容比，才是同口徑。
11. **重放與真實行程的計時範圍不同，不可互比**：真實行程的 advance 是 `Advance` 全段，含 mutex 等待與 `publishListener_`（IpcHost 的發布通知），且與 IO 執行緒競爭；重放 host 組沒有 listener 與鎖競爭，match 組只有 `PvpMatch::Tick`。
12. **沒有校準空計時的底線**：兩次 `steady_clock::now()` 約數十 ns，對 p50 0.4 µs 約占一成。
13. **宣告 §5 的負載記錄只做了一部分**：重放每次重複前後記了負載平均與 `ps` 前 5 名（宣告寫前 10 名）；載入的 10 次與場次期間沒有 `ps` 清單；負載平均每 5 s 才更新，0.1 s 的重複前後比對沒有資訊。
14. **量測檔仍在 L1 的全量建置中編譯**，只是不執行（不是 ctest）。

## 量測程式（量測用，`v8-product-match-sim` 接手時一併處理）

- `tests/object_fps_pvp/AuthorityDigest.cpp`：加 `PVP_DIGEST_TIMED_STEP`（預設展開成原敘述）與 `PVP_DIGEST_NO_MAIN`。digest 執行檔不定義它們；`object_fps_pvp.authority_digest` ctest 通過（golden 37、-O0 比對）。
- `tests/object_fps_pvp/BaselineRecord.cpp`、`CMakeLists.txt` 的 `gyo_object_fps_pvp_baseline_record`：不加 add_test，L1 只編譯不執行。量測後依審查把輸出的元素大小註解由 `PlayerAnimation`（snapshot 型別，48 B）改成 `AnimationTrack`；已存證據裡的註解行仍是舊的 `PlayerAnimation=48`，數字欄不受影響。
- `tests/object_fps_pvp/pvp_baseline_session.py`：真實行程，手動執行。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/pvp-baseline/`（本機 MacBookPro16,1；git 管理外）：

- `machine.txt`（機器、工具鏈、執行檔與地圖的 SHA-256）、`worktree.diff`（量測時的程式差分）、`summarize.py`、`summary.json`
- 第一次：`replay/replay-{0..4}.csv`、`replay/samples-{0..4}.csv`（每 tick 的 ns）、`replay/load.txt`；`load/load-{0..9}.csv`；`real/start.csv`、`real/start-*.log`、`real/session-{00..04}/`（match／gateway／probe log、`rss.csv`）、`real/sessions-00.json`、`real/loadavg-sessions.txt`
- 重量：`retry/` 下同樣的結構與 `retry/summary.json`
