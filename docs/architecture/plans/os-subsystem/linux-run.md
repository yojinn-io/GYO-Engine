# OS 子系統：Linux 項目的執行（os-linux-run）

建立：2026-10-28。決定：D75（[交接](HANDOFF.md)的決策紀錄）。
**狀態：事前宣告完成，等本會話執行。**

## 這份文件是什麼

- OS 子系統還沒在 Linux 上做的項目，由本會話在這台 Mac 的 Docker 裡跑（D75）。
- 項目來源：交接的「下一次實機的清單」的 Linux 部分、各批的紀錄、PLAN 第 7.13～7.15 節。
- 判定都在這份文件裡、看到任何結果之前寫定（AGENTS.md §14.1）。這份文件先提交，之後才開始建置與量測。

## 環境與範圍的限界

- Docker Desktop 29.8.1，跑在 macOS Intel（i9-9980HK）的 LinuxKit 虛擬機上：kernel 7.0.14-linuxkit，16 顆 vCPU，記憶體 7.75 GB，linux/amd64（不經模擬）。
- 映像：[`scripts/linux-run.Dockerfile`](scripts/linux-run.Dockerfile)。ubuntu:24.04 加上 CI 的 Linux 套件清單（`.github/actions/native-setup/action.yml`），另加 git、ccache 與 pip 的 CMake 3.31.6。
- **限界**（宣告與結果都要寫）：
  - 計時量測跑在虛擬機裡，數字含 macOS 主機的排程影響，不代表實體 Linux。
  - 容器以 root 執行，但沒有 `CAP_SYS_NICE`：提高執行緒優先度會失敗，降低可以。和 CI 的 runner（一般使用者）同一類條件。
  - 容器沒有 Go：標 `go` 的測試不跑（`-LE go`）。
  - 量測期間，這台 Mac 上的其他會話可能在動作；記錄開始與結束的時間，無法排除。
- 做：

| 代號 | 白話 | 判定 |
|---|---|---|
| L0 | 映像、三份 checkout、三份建置 | — |
| L1 | B 的 ctest（不含 `go`） | 記錄；用到的測試失敗就停下回報 |
| L2 | 複製量測程式 | — |
| L3a | os-wait-backend 的 A/A（cli） | 關卡：(a) 是否在這個環境可用 |
| L3 | os-wait-backend 的 A/B（cli） | L3a 通過時照 (a) 判定；否則只記錄 |
| L4 | os-wake-record 的 Linux 基準（cli） | 只記錄（D68） |
| L5 | os-threads-backend 的 L2 | `role started` 行逐字相同（D69） |
| L6 | Linux 專用突變 5 條 | killed／survived／未跑 |

- 不做：視窗情境（Linux 只量 cli，PLAN 第 7.15 節）；實體 Linux 的計時（之後另找機器時再說）。

## 規則

- 只跑、只記錄。不改程式碼、測試、突變目錄與 CMake。
- 停止條件（AGENTS.md §14 開頭與 §14.1）：
  - L3 照 (a) 不通過或無效：不重跑，停下回報。撤回方式是 revert（D71）。
  - L5 不相同：停下回報（D69）。
  - L1 中 L3～L6 會用到的測試失敗（`gyo_time_tests`、`gyo_threads_tests`、`gyo_os_tests`、`gyo_trace_tests`、`object_fps_pvp.worker`、`.ipc`、`.close_timing`、`.cpu`）：停下回報。其他失敗只記錄。
  - ow-x1 存活：照 PLAN 第 7.15 節要補測試，那是程式變更，這次不做，停下回報。
  - 同一個問題換了 2 次做法還沒有結論，或用時超過 6 h（估計的 1.5 倍）：停下回報。
- 看到結果之後不放寬判定。回報的第一行是收支。
- 證據在 Docker volume `gyo-linux-run` 的 `/work/evidence/`；跑完複製到主 checkout 的 `build/target/_build/test/logs/os-linux-run-20261028/`（不進 repo）。

## L0：準備（約 1～1.5 h）

- 建映像：`docker build -f docs/architecture/plans/os-subsystem/scripts/linux-run.Dockerfile -t gyo-linux-run:1 docs/architecture/plans/os-subsystem/scripts`。
- 容器：volume `gyo-linux-run` 掛在 `/work`（虛擬機的 ext4，可放非 UTF-8 的檔名，os-f8 要用）；主 checkout 唯讀掛在 `/host-repo`；不限制 CPU。
- 三份 checkout，從 `/host-repo` clone 到 `/work`：
  - B：本分支提交這份文件的 commit（程式和 `c54ef8b5` 相同）。
  - A_wait：`4ffc7c65`（os-wait-backend 之前）。
  - A_threads：`5733d361`（os-threads-backend 之前）。
- 建置（三份相同；組態對應 macOS 的產品組態 `wide`；ccache 讓三份共用編譯結果，不改產生的程式）：

