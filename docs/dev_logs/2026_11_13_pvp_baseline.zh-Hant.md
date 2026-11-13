# PvP v8：v8-product-pvp-baseline（今天的 Match 量多少）

日期：2026-11-12～13。Owner：`object_fps_pvp`（執行者：架構師會話開的任務卡）。計畫：遷移計畫（`docs/architecture/plans/kernel/migration-plan.zh-Hant.md`，分支 `claude/arch-migration-plan`）§3.2；[事前宣告](../object_fps_pvp/plans/v8/v8-product-pvp-baseline-declaration.md)；[報告](../object_fps_pvp/plans/v8/pvp-baseline-report.zh-Hant.md)。

- 執行：實作者＝本會話（開卡時 Fable 5.1，開工後使用者切到 Opus 5.5；量測程式與統計 high，文件 medium）；審查者＝Sonnet／high 唯讀子代理（架構師指定：實作者 Opus、審查者換模型；下段純量測用 Sonnet）。
- 性質：只量、只記錄、不判。不改玩法、wire、digest、golden；`apps/`、`engine/`、`framework/`、`tests/common/`、`build/cmake`、`.github/` 都沒碰。

## 起因

遷移計畫的閘門 G2a／G2b 以「今天的 Match」當分母，但今天沒有任何量測，§6 預算表 BU-1～BU-8 的數字全是推定。本批量出分母，交架構師與使用者定預算。

## 做了什麼

| 項 | 檔 | 內容 |
|---|---|---|
| 計時掛點 | `tests/object_fps_pvp/AuthorityDigest.cpp` | 加 `PVP_DIGEST_TIMED_STEP(...)`（預設展開成原敘述；包住 `Duel::Tick` 的 `match_.Tick`、`RunHost` 的 `host.Advance`、`RunLink` 的 `match.Tick` 三處）與 `PVP_DIGEST_NO_MAIN`（定義時不編 `main`）。digest 執行檔不定義它們 |
| 重放量測 | `tests/object_fps_pvp/BaselineRecord.cpp`（新） | 定義兩個巨集後 `#include "AuthorityDigest.cpp"`，重放情境就是 digest 的情境。`--replay`：暖機一遍後計時 golden 37，每個 digest 對 fixture 驗證、計時次數對帳；再跑 4 人長情境 long4（暖機 600＋量測 3,600 tick），並以自定義的 friend `PvpMatchTestAccess` 讀各欄位元組。`--load`：讀 product arena→建 Match＋4 人 Join→第一個 Tick |
| CMake | `tests/object_fps_pvp/CMakeLists.txt` | `gyo_object_fps_pvp_baseline_record`，**不加 add_test**；L1 只編譯不執行 |
| 真實行程 | `tests/object_fps_pvp/pvp_baseline_session.py`（新） | `start`：spawn Match→ready 行；`session`：Match＋Gateway＋quad 探測（4 bot、600 s），彙整 Match 自己的統計行、每秒 `ps` RSS、逐窗對帳 |
| 容器 | `build/ci/linux-toolchain/Dockerfile`（新）、`tests/object_fps_pvp/pvp_baseline_container.sh`（新） | L1 linux-x64 同版本工具鏈（ubuntu 24.04＠digest、gcc-14、cmake 3.31.6、ninja 1.11.1、Python 3.12、Go 1.23）；`build` 階段建置＋digest ctest，`measure` 階段跑同一套 M1～M7 |

**這兩處改動是量測用**（架構師 2026-11-12 備查時指定）：`AuthorityDigest.cpp` 的兩個巨集與 `BaselineRecord.cpp`／`pvp_baseline_session.py`，由遷移批 `v8-product-match-sim` 接手時一併處理（改量新路徑或移除），不留成常態。

## 驗證與經緯

1. **偵察**：C++ `MaxPlayers = 4`（`Movement.hpp:21`）→計畫的 8 人長情境改成 4 人；真實行程的 `pace.advance` 正是計畫的口徑，但只有 `LateWakeStats` 的 10 格直方圖（250 µs 起跳）→百分位只能給格上界；`PvpMatch` 有 friend `PvpMatchTestAccess` 可讀欄大小；Linux 沒有實機也沒有 CI 入口。全部寫進宣告，架構師備查無異議。
2. **掛點不改行為**：`object_fps_pvp.authority_digest` ctest 通過；量測檔每次重複都驗證 golden 37 的 digest。
3. **試跑**（不算量測）：發現 Match end 行的 `authority_tick` 會被 IpcHost 在 Gateway 斷線時重置（試跑值 3），對帳改成「Advance 數對 60 Hz × 視窗秒數」，在量測前修訂宣告。
4. **第一次量測**：重放 5 次、載入 10 次、冷啟動 10 次、場次 5×600 s。件数の突き合わせ全一致。散佈：M1 p95 10.5%～37.6%、M3 25.9%、M7 24.5%（>10%）；RSS 9.4%。
5. **重量一次**（場次結束、負載降到約 2.5～3.4 後；方法與門檻不變）：散佈仍 >10%（M1 10.2%～38.4%、M3 32.0%、M7 22.0%），中位數與第一次相差 3% 內。照宣告 §5：M1、M3、M7「量不穩、不出基準」，停下回報。第一次的結果保留在證據與報告。
6. **修訂 1、2（架構師轉達 U8 的決定）**：Docker Linux 容器＝閘門用、Intel Mac＝開發參考。寫 Dockerfile 照 L1 工具鏈；容器內 digest ctest 通過（gcc-14 下 golden 37 與 Mac 相同）。容器組：重複 10 次、`--cpuset-cpus=8-15 --memory=4g`，2026-11-13 01:24～03:09（先等主機的 XProtect 背景掃描結束）。本機安靜組：同樣 10 次，03:10～04:54，不同時、不跑建置。
7. **兩組結果**：件数の突き合わせ全一致。容器組 M1 四組 12.8%～48.4%、M3 38.9%、M7 15.0%，只有 RSS 5.0% 過；本機安靜組 M1 host 8.7%、long4 4.5%、M7 2.7% 過，golden-all 10.1%、match 12.8%、M3 13.8%、RSS 42.9% 不過。照修訂 2：兩組都不穩＝「µs 級分母在此機器量不穩」，(b)／(c) 交使用者；容器內 RSS 逐指標過門檻，能否單獨當分母待使用者確認。

