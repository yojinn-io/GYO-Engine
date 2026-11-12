件数の突き合わせ：一致（四組都一致。重放：每次重複的計時次數 golden-match 11,520／golden-host 3,300／long4 3,600 固定、37 個 digest 每次都等於 fixture、long4 digest 全同；真實行程：本機白天 5 場、本機安靜 10 場、容器 10 場，每場 61 個統計視窗逐窗「advance 次數＝overrun 次數＝wake 次數」，破口 0，Advance 數對 60 Hz 期望的比值 0.998～1.000；載入、冷啟動每組 10／10 exit 0）

# v8-product-pvp-baseline 報告：今天的 Match 量多少

更新：2026-11-13。Owner：`object_fps_pvp`。[事前宣告](v8-product-pvp-baseline-declaration.md)（使用者「開始」即核定，架構師 2026-11-12 備查無異議；修訂 1、2 由架構師 2026-11-13 追加）。本報告**只記錄、不判**：不判 G2a／G2b，不對遷移計畫 §6 的候選預算比較。

## 停止理由（先讀這段）

**µs 級分母（重放 tick M1、重放載入 M3）與冷啟動（M7）在閘門用機器（Docker Linux 容器）上量不穩：10 次 p95 散佈都超過 10%，照宣告不出基準。** 容器組裡只有穩態 RSS（M6）在門檻內。本機安靜組有三項過門檻（M1 的 host 組與 long4、M7），但本機是開發參考，不是閘門用機器（U8）；而且本機安靜組的 RSS 反而不過（白天 9.4%、安靜 42.9%）。修訂 2 的判讀條件是「兩組都不穩」：**兩組確實都不穩**（容器組 M1、M3、M7 全不過；本機安靜組 golden-all 10.1%、match 12.8%、M3 13.8%、RSS 42.9% 不過）＝「µs 級分母在此機器量不穩」，下面 (b)／(c) 由使用者決定。 10% 規則本卡不改。

四組（同一宣告、同一判定；散佈＝(max−min)／中位數，門檻 10%）：

| 指標 | 本機白天第一組（5 次） | 本機白天重量（5 次） | **容器組（10 次，閘門用）** | 本機安靜組（10 次） |
|---|---|---|---|---|
| M1 golden-all p95 | 20.9% | 10.2% | **16.7%** | 10.1% |
| M1 golden-match p95 | 12.3% | 10.8% | **27.8%** | 12.8% |
| M1 golden-host p95 | 37.6% | 38.4% | **48.4%** | 8.7% ✓ |
| M1 long4 p95 | 10.5% | 25.9% | **12.8%** | 4.5% ✓ |
| M3 載入到第一個 Tick | 25.9% | 32.0% | **38.9%** | 13.8% |
| M7 冷啟動到 ready | 24.5% | 22.0% | **15.0%** | 2.7% ✓ |
| M6 穩態 RSS | 9.4% ✓ | （未重量） | **5.0% ✓** | 42.9% |
| M4 Advance p95（格上界） | 0%（無檢出力） | — | 0%（無檢出力） | 0%（無檢出力） |

使用約 5.7 h（估 4～6 h；2026-11-12 23:20～11-13 04:56，其中容器量測與本機安靜組約 3.5 h 是背景執行）。容器組與本機安靜組照修訂 2 是新的追加量測，不是同一指標的重量；單一指標的不穩定沒有累計超過 2 h 的排查。

**要決定的事**（使用者；架構師轉交）：

1. **(b) 10% 規則對 µs 級分母是否適用**：改規則要經使用者核准（§14.1），之後依新規則重量。判斷用的事實：
   - 中位數跨組大致可重現：本機 golden-all p95 三組 3.23／3.17／2.95 µs（三組之間相差約 9%）。
   - 散佈多半來自一兩次重複偏高或分群：容器第 6 次重複的 p50 四組都最高，p95 在 match 與 long4 最高（host 與 all 的 p95 最高是第 9 次）；容器 host 組 p95 分成約 2.35 µs（5 次）與約 2.72 µs（3 次）兩群，另有 3.04、3.56 兩次。原因未證實（候選見下文）。
   - 10 次的 (max−min) 一般比 5 次大，同一門檻在 10 次下較嚴（修訂 2 已註明）。
   - 對立的事實：同一門檻、同樣 10 次，本機安靜組的 host 組（8.7%）、long4（4.5%）、冷啟動（2.7%）過門檻，表示這個尺度在較安靜的條件下並非量不到；容器組的不穩也可能來自虛擬化，而不是門檻本身。
