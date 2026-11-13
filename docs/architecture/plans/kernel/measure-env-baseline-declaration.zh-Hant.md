# v8-arch-measure-env-baseline 計劃兼事前宣告：四環境的實機對比基線

更新：2026-11-13。Owner：kernel 研究線（架構師開的任務卡；量測對象與量測程式屬 `object_fps_pvp`，本卡只重用、不改其語義）。
分支 `claude/arch-measure-env-baseline`，自 origin/master `8cf52dc1`（PR #201 已合併）開。

**狀態：使用者 2026-11-13 核准**（「照建議，核准」：U-L5 量測時段實機閒置、U-M0 加計時底線、U-M1 Mac 兩欄重用；§9 其餘已答）。量什麼、怎麼量、幾次、對帳與統計的算法都在看到任何新數字之前寫下，看結果後不改（AGENTS §14.1）。正式量測的時段開跑前另取同意（§11）。

## 0. 為什麼（Feature → Contract → Architecture）

- **起因**：前一張卡 `v8-product-pvp-baseline`（[報告](../../../object_fps_pvp/plans/v8/pvp-baseline-report.zh-Hant.md)）在閘門用的 Docker 容器（Intel Mac 上的 Docker Desktop）量 10 次，µs 級指標（M1 重放 tick p95、M3 重放載入、M7 冷啟動）的散佈＝(max−min)／中位數都超過 10%，照宣告停下。
- **使用者的決定（2026-11-13）**：不在「(b) 10% 對 µs 級是否適用」與「(c) 改前提」之間選；先建立四個環境的實機對比基線，之後才談哪邊可以、哪邊不行，最後給任務劃分級別（Intel Mac 就能做／需容器／需 Linux 實機）。
- **支撐的 Feature**：遷移計畫的閘門 G2a（`v8-arch-first-host-cost`）與 G2b（`v8-product-match-host`）——它們要在某台機器上以「今天的值×倍數」（U12）判定；沒有一個能穩定量出今天的值的環境，兩道閘門都判不了。
- **Contract**：不改 wire、digest、golden、量測輸出格式（`format=gyo.kernel-baseline, version=1`）。
- **Architecture**：不變。不碰 `apps/`、`engine/`、`framework/`、`.github/`；`tests/object_fps_pvp/` 只在 §3.4 的範圍內加一支計時底線執行檔（需核准）。

**本卡只量、只記錄、不判。** 不定「過不過」的門檻；10% 規則本卡不改；前一張卡的宣告與報告不改寫。

## 1. 四個環境、資料來源與三個變因

### 1.1 Mac 兩欄重用前一張卡的資料（2026-11-13 確認）

| 代號 | 環境 | 資料來源 | 二進位 | 釘核 |
|---|---|---|---|---|
| **MD** | Intel Mac 上的 Docker Desktop | **重用**前一張卡的容器組（2026-11-13 01:24～03:09，10 次） | ELF，映像 `gyo-linux-toolchain:l1-20261112` 內以 gcc-14 建置，原始碼 `eb018478` | `--cpuset-cpus=8-15 --memory=4g`（VM 內編號） |
| **MN** | Intel Mac 原生 | **重用**前一張卡的本機安靜組（2026-11-13 03:10～04:54，10 次） | Mach-O（Apple clang 21），原始碼 `2a653fc1` | 不可（macOS 沒有 cpuset／taskset） |
| **LN** | Linux 實機原生 | **新量** | 與 MD **位元組相同**的 ELF | `taskset -c <S>` |
| **LD** | Linux 實機上的 Docker | **新量** | 同一個映像、與 MD 位元組相同的 ELF | `--cpuset-cpus=<S> --memory=4g` |

**可以重用的根據**（看新數字前查過，證據位置見文末）：