## 結果（第一組；Intel Mac 白天，參考值）

| 指標 | 值 |
|---|---|
| 重放 tick p95（golden 37 全體／match 組／host 組／long4；合成內容） | 3.23／1.58／4.40／3.39 µs（重量 3.17／1.58／4.29／3.35） |
| 真實行程 Advance p50／p95／p99 | ≤250／≤250／≤500 µs（格上界）；max 中位 17.8 ms；≥16 ms 共 10 次 |
| World 位元組（4 人） | 實體欄 1,696 B；含輸入與快取峰值 4,104 B |
| Match 穩態 RSS | 約 180 MiB（散佈 9.4%，可當基準） |
| 重放載入到第一個 Tick | 2.54 ms |
| 真實行程 spawn→ready | 1.19 s |

容器組與本機安靜組：

| 指標 | 容器內（閘門用） | 本機安靜組 |
|---|---|---|
| 重放 tick p95（golden-all／match／host／long4；合成內容） | 1.61／0.82／2.56／2.42 µs（散佈 16.7／27.8／48.4／12.8%） | 2.95／1.44／3.91／2.91 µs（10.1／12.8／8.7／4.5%） |
| 真實行程 Advance | p50／p95 ≤250、p99 ≤500 µs；max 中位 0.81 ms；≥16 ms 0 次 | p50／p95 ≤250 µs；max 中位 0.87 ms；≥16 ms 1 次 |
| Match 穩態 RSS | 30.3 MiB（5.0%，逐指標過門檻） | 164 MiB（42.9%） |
| 重放載入到第一個 Tick | 2.35 ms（38.9%） | 2.93 ms（13.8%） |
| 真實行程 spawn→ready | 1.50 s（15.0%） | 0.98 s（2.7%） |

停止理由與待決選項（(b) 10% 規則對 µs 級分母是否適用、(c) 改前提：Linux 實機或新宣告改量法）寫在報告開頭。

## 審查

**修訂後的複查**（Sonnet／high 唯讀子代理）：從原始 CSV／log 獨立重算容器組與本機安靜組的全部數字，與報告一致（差 0.001 的是中位數捨入）；判讀照修訂 2、未放寬規則；合成內容限界在開頭與 BU 表都有。must 1（「第 6 次重複四組 p95 都最高」與證據不符，實為 p50 四組最高、p95 只有 match 與 long4）、should 8（KiB／MiB 混用、主機側負載未排除、判讀條件寫成「兩組都不穩」原文、RSS 逐指標讀法要請使用者確認、補 (b) 的對立事實等），全部修在文件。

**第一組的審查**（Sonnet／high 唯讀子代理）：結論：報告的數字與統計都能從原始輸出重算（BU-1 各組、BU-4、RSS、BU-6、場次表全部一致），不需重量。must 2、should 5，全部修在文件：

- must：重放用的是合成內容（`PvpMatch` 的預設 stance、動畫規則、命中模型），不是 `match_main` 的產品組態→報告限界 10、BU-1／BU-4／BU-6（重放）列加註。
- must：`animations_` 的元素是 `AnimationTrack`（184 B），不是 `PlayerAnimation`（48 B）→報告改正；`BaselineRecord.cpp` 的輸出註解改印 `AnimationTrack`（量測後改；數字欄不受影響）。
- should：第 0、1 場 stall 多的歸因降為「未證實」；註明重放與真實行程的計時範圍不可互比；註明宣告 §5 負載記錄只做了一部分；寫明 World 的範圍；修正「557 發」、冷啟動範圍、「兩次中位數」表的措辭。
- 審查也確認：計時只包 authority step；host 情境每次 `Advance` 剛好一個 tick；巨集預設展開與原敘述逐字相同，`--check-golden` 37／0；`BaselineRecord.cpp` 在 Apple clang `-Wall -Wextra -Wpedantic` 下 0 警告（GCC／MSVC 未驗證，交 L1）。

## 用時

約 5.7 h（估 4～6 h；2026-11-12 23:20～11-13 04:56）。第一組約 1.8 h（建置配置與相依約 25 min、真實場次 55 min）；修訂後的容器建置約 30 min，容器量測 1.75 h 與本機安靜組 1.75 h 在背景。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/pvp-baseline/`（本機 MacBookPro16,1；git 管理外）：第一次的 `replay/`、`load/`、`real/`，重量的 `retry/`，容器組的 `container/`（輸出在 `container/m/`），本機安靜組的 `quiet/`，`machine.txt`、`worktree.diff`、`summarize.py`、`summary.json`（各組目錄內另有自己的 `summary.json`）。