2. **(c) 改前提（範圍）**：Linux 實機（U8：只在出問題且允許時）；或新宣告改量法（例如每次重複內多遍、加大樣本、換統計量）後重量。
3. 不做決定前，本報告的 M1、M3、M7 數字都**只是參考值**。容器內 RSS（M6）過門檻：這是**逐指標**套用宣告 §5 的讀法；修訂 2 的判讀字面以「容器組整體」為單位，RSS 能否單獨當分母請使用者確認。

## 先讀的限界

1. **重放用的是合成內容**：BU-1、BU-4（World）、BU-6（重放）的分母都來自 `PvpMatch` 的預設 stance、動畫規則與合成命中模型，不是 `match_main` 的產品組態（posed 命中模型、內容動畫）。遷移後的重放 host 要用同一合成內容比，才是同口徑（細節見限界 10）。
2. **容器跑在 Docker Desktop 的 Linux VM 上**（macOS 主機的虛擬化），不是 Linux 實機；計時含虛擬化的影響（限界 15）。
3. **真實行程的 tick 百分位只有格上界**（250／500／1000… µs），p95 散佈沒有檢出力（限界 2）。

## 給 §6 每個 BU 的「今天的值」

格內是中位數（重放是每次 p95 的中位數），括號是散佈。**只有標 ✓ 的格過門檻；閘門用的是容器欄。**

| BU | 容器內（閘門用；Linux gcc-14） | 本機安靜組（Intel Mac） | 本機白天第一組（重量） | 狀態 |
|---|---|---|---|---|
| BU-1（重放，**合成內容**） | golden-all p95 **1.61 µs**（16.7%）；match 0.82（27.8%）；host 2.56（48.4%）；long4 2.42（12.8%） | golden-all 2.95（10.1%）；match 1.44（12.8%）；host 3.91（8.7% ✓）；long4 2.91（4.5% ✓） | golden-all 3.23（3.17）；match 1.58（1.58）；host 4.40（4.29）；long4 3.39（3.35） | 容器不穩：參考值 |
| BU-1′（真實） | Advance p50／p95 ≤250、p99 ≤500 µs（10 場同格）；max 中位 0.81 ms（0.72～2.0 ms）；≥16 ms 0 次；stall 1 次 | p50／p95 ≤250；p99 ≤500（7 場）／≤1000（3 場）；max 中位 0.87 ms（0.80～22.5 ms）；≥16 ms 1 次；stall 1 次 | p50／p95 ≤250、p99 ≤500；max 中位 17.8 ms；≥16 ms 5 場中 3 場共 10 次；stall 118 次 | 只有格上界；max 參考 |
| BU-2、BU-3 | 不適用：今天沒有 kernel、Registry、Scheduler | — | — | — |
| BU-4（World，**合成內容**） | 4 人：實體欄 **1,696 B**；含輸入與 tick 快取峰值 **4,104 B** | 同 | 同 | 決定性（各組相同） |
| BU-4（RSS） | 穩態 **30.3 MiB**（31,076 KiB；5.0% ✓；30,348～31,896 KiB） | 164 MiB（167,544 KiB；42.9%；129,512～201,404 KiB） | 180 MiB（184,276 KiB；9.4% ✓） | 容器值過門檻（逐指標讀法，待使用者確認） |
| BU-5、BU-7 | 不適用：Client 側，本卡不量 | — | — | — |
| BU-6（重放，**合成內容**） | 讀 arena→建 Match＋4 人 Join→第一個 Tick **2.35 ms**（38.9%） | 2.93 ms（13.8%） | 2.54 ms（2.54） | 不穩：參考值 |
| BU-6（真實） | spawn→ready **1.50 s**（15.0%；1.45～1.67 s） | 0.98 s（2.7% ✓；0.97～0.99 s） | 1.19 s（1.21） | 容器不穩：參考值 |
| BU-8 | 不適用：今天沒有頁保護 | — | — | — |

World 的範圍：實體欄＝`bodies_`＋`combats_`＋`animations_`（元素 `PlayerState` 128＋`CombatState` 112＋`AnimationTrack` 184 B／人；預留容量同）；另加輸入來源（密集 1,088 B＋巢狀估計 300～600 B）與 tick 快取（合成膠囊的 shapes，584～720 B）。不含 `arena_`、`hitModel_`、`rules_` 等。

## 容器內（修訂 1、2）