1. **原始碼沒變**：`git diff eb018478 8cf52dc1` 在 `tests/`、`apps/`、`engine/`、`framework/`、`build/`、`assets/`、CMake 設定之間只有 `pvp_baseline_container.sh` 的 2 行（`REPS` 參數，量測內容不變）。`2a653fc1`→master 同理。
2. **MD 的二進位還在且相同**：Docker volume `pvp-baseline-work` 裡的 `baseline_record`、Match、Gateway、quad 探測與 `pvp_arena.json`，SHA-256 與前一張卡 `container/m/machine.txt` 記的五個值逐一相同（2026-11-13 唯讀核對）。映像 id 仍是 `sha256:4b443a1281195fa961582807bbae8c11e211ce1d73219af7c86fba4c0b937d8d`。所以 LN、LD 直接跑**這幾個檔本身**，三個 ELF 環境是同一份二進位。
3. **量法、次數相同**：前一張卡的修訂 2 已經是 10 次、釘核、安靜時段，MD 與 MN 的原始資料（每次重複的 CSV、每 tick 的樣本、場次 log、RSS 取樣、負載記錄）都在。
4. **MN 的限制照前一張卡寫**：安靜組的 Match 二進位（`07feba75…`）與白天組不同，但原始碼相同（前一張卡限界 16）。MN 本來就只比散佈（§1.2），不影響本卡用途。

**重用的代價（寫進範囲の限界）**：MD、MN 沒有新加的記錄項（clocksource、計時底線 M0），也不是和 Linux 同一時段；Mac 兩欄彼此也不是交錯量的（先 MD 後 MN）。這三點都只影響「跨機器比」，而跨機器本來就只比散佈。若使用者要 Mac 兩欄也用同一時段、交錯重量，見 §9 U-M1（多約 3.6 h 的 Mac 量測）。

### 1.2 每一對差在哪、能比什麼

| 對 | 相同 | 不同 | 絕對值 | 散佈 | 這一對回答的問題 |
|---|---|---|---|---|---|
| **LN↔LD** | 硬體、kernel、二進位、釘的核、時段（交錯量） | 只有容器化（namespace、cgroup、overlayfs） | **可比** | 可比 | U8「容器判閘門」能否成立的直接證據：容器化本身加多少值、多少雜訊 |
| **MD↔LD** | 映像、二進位、資源限制的寫法、CPU 數（8） | 硬體（CPU 型號）、Docker Desktop 的 VM 層、時段 | 不可比（CPU 不同） | 可比 | 同一個容器在 Mac 上不穩，是 VM 造成的還是容器本身 |
| **MN↔MD** | 硬體、同一晚 | VM、OS、**編譯器（Apple clang 21 對 gcc-14）**、二進位格式 | 不可比 | 可比 | Mac 上加了 VM 之後穩定度變多少（不只差 VM，見下） |
| **LN↔MN** | 原始碼 | 硬體、OS、編譯器、時段 | 不可比 | 可比 | 只看穩定度：開發機原生能不能取代 Linux 實機 |
| MN↔LD、LN↔MD | 原始碼 | 其餘全部 | 不可比 | 可比 | 參考 |

**對任務卡的一處更正（架構師請核）**：卡上寫「Intel Mac vs 它的 Docker＝只差 Docker Desktop 的 VM」。實際上 MN 跑 Apple clang 編的 Mach-O、MD 跑 gcc-14 編的 ELF，所以這一對同時差 VM、OS 與編譯器，**只能比散佈**。要單獨看「VM 的影響」，用的是 **MD↔LD**（同一份 ELF，差在有沒有 VM 與 CPU 型號）。RSS 也受 OS 的記帳方式影響，MN 的 RSS 只和 MN 自己比。

## 2. 量什麼（沿用前一張卡的指標與口徑）

