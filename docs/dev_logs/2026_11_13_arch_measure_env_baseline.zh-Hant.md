# Kernel 研究線：v8-arch-measure-env-baseline（四環境的實機對比基線）

日期：2026-11-13。Owner：kernel 研究線（架構師開的任務卡；量測對象與程式屬 `object_fps_pvp`，只重用）。分支 `claude/arch-measure-env-baseline`。[事前宣告](../architecture/plans/kernel/measure-env-baseline-declaration.zh-Hant.md)、[報告](../architecture/plans/kernel/measure-env-baseline-report.zh-Hant.md)。

- 執行：本會話（Opus 5.5；準備與量測 medium、分析與報告 high）；審查者＝Sonnet／high 唯讀子代理（宣告 §12）。
- 性質：只量、只記錄、不判。10% 規則不改；前一張卡的宣告、報告、dev_log 不改。程式只加一支獨立執行檔；`apps/`、`engine/`、`framework/`、`.github/` 沒碰。

## 起因

前一張卡 `v8-product-pvp-baseline` 在 Intel Mac 的 Docker Desktop 上量的 µs 級指標全距比都超過 10% 而停下。使用者決定先建立四個環境（Linux 實機原生 LN、Intel Mac 原生 MN、Mac 上的 Docker MD、Linux 實機上的 Docker LD）的實機對比基線，再給任務分級。

## 做了什麼

| 項 | 檔／位置 | 內容 |
|---|---|---|
| 計時底 | `tests/object_fps_pvp/TimerFloor.cpp`（新）、`CMakeLists.txt` | `gyo_object_fps_pvp_timer_floor`：10 萬對 `steady_clock::now()` 的差（p50／p95／p99／max）；不進 ctest。獨立執行檔，讓量測用的四個 ELF 保持位元組相同 |
| 重用核對 | 證據 `reuse-check.txt` | `eb018478`→master 的量測相關差異只有 `pvp_baseline_container.sh` 2 行；volume 內五個檔的 SHA-256 與前一張卡相同；映像 id 相同 |
| 投遞 | 證據 `bundle/` | `docker save` 的映像、volume 取出的 ELF＋映像的 libstdc++／libgcc_s、`run-linux.sh`（preflight／measure／pack；LN、LD 交錯） |
| Linux 量測 | 使用者的 Linux 實機（ssh；只在指定的工作資料夾內建檔） | 15:20～18:47：重放 2×10、載入 2×10、冷啟動 2×10、場次 2×10×600 s、計時底 2×10，項目前後記負載、PSI、頻率、溫度 |
| Mac M0 | 證據 `mac-m0/` | MN、MD 計時底各 3 次 |
| 分析 | 證據 `analyze.py`、`summary.json` | 對帳、§6 四種散佈量、LN↔LD 中位數比、§7 分級；MD／MN 全距比重算與前一張卡 14 格相同 |

## 驗證與經緯

1. **重用的根據**先查（使用者提示 #201 可看）：原始碼與二進位相同，Mac 兩欄重用，不重量（使用者核准 U-M1）。
2. **乾跑**：LN 路徑在 Mac 同映像容器、LD 路徑在 Mac 的 docker CLI 各跑短版；修了腳本 6 處（`/work` 在映像內是資料夾、LN-only 時 docker 缺席、`ldd` 顯示在 pipefail 下、EXIT trap 的區域變數、macOS 無 `/proc/cpuinfo`、macOS `ps` 無 `--sort`）。
3. **實機前提**：被量的 ELF 寫死 `/work/src`，LN 需要 `/work` 指向工作資料夾——使用者以 sudo 建了符號連結（我不執行 sudo）；`systemd-inhibit` 經 ssh 被 polkit 拒絕，AC 閒置不睡眠本來就是 `nothing`，以 `NO_INHIBIT=1` 跑，不改設定；preflight 通過（image id 相同、LD 內 5／5 SHA-256、`nproc` 8、`memory.max` 4 GiB）；實機上短版交錯再跑一次（輸出丟棄）。
4. **量測**：牆鐘 3 h 27 min；`MEASURE DONE`；收回 tar 的 SHA-256 遠端與本機相同。
5. **對帳**：四環境一致。分析腳本兩處寫錯，在看結論前修正：樣本檔把 long4 記作 `long4`（筆數核對一度全 0）；World 改回宣告的「LN、LD 與 MD 相同」（MN 用 libc++，密集欄 1,088 B 對 ELF 1,536 B）。
6. **發現**：(a) 前一張卡報告的容器 World 峰值 4,104 B 應為 4,552 B（4,104 是 Mac 的值；不改寫，報告開頭請架構師核）；(b) 量測開頭約 1～2 秒內 LD 暖機與 LN 第 0 次重放偏快（LN 第 0 次快約 36%；重放／載入／冷啟動裡只有它前後有核心到 4.5～4.7 GHz，其餘約 3.0 GHz；第一個項目 LN 暖機卻是慢的）——短暫 boost 的推測，旁證、機制未明。
7. **審查**（Sonnet／high 唯讀，獨立重算）：blocker 1／should 4／nit 7，全部處置。blocker 是 BU-6 的 d 誤用 BU-1 的 ×1.25，改回遷移計畫 §6 的 ×1.10（宣告 §7 本來就規定 d 取自 §6）。
8. **結果要點**：LN、LD 的 M1 四分位距比約 1%（全距比被 LN 第 0 次拉到 35～44%）；LD／LN 中位數比 0.97～1.02；MD 的四分位距比在多數指標是 LD 的 2.7～42 倍；照 §7 推法（相對倍數）：BU-1、冷啟動最便宜可做＝MN，BU-4 RSS＝MD，載入（BU-6 重放）＝LN（LD、MN 邊緣）。§6 的絕對項（＋0.25 ms、＋100 ms）讓 µs 級與載入的實際容許量大得多，交 U12。U8 提案與 BU-4 RSS 分母建議寫在報告 §6、§7，由使用者定。
9. **用時**：10:52～約 20:00，牆鐘約 9 h（估 7～9.5 h）。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/measure-env-baseline/`（本機 MacBookPro16,1；git 管理外）：`linux/`（收回的 tar `b35d26e0…` 與解開的原始檔、preflight／measure log）、`mac-m0/`、`bundle/`、`dryrun/`、`reuse-check.txt`、`analyze.py`、`summary.json`、`authority_golden-eb018478.txt`。MD、MN 的原始資料在同一主 checkout `build/evidence/pvp-baseline/`。Linux 實機的工作資料夾留著，由使用者決定刪不刪；`/work` 符號連結待使用者刪。