- 映像：`build/ci/linux-toolchain/Dockerfile`，基底 `ubuntu:24.04@sha256:2198e4c65076e9cb0b3d695219df667c0e3b0072da64ca3c2648aca17dcecaf5`；建成的映像 `gyo-linux-toolchain:l1-20261112`，id `sha256:4b443a1281195fa961582807bbae8c11e211ce1d73219af7c86fba4c0b937d8d`。工具鏈：gcc-14 14.2.0-4ubuntu2~24.04.1、cmake 3.31.6、ninja 1.11.1、Python 3.12.3、Go 1.23.12（照 L1 linux-x64 run 43244933991）。
- Docker Desktop 29.9.3；VM 16 CPU、記憶體約 8.3 GB、kernel 7.0.14-linuxkit。
- 資源限制：`docker run --cpuset-cpus=8-15 --memory=4g`（容器內 `nproc` 8、cgroup `memory.max` 4 GiB、`cpu.max` 不限）。
- 原始碼：`eb018478` 的 `git archive`；量測腳本 `tests/object_fps_pvp/pvp_baseline_container.sh`（取自 `2a653fc1`，只多 `REPS` 參數）。`test` preset、RelWithDebInfo。容器內 `object_fps_pvp.authority_digest` ctest 通過（Linux gcc-14 下 golden 37 的 digest 與 Mac 相同）。
- 時間：2026-11-13 01:24～03:09（本機時間）。容器內 1 分鐘負載平均：重放前 0.00～0.07；場次期間 0.08～0.81（中位 0.31）。主機側（macOS）負載平均：開始 1.71／3.56／4.55、結束 3.46／2.79／2.47；開始前等 XProtect 背景掃描結束。

| 組 | tick／次 | p50 µs | p95 µs（散佈） | p99 µs | max µs | mean µs |
|---|---|---|---|---|---|---|
| golden-match | 11,520 | 0.355 | 0.817（27.8%） | 1.504 | 43.9 | 0.491 |
| golden-host | 3,300 | 1.315 | 2.557（48.4%） | 5.101 | 25.5 | 1.474 |
| golden-all | 14,820 | 0.385 | 1.605（16.7%） | 3.280 | 47.4 | 0.709 |
| long4/product | 3,600 | 0.948 | 2.422（12.8%） | 3.577 | 38.5 | 1.121 |

（10 次的中位數。）載入：讀檔 2.31 ms、建構＋Join 35 µs、第一個 Tick 2.0 µs。真實場次 10 場：Advance 數 36,661～36,666，對 60 Hz 期望比值 0.9999～1.0000；overrun max 中位 0.88 ms（第 4 場一次 54.4 ms，即那 1 次 stall）。

## 本機安靜組（修訂 2）

2026-11-13 03:10～04:54，使用者就寢、本機不跑建置（容器組結束後才開始，不同時）。重放前 1 分鐘負載平均 1.22～1.81；場次期間每分鐘記的 `ps` 前 3 名：quad 探測（中位約 14%）與 Gateway（約 10%）常駐前兩名，Docker VM 偶爾出現（103 筆中 3 次）。

| 組 | tick／次 | p50 µs | p95 µs（散佈） | p99 µs | max µs | mean µs |
|---|---|---|---|---|---|---|
| golden-match | 11,520 | 0.383 | 1.443（12.8%） | 3.099 | 37.8 | 0.583 |
| golden-host | 3,300 | 2.229 | 3.910（8.7%） | 7.180 | 29.3 | 2.449 |
| golden-all | 14,820 | 0.413 | 2.948（10.1%） | 4.236 | 37.8 | 0.999 |
| long4/product | 3,600 | 0.902 | 2.914（4.5%） | 4.081 | 23.9 | 1.089 |

載入：讀檔 2.89 ms、建構＋Join 37 µs、第一個 Tick 10.6 µs；10 次大致由 2.66 ms 遞增到 3.0 ms 附近（原因未查）。冷啟動 10 次 0.968～0.994 s。真實場次 10 場：第 5 場有一次 22.5 ms 的 Advance（≥16 ms 唯一一次，也是唯一的 stall）；穩態 RSS 各場 126～197 MiB（129,512～201,404 KiB），散佈 42.9%（白天 5 場 174～191 MiB）。

## 本機白天第一組：重放（M1、M2、M3）細節

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

## 本機白天第一組：真實行程（M4～M7）細節

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

## 散佈 >10% 的原因候選（白天第一組時寫；容器組與安靜組的補充在最後兩項）