| 代號 | 指標 | 形式 | LN、LD 各幾次 | 對應 BU |
|---|---|---|---|---|
| M1 | 重放 authority step 的 p50／p95／p99／max／mean：golden-all、golden-match（35 情境）、golden-host（2 情境）、long4（4 人，暖機 600＋量測 3,600 tick） | 重放 | 10 個行程 | BU-1 |
| M2 | World 位元組（long4） | 重放 | 同 M1（決定性，只核對與 MD 相同） | BU-4 World |
| M3 | 讀 product arena→建 Match＋4 人 Join→第一個 Tick | 重放 | 10 個行程 | BU-6 重放 |
| M4 | 真實行程 Advance 直方圖（p50／p95／p99 格上界、max 精確、≥16 ms 次數） | 真實 | 10 場×600 s | BU-1′ |
| M5 | tick overrun、stall | 真實 | 同 M4 | BU-1′ |
| M6 | Match 行程穩態 RSS（開場 60 s 後的取樣中位數）與峰值 | 真實 | 同 M4 | BU-4 RSS |
| M7 | spawn→ready 行（冷啟動） | 真實 | 10 次 | BU-6 真實 |
| **M0**（新，需核准，§3.4） | 計時底線：連續兩次 `steady_clock::now()` 的差（p50／p95／max），以及 clocksource | 獨立執行檔 | 每個重放行程之前一次；MN、MD 另補量 3 次（數分鐘、負載輕） | 解讀 M1 用，不對應 BU |

暖機、百分位（最近秩）、計時範圍、真實行程的彙整算法、M4～M7 的腳本全部同前一張卡（[宣告](../../../object_fps_pvp/plans/v8/v8-product-pvp-baseline-declaration.md) §3、§5）：`BaselineRecord.cpp`、`pvp_baseline_session.py`、`pvp_baseline_container.sh` 的 `measure` 階段。

## 3. 怎麼量

### 3.1 同一個映像

`docker save gyo-linux-toolchain:l1-20261112` 成 tar 帶到 Linux 實機 `docker load`，載入後核對 image id 與上面相同（不在 Linux 上重 build——apt 抓的套件會漂）。報告寫 image id 與 tar 的 SHA-256。

### 3.2 同一份二進位

- 從 volume `pvp-baseline-work` 唯讀取出 `/work/src` 的量測所需部分（Match、quad 探測、`baseline_record`、Gateway、`assets/object_fps_pvp/`、`tests/object_fps_pvp/pvp_baseline_session.py`、執行時需要的資料），打包成 `bin-eb018478.tar`，附 SHA-256 清單。不重新建置。
- **LD**：把這個包解到實機上的一個 Docker volume，路徑照舊 `/work/src`，用同一個映像、同一個 `measure` 階段（master 的 `pvp_baseline_container.sh`，`REPS=10 RUNS=10 DURATION=600`）。
- **LN**：同一份 ELF 在實機上直接跑。映像是 Ubuntu 24.04（glibc 2.39、gcc-14 的 libstdc++；Match 只連 libstdc++／libm／libgcc_s／libc，2026-11-13 `ldd` 確認）。實機 glibc ≥2.39 時，把映像裡的 `libstdc++.so.6`、`libgcc_s.so.1` 一起放進包的 `lib/`，以 `LD_LIBRARY_PATH` 執行；`preflight` 用 `ldd` 確認全部解析到這兩個檔與系統 glibc。**glibc 較舊時**見 §9 U-L4。LN 跑的命令與 `measure` 階段相同（同一支腳本的邏輯抽成 LN 版，路徑不同、加 `taskset`）。
- 開量前在 LN、LD 兩處重算 SHA-256，和 MD 的五個值比對。

### 3.3 執行順序與負載

- **Linux 上 LN 與 LD 交錯跑**（ABAB）：重放 rep0 LN→rep0 LD→rep1 LN…；載入、冷啟動、場次同樣交替。目的：時段漂移（溫度、背景工作）平均落在兩邊，LN↔LD 的差不被時間序混淆。
- **釘核 `<S>`**：8 個 logical CPU（與 MD 的 `--cpuset-cpus=8-15` 同數量、同編號）：實機的 `8-15`＝實體核 4～7 的兩個執行緒（2026-11-13 `lscpu -e` 確認 SMT 兄弟相鄰），不含 CPU 0。LN 與 LD 用同一組。`preflight` 再記一次拓撲。
- **一台機器同時只跑一個量測**；量測期間不跑任何建置。實機的閒置條件見 §9 U-L5。
- **每次重複前後記錄**：1 分鐘負載平均、`ps` CPU 前 10 名（同前一張卡容器組）；場次期間每分鐘一次。另記（只讀，不改）：`scaling_governor`、turbo 狀態（`intel_pstate/no_turbo` 或 `cpufreq/boost`）、clocksource、`/proc/pressure/cpu`、CPU 溫度（有 `/sys/class/thermal` 就記）。**不改任何系統設定**（governor、turbo、SMT 照現狀）。
- **暖機**：同前一張卡；另外 LN、LD 開量前各跑一次不計的重放（丟棄）。

