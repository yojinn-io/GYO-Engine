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

**這兩處改動是量測用**（架構師 2026-11-12 備查時指定）：`AuthorityDigest.cpp` 的兩個巨集與 `BaselineRecord.cpp`／`pvp_baseline_session.py`，由遷移批 `v8-product-match-sim` 接手時一併處理（改量新路徑或移除），不留成常態。

## 驗證與經緯

1. **偵察**：C++ `MaxPlayers = 4`（`Movement.hpp:21`）→計畫的 8 人長情境改成 4 人；真實行程的 `pace.advance` 正是計畫的口徑，但只有 `LateWakeStats` 的 10 格直方圖（250 µs 起跳）→百分位只能給格上界；`PvpMatch` 有 friend `PvpMatchTestAccess` 可讀欄大小；Linux 沒有實機也沒有 CI 入口。全部寫進宣告，架構師備查無異議。
2. **掛點不改行為**：`object_fps_pvp.authority_digest` ctest 通過；量測檔每次重複都驗證 golden 37 的 digest。
3. **試跑**（不算量測）：發現 Match end 行的 `authority_tick` 會被 IpcHost 在 Gateway 斷線時重置（試跑值 3），對帳改成「Advance 數對 60 Hz × 視窗秒數」，在量測前修訂宣告。
4. **第一次量測**：重放 5 次、載入 10 次、冷啟動 10 次、場次 5×600 s。件数の突き合わせ全一致。散佈：M1 p95 10.5%～37.6%、M3 25.9%、M7 24.5%（>10%）；RSS 9.4%。
5. **重量一次**（場次結束、負載降到約 2.5～3.4 後；方法與門檻不變）：散佈仍 >10%（M1 10.2%～38.4%、M3 32.0%、M7 22.0%），中位數與第一次相差 3% 內。照宣告 §5：M1、M3、M7「量不穩、不出基準」，停下回報。第一次的結果保留在證據與報告。

## 結果（參考值；Intel Mac 代理）

| 指標 | 值 |
|---|---|
| 重放 tick p95（golden 37 全體／match 組／host 組／long4；合成內容） | 3.23／1.58／4.40／3.39 µs（重量 3.17／1.58／4.29／3.35） |
| 真實行程 Advance p50／p95／p99 | ≤250／≤250／≤500 µs（格上界）；max 中位 17.8 ms；≥16 ms 共 10 次 |
| World 位元組（4 人） | 實體欄 1,696 B；含輸入與快取峰值 4,104 B |
| Match 穩態 RSS | 約 180 MiB（散佈 9.4%，可當基準） |
| 重放載入到第一個 Tick | 2.54 ms |
| 真實行程 spawn→ready | 1.19 s |

停止理由與三個待決選項（新宣告重量／先看 10% 門檻對 µs 級分母的意義／改到 Linux 或更安靜的機器）寫在報告開頭。

## 審查

Sonnet／high 唯讀子代理。結論：報告的數字與統計都能從原始輸出重算（BU-1 各組、BU-4、RSS、BU-6、場次表全部一致），不需重量。must 2、should 5，全部修在文件：

- must：重放用的是合成內容（`PvpMatch` 的預設 stance、動畫規則、命中模型），不是 `match_main` 的產品組態→報告限界 10、BU-1／BU-4／BU-6（重放）列加註。
- must：`animations_` 的元素是 `AnimationTrack`（184 B），不是 `PlayerAnimation`（48 B）→報告改正；`BaselineRecord.cpp` 的輸出註解改印 `AnimationTrack`（量測後改；數字欄不受影響）。
- should：第 0、1 場 stall 多的歸因降為「未證實」；註明重放與真實行程的計時範圍不可互比；註明宣告 §5 負載記錄只做了一部分；寫明 World 的範圍；修正「557 發」、冷啟動範圍、「兩次中位數」表的措辭。
- 審查也確認：計時只包 authority step；host 情境每次 `Advance` 剛好一個 tick；巨集預設展開與原敘述逐字相同，`--check-golden` 37／0；`BaselineRecord.cpp` 在 Apple clang `-Wall -Wextra -Wpedantic` 下 0 警告（GCC／MSVC 未驗證，交 L1）。

## 用時

約 1.8 h（估 4～6 h）。其中建置配置與相依約 25 min，真實場次 55 min（背景）。

## 證據

主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/pvp-baseline/`（本機 MacBookPro16,1；git 管理外）：第一次的 `replay/`、`load/`、`real/`，重量的 `retry/`，`machine.txt`、`worktree.diff`、`summarize.py`、`summary.json`。