1. **尺度**：一個 tick 0.4～4 µs，`steady_clock` 兩次讀取的成本（數十 ns）與快取、分支預測的狀態就占幾個百分點；host 組只有 3,300 個樣本，p95 落在幾個樣本上。
2. **CPU 頻率**：Intel Turbo Boost 與省電狀態的切換（i9-9980HK，筆電）；沒有關掉的手段（不改系統設定）。`pmset -g therm` 沒有熱節流紀錄。
3. **背景負載**：同時有其他會話與桌面程式（WindowServer、Claude、Finder、mdworker 在 `ps` 前幾名）；第一次重放時 5 分鐘負載平均 17～18（剛建置完）。但重放第 5 次（負載最低）的 host 組 p95 反而最高，負載平均解釋不了全部。
4. **載入（M3）**：2.5 ms 幾乎全是讀檔與 JSON 解析，單次樣本受檔案快取與配置器狀態影響（第一次有一筆 3.03 ms）。
5. **冷啟動（M7）**：含行程建立、dyld、讀資產與算 SHA-256；log 輪詢 1 ms。
6. **容器組**：容器內負載近 0，但主機側負載（1 分鐘 1.7～3.5）未排除。候選是 VM 的 vCPU 排程（`--cpuset-cpus` 只限定 VM 內的 CPU，不限定主機實體核）與主機 CPU 頻率；未證實。冷啟動在容器裡較慢（1.50 s 對本機 0.98～1.19 s）且散佈 15%。
7. **本機安靜組**：負載低時冷啟動（2.7%）、host 組（8.7%）、long4（4.5%）過門檻，golden-all（10.1%）與 match 組（12.8%）差一點；RSS 反而不穩（42.9%），白天 5 場是 9.4%。本機 RSS 是容器的 5～6 倍，原因未查。

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
15. **容器不是 Linux 實機**：Docker Desktop 的 Linux VM（Apple Virtualization framework）跑在同一台 Intel Mac 上，計時含虛擬化；`--cpuset-cpus=8-15` 是 VM 內的 CPU 編號，不保證對應主機的實體核。
16. **本機兩組的 Match 執行檔不是同一個二進位**：SHA-256 第一組 `4a8f1eb1…`、安靜組 `07feba75…`（量測間有重新建置）。`apps/`、`engine/`、`framework/` 從 base `d0d8563f` 起沒有原始碼差分；量測檔也因輸出註解的修改重編過（`2108a4ef…`→`c43b330e…`）。Gateway 與 quad 探測相同。
17. **容器組與本機組的計時不可直接比**：編譯器（gcc-14 對 Apple clang 21）、OS 與虛擬化都不同。容器值只和之後同一映像、同一資源限制的容器值比。

## 量測程式（量測用，`v8-product-match-sim` 接手時一併處理）

- `tests/object_fps_pvp/AuthorityDigest.cpp`：加 `PVP_DIGEST_TIMED_STEP`（預設展開成原敘述）與 `PVP_DIGEST_NO_MAIN`。digest 執行檔不定義它們；`object_fps_pvp.authority_digest` ctest 通過（golden 37、-O0 比對）。
- `tests/object_fps_pvp/BaselineRecord.cpp`、`CMakeLists.txt` 的 `gyo_object_fps_pvp_baseline_record`：不加 add_test，L1 只編譯不執行。量測後依審查把輸出的元素大小註解由 `PlayerAnimation`（snapshot 型別，48 B）改成 `AnimationTrack`；已存證據裡的註解行仍是舊的 `PlayerAnimation=48`，數字欄不受影響。
- `tests/object_fps_pvp/pvp_baseline_session.py`：真實行程，手動執行。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/pvp-baseline/`（本機 MacBookPro16,1；git 管理外）：

- `machine.txt`（機器、工具鏈、執行檔與地圖的 SHA-256）、`worktree.diff`（量測時的程式差分）、`summarize.py`、`summary.json`
- 第一次：`replay/replay-{0..4}.csv`、`replay/samples-{0..4}.csv`（每 tick 的 ns）、`replay/load.txt`；`load/load-{0..9}.csv`；`real/start.csv`、`real/start-*.log`、`real/session-{00..04}/`（match／gateway／probe log、`rss.csv`）、`real/sessions-00.json`、`real/loadavg-sessions.txt`
- 重量：`retry/` 下同樣的結構與 `retry/summary.json`
- 容器組：`container/`（`docker.txt`＝Docker 版本、VM、映像 id、資源限制；`build.log`；`src-eb018478.tar`；`pvp_baseline_container.sh`；`measure-start.txt`、`measure.log`）；輸出在 `container/m/`（`machine.txt`、`replay/`、`load/`、`real/`、`summary.json`）
- 本機安靜組：`quiet/`（`run-quiet.sh`、`times.txt`、`run.log`、`machine.txt`、`replay/`、`load/`、`real/`、`summary.json`）