### 3.4 程式的變更（需核准；只在 `tests/object_fps_pvp/`）

- 新增一支**獨立**執行檔 `gyo_object_fps_pvp_timer_floor`（不進 ctest，比照 `baseline_record`）：連續 100,000 對 `steady_clock::now()` 的差，輸出 p50／p95／max 一列（`kind=timer-floor`，帶 `format=gyo.kernel-baseline, version=1` 標頭）。
- **為什麼獨立、不加進 `BaselineRecord.cpp`**：加進去會讓 `baseline_record` 的二進位變掉，MD 的資料就不能重用（§1.1 第 2 點）。獨立執行檔讓 M1～M7 的四個 ELF 保持位元組相同。
- 它在同一個映像內以 master 的原始碼建置（只建這一個 target）；Mac 原生版以 Apple clang 建置。理由：前一張卡限界 12「沒有校準空計時的底線」；VM 與實機的 clocksource 不同（kvm-clock／tsc／Hypervisor），可能是 MD 散佈大的一部分。
- 投遞與收回的腳本（§4）、統計腳本放證據資料夾，不進 repo（與前一張卡的 `run-quiet.sh`、`summarize.py` 同樣處理）。
- 若不核准 M0：其餘照做，報告寫「計時底線未量」。

## 4. Linux 實機的投遞與收回（具體方式待 §9）

- 不假設現場有網路。投遞包＝`image.tar`（`docker save`）、`bin-eb018478.tar`、（核准 M0 時）`timer-floor.tar`、`run-linux.sh`（一支腳本，子命令 `preflight`、`measure`、`pack`），附 SHA-256 清單。
- `preflight`：只讀。記 OS、kernel、CPU 型號與拓撲、glibc、`ldd` 結果、Docker 版本、cgroup v2、可用磁碟、governor、clocksource；任一前提不成立就印出並以非 0 結束，不量。
- `measure`：照 §3.3 交錯跑 LN 與 LD；輸出到同一資料夾。
- `pack`：把輸出打成一個 tar 交回。收回後放主 checkout 的 `build/evidence/measure-env-baseline/linux/`。
- **交出前每條命令在本機實際跑過**：LN 的路徑在 Mac 上用同一映像的容器跑一遍（容器就是 Ubuntu 24.04；`taskset` 在容器內可用）；LD 的路徑在 Mac 主機以 docker CLI 跑一遍；都用短版（`REPS=1`、載入與冷啟動各 1、`RUNS=1 DURATION=30`），不算量測，輸出放 `dryrun/`、不進報告。
- **文件不寫主機名、帳號、IP**；只寫「使用者的 Linux 實機（CPU 型號、核數、OS 版本）」與證據位置。

## 5. 件数の突き合わせ

規則同前一張卡宣告 §4（M1：每情境計時樣本數＝digest 記錄數、golden 37 個 digest 等於 fixture、long4 3,600／4,200；M3、M7：10 列、`TickCount()==1`／exit 0；M4／M5：逐窗「advance＝overrun＝wake」、`identity_breaks`＝0；M6：取樣數記錄）。LN、LD 各自對帳；MD、MN 沿用前一張卡已記的對帳結果（報告第一行「一致」）。另加：

- 四個環境的 golden 37 digest 與 long4 digest（`60b4eafc…`）全部相同。
- ELF 五個 SHA-256 在 LN、LD、MD 三處相同；image id 在 LD、MD 兩處相同。