```
cmake --preset test -DCMAKE_C_COMPILER=gcc-14 -DCMAKE_CXX_COMPILER=g++-14 \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DGYO_APPS=object_fps_pvp -DGYO_TOOLS= -DGYO_BUILD_SDL_GPU_BACKEND=OFF \
  -DGYO_BUILD_SDL_IMAGE_LOADER=OFF -DGYO_BUILD_SDL_TTF_ADAPTER=OFF -DGYO_BUILD_UFBX_LOADER=OFF
cmake --build build/target/_build/test -j 8
```

- configure 或建置失敗：記錄，換做法 2 次以內；仍不行就停下回報。

## L1：B 的 ctest（約 10～15 min）

```
xvfb-run -a -s '-screen 0 1920x1080x24' ctest --test-dir build/target/_build/test -LE go --output-on-failure
```

- 和 CI 一樣在 xvfb 底下跑。記錄通過、失敗、Skipped；判定見「規則」。

## L2：複製量測程式

- 從 A_wait 與 B 的建置樹複製 `gyo_time_wake_record` 到 `/work/evidence/programs/A`、`programs/B`，記錄 SHA-256。
- **從這裡到 L5 結束，不建置。**

## L3a：A/A（約 21 min；關卡）

```
python3 tests/common/time/wake_compare.py --a /work/evidence/programs/A --b /work/evidence/programs/A --output /work/evidence/wait-AA --pairs 4 --modes cli --a-label "A_wait 4ffc7c65" --b-label "A_wait 4ffc7c65 (A/A)"
```

- 關卡：腳本對 A/A 的判定（(a) 的 6 項：deadline 與 notify 的 P99、P50，不早醒，後端相同）**全部通過**，才表示 (a) 在這個環境可用。
- 通過：L3 照 (a) 判定。
- 不通過或無效：(a) 在這個環境不可用。L3 照跑但只記錄，不當驗收；結果回報使用者，由使用者決定 Linux 的驗收怎麼做。不重跑 A/A。

## L3：A/B（約 21 min）

```
python3 tests/common/time/wake_compare.py --a /work/evidence/programs/A --b /work/evidence/programs/B --output /work/evidence/wait-AB --pairs 4 --modes cli --a-label "A_wait 4ffc7c65" --b-label "B <短 SHA>"
```

- 判定（L3a 通過時；D71 凍結的 (a)，Linux 只有 cli，deadline 與 notify 都判）：
  - 兩側各 4 次的中位數：B 的 P99 ≤ A 的 P99 × 1.25 ＋ 0.05 ms；B 的 P50 ≤ A 的 P50 ＋ 0.02 ms。
  - B 不早醒（deadline 的 min_ms ≥ 0）；兩側後端相同（Linux 是 ConditionVariable）。
- exit 0＝通過，1＝不通過，2＝無效。不通過或無效都不重跑，停下回報。
- 記錄：12 項中 cli 的 6 項、每次的 P50／P99／min／max、sleeper 的 P99、`meta.txt` 的負載。

## L4：os-wake-record 的 Linux 基準（約 3 min；只記錄）

```
python3 tests/common/time/wake_baseline.py --build-dir /work/a-wait/build/target/_build/test --output /work/evidence/wake-baseline --modes cli
```

## L5：os-threads-backend 的 L2（約 10 min）

- A_threads 與 B 各跑兩次（串行，`-V`，在 xvfb 底下）：
  - `ctest --test-dir build/target/_build/test -V -R "^object_fps_pvp\.(worker|ipc|close_timing)$"`
  - `ctest --test-dir build/target/_build/test -V -R "^object_fps_pvp\.cpu$"`
- 從輸出取出從 `role started` 到行尾的文字，去重、排序，比較兩份。
- 判定：逐字相同（D69）。另記錄 `wait_backend`、`name_applied`、`priority_applied` 的值（沒有 `CAP_SYS_NICE` 時，提高優先度的角色預期是 0）。

## L6：Linux 專用突變（約 30 min）

- 在 B 的 checkout，每條單獨跑：

```
python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py --build-dir build/target/_build/test --cmake cmake --catalog docs/architecture/plans/os-subsystem/scripts/mutations.json --only <id>
```

- exit 0＝killed；1＝看印出的結果（survived、stale、wrong_failure、timeout、build_failed）；2＝基準線不通過。
- 5 條：os-f8-posix-cwd-not-utf8、tt-x5-linux-trace-write-failure-ignored（`/dev/full`）、ti-x4-linux-name-not-cut、th-x6-linux-background-nice、ow-x1-cv-latch-outside-mutex。
- 跑完 `git status` 要乾淨。

## 時間

約 3～4 h，大部分是建置與量測的等待。跑的期間用 `caffeinate -i` 防止 Mac 睡眠，跑完就結束它。

## 結果

（還沒跑。）