報告第一行照 AGENTS §14 寫「件数の突き合わせ：一致／不一致／確認できない」。不一致或確認不能的環境×指標，不進 §6 的並列，等使用者判斷。

## 6. 統計（只記錄，不判）

每個環境×每個指標，對同一批原始資料（M1：10 個行程各自的 p95，p50 另列；M3、M7：10 個值；M6：10 場的穩態中位數；M4：max 與格上界）算：

| 統計量 | 定義 |
|---|---|
| 中位數 | 10 個值的中位數（取中間兩個的平均；前一張卡取法相同） |
| 全距比 | (max−min)／中位數（前一張卡的散佈） |
| 四分位距比 | (Q3−Q1)／中位數；Q1、Q3 用線性內插（numpy 預設的 `linear`） |
| 去極值全距比 | 排序後去掉最高兩個與最低兩個，剩 6 個的 (max−min)／中位數 |
| 組間中位數偏差 | 10 次依執行順序分前 5、後 5 兩組：\|中位數(前5)−中位數(後5)\|／中位數(全10) |
| 跨環境中位數比 | 只對絕對值可比的 **LN↔LD**：中位數(LD)／中位數(LN) |

- MD、MN 的全距比必須等於前一張卡報告的數字（重算核對；不等就停下查，不改前一張卡）。
- **10% 這條線在報告裡只當參考線列出**，不寫「過」「不過」。「10% 對 µs 級是否適用」由這批資料回答：同一指標在 LN 上四種散佈量的大小，就是「這台實機上這個量能分辨多大的差」。
- 參考欄：前一張卡的本機白天兩組（5 次）只列全距比與中位數，不進 §7 的分級（次數不同、不是安靜時段）。
- 計算腳本放證據資料夾；報告的每個數字都能由原始 CSV 重算。

## 7. 報告與任務分級（只寫建議）

報告最後一節依資料提出「Intel Mac 可做／需容器／需 Linux 實機」的分級建議，由使用者定，不自行寫成規則。**分級的推法在看新資料前寫定**：

1. 對每個環境×指標，取「去極值全距比」與「四分位距比」中較大者當**可分辨的相對差 r**（越小越能分辨細的變化）。
2. 列出各任務要分辨的相對差 d，出所為遷移計畫 §6 的候選倍數（例：BU-1 ×1.25 → 25%；BU-4 RSS ×1.10 → 10%；功能開發時的「有沒有明顯變慢」→ 由使用者給）。這些倍數在 U12 重核前都是候選，報告照引並標「候選」。
3. 建議：r ≤ d／2 的環境「可做」該任務；最便宜的可做環境就是建議的級別（成本順序 MN＜MD＜LD＜LN）。r 介於 d／2 與 d 之間記「邊緣」；都不到就寫「四個環境都分辨不了」。
4. 同時列出 LN↔LD 的中位數比：若容器化本身讓值偏移，容器判閘門時要和「同一映像、同一機器的容器基線」比，不能和實機的值比——這一點寫進對 U8 的建議。

## 8. 影響範圍（只建議，不改）

- **U8**：結論寫成 kernel HANDOFF 的一行**提案**（修訂或取代 U8 三層），決定行由使用者寫定；本卡不改遷移計畫（PR #199 的檔）。
- **U12**：§6 的「今天的值×倍數」重核排在本卡之後，本卡不提預算值。
- **BU-4 RSS 的分母**：「容器內 RSS 能否單獨當分母」用 LN↔LD 的 RSS 中位數比與散佈再提建議，由使用者定。
- 不碰：`v8-arch-a6-revision`、`v8-arch-kernel-scaffold`、`v8-arch-h7-scanner` 的檔；前一張卡的宣告、報告、dev_log；volume `pvp-baseline-work` 只唯讀掛載。
- 交付時同一變更更新 kernel `README.md` 的批次列與 `HANDOFF.md` 的現況、寫 dev_log（`docs/dev_logs/2026_11_<日>_arch_measure_env_baseline.zh-Hant.md`）。

## 9. 未決（使用者回答後補進本節，再請核准）

| 代號 | 問題 | 答案／建議 |
|---|---|---|
| U-L1 | Linux 實機怎麼連 | **已答（使用者 2026-11-13）**：我經 ssh 直接操作（金鑰登入，本機 `~/.ssh` 的專用金鑰）。**所有建檔只在實機的指定工作資料夾**（`~/Workspace/Remote` 之下；使用者：不動 home 的其他地方，建置都放在那裡） |
| U-L2 | 實機規格 | **已查（2026-11-13 唯讀 ssh）**：x86_64；AMD Ryzen 7 6800H（8 核 16 執行緒，SMT 兄弟是相鄰編號 0/1、2/3…）；30 GiB；Ubuntu 24.04.5、kernel 7.0.0-42；glibc 2.39、libstdc++6 `14.2.0-4ubuntu2~24.04.1`（與映像同版本）；clocksource tsc；governor `powersave`、boost 開；cgroup v2；Docker 已裝且服務在跑，**但帳號不在 `docker` 群組，不用 sudo 連不上 docker socket**→見 U-L6；工作資料夾所在分割區剩 62 GB；開機中是桌面環境（graphical.target） |
| U-L3 | 投遞路徑 | **已答**：scp（同一個區網） |
| U-L4 | glibc <2.39 時怎麼辦 | **不適用**：實機 glibc 2.39、libstdc++ 與映像同版本，LN 可直接跑同一份 ELF（仍照 §3.2 附上映像的兩個 so，`preflight` 以 `ldd` 確認解析結果） |
| U-L5 | 量測時段實機是否閒置（約 4 h 內不跑其他工作、不睡眠） | **已答（照建議）**：閒置；時段開跑前另約 |
| U-L6 | docker 權限：(a) 你把帳號加進 `docker` 群組（`sudo usermod -aG docker <帳號>`，重新登入生效）；(b) LD 那半由你在現場以 sudo 跑腳本的 `measure-ld` 子命令 | **已答（使用者 2026-11-13）**：(a)，使用者已加入 `docker` 群組；新的 ssh 登入可不用 sudo 連 Docker（Server 29.9.3，與 Mac 的 Docker Desktop 同版本號）已確認 |
| U-M0 | §3.4 的計時底線（獨立執行檔）是否加 | **已答（照建議）**：加 |
| U-M1 | Mac 兩欄：重用前一張卡的資料（§1.1），或和 Linux 一樣交錯重量 | **已答（照建議）**：重用 |

**ssh 的約定**：金鑰登入、`BatchMode`（不輸入也不看密碼）；主機金鑰記在本會話的暫存區，不寫你的 `~/.ssh/known_hosts`；只在實機的 `~/Workspace/Remote/measure-env-baseline/` 內建檔、不用 sudo、不改系統設定；量完我只取回輸出 tar，現場檔案留給你決定刪不刪。實機的主機名、IP、帳號不寫進 repo。
（2026-11-13 本機的私鑰檔權限是 0644，ssh 拒用；已改成 0600，只收緊、內容未動。）

## 10. 範囲の限界（先寫下）

1. **Docker Desktop 的 VM 不是實機**：MD 的計時含 VM（Apple Virtualization framework，前一張卡的記錄）的 vCPU 排程；`--cpuset-cpus` 是 VM 內編號，不對應主機實體核。
2. **兩台機器 CPU 不同**：Linux 實機（AMD Ryzen 7 6800H，筆電級，governor `powersave`、boost 開）與 Intel Mac（i9-9980HK）的絕對值不可比，只比穩定度（§1.2）。兩台都是筆電級 CPU，頻率會浮動；這正是要量的現實條件之一，照現狀記錄。
3. **MN 不能釘核**（macOS 無 cpuset）；MN↔MD 也差編譯器與 OS。
4. **Mac 兩欄是重用的舊資料**：與 Linux 不同日、不同時段；MD 與 MN 不是交錯量的；沒有 clocksource、溫度、PSI 的記錄（M0 若核准則事後補量，但不是同一時段）。
5. **真實行程的 tick 百分位只有格上界**（250 µs 起跳）：M4 的散佈沒有檢出力，只看 max 與 ≥16 ms 次數。
6. **重放用合成內容**、**4 人**、M3 不含行程啟動、M7 不含 Ready 交換——同前一張卡限界 1、3、4、10。
7. **系統設定照現狀**：不關 turbo、不改 governor、不關 SMT；兩台機器的這些狀態只記錄（Mac 側只有 `pmset -g therm`），可能是散佈的來源之一。
8. **RSS 用 `ps` 每秒取樣**，不是 VmHWM；macOS 與 Linux 的 RSS 定義不同。
9. **一個時段的資料**：每個環境只有一個時段（10 次）；不同日、不同溫度下的變化不在範圍內。
10. **CI runner 不量**（U8 照舊不用）。

## 11. 估時、停止條件、機器負載

- **估時**：準備（取出二進位、打包、腳本、本機乾跑、M0 執行檔）1.5～2.5 h；Linux 量測牆鐘約 3.5～4 h（重放 2×10、載入 2×10、冷啟動 2×10、場次 2×10×約 10.5 min），背景執行；Mac 只有 M0 補量數分鐘；分析與報告 2～3 h。**實作工時 3.5～5.5 h，牆鐘 7～9.5 h；上限 9.5 h，1.5 倍＝14 h 停下回報**。（U-M1 選重量時，Mac 加約 3.6 h 牆鐘，可與 Linux 同時跑，上限不變。）
- **停止條件**（AGENTS §14 冒頭）：同一問題換兩次量法仍無結論或大幅超估時；原因的說明會改計測的基準、Contract 或計劃前提（例如 Linux 實機也一樣不穩到量不出任何東西）；發現回歸或確定超出本計劃範圍。**單一問題累計超過 2 h 停下回報**。§6 的 MD／MN 重算與前一張卡的數字不一致時也停。
- **件数不一致的環境**：該環境×指標停下、不補量，報告寫明，等使用者判斷。
- **機器負載**：全部 headless、不用 GPU；Match＋Gateway＋4 個 Client 約 1～2 核心、記憶體數百 MB；Docker 容器上限 4 GiB。Linux 實機約 4 h；Mac 只有打包（`docker save` 約 2 GB 的寫入）與數分鐘的 M0。**開量前再向你說明時段並取得同意。**
- **磁碟**：Linux 兩個環境的證據預估 25 GB 左右（前一張卡兩組 13 GB，主要是每 tick 的樣本 CSV）；實機與主 checkout 的空間在開量前確認。

## 12. 模型、檔位、審查

| 工作 | 模型／檔位 | 理由 |
|---|---|---|
| 準備與量測執行（腳本、打包、乾跑、監看） | Opus／medium | 下段作業，但任務卡要求不用小模型 |
| 統計、分析、報告、分級建議 | Opus／high | 結論會動到 U8 |
| 審查（只讀） | Sonnet／high | 與作者（Opus）不同模型，同前一張卡由架構師核的做法；重點：數字能否由原始資料重算、統計是否照 §6、分級是否照 §7 的推法、重用的根據（§1.1）是否成立 |
| 驗證者（§14.4） | 不需要 | 不改任何判定 |

本會話開工時由 Fable 5.2 切到 Opus 5.5（使用者切換），以下都由 Opus 執行。

## 證據

- 重用：主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/pvp-baseline/`（本機 MacBookPro16,1）的 `container/`（MD）與 `quiet/`（MN）；二進位在本機 Docker volume `pvp-baseline-work`。
- 本卡：主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/measure-env-baseline/`（git 管理外）：`reuse-check.txt`（§1.1 的 diff 與 SHA-256 核對）、`bundle/`（投遞包與 SHA-256）、`linux/`（收回的 LN、LD 輸出）、`mac-m0/`、`dryrun/`（乾跑，不進報告）、`analyze.py`、`summary.json`。
