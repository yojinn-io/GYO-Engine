# 交接卡：J-17 掃描器線（`v8-arch-j17-scanner`）

建立：2026-11-12（架構師會話）。Owner：**J-17 掃描器線**（新開的工具會話；本卡是它的計畫＋記錄器，現況節由該線自己更新）。
核心規格的修訂與 kernel 本體仍在架構師線；本線只做掃描器。架構師是本線 Architecture Delta 與資料契約的審核者。

## 為什麼現在開

spec J-17（`spec.zh-Hant.md` 守衛表）要求一個「(c) 區來源檔清單的掃描」：Kernel 用合成的 (c) System 檔自測，產品守衛讀 Kernel 匯出的清單並掃描。
它是後面每一批遷移的驗收工具（pvp 的「位置＝區」整理、Match／Client 的 host 遷移都要用它判「(c) 檔沒有讀時鐘、沒有全域狀態、沒有第三方全域 context」）。
掃描規則不依賴 kernel 的 API，只依賴清單格式；所以能先於 kernel 本體做。
-audit 時共用腳本 `docs/architecture/plans/kernel/scripts/audit_scan.py` 已跑過一次全範圍，但漏了幾類（下方規則 2、4 與第三方全域 context），而且它是一次性的審計腳本，不是守衛。

## 要做什麼（AGENTS §5：手動 → Script → Tool；本線停在 Script＋ctest 守衛）

### 1. 掃描規則（spec J-17 v0.3 原文，加 audit §5／§6 的漏掃項）

對清單裡的每個 (c) 來源檔（`.cpp`／`.hpp`／`.inl`），先把註解與字串字面值遮掉（規則 1、3、4 的第三方名稱也要掃註解外的程式碼；是否掃註解由本線決定並寫明），再掃：

1. **include 而非函式名**：`engine/time/MonotonicClock.hpp`、`<chrono>`、`<random>`、`<thread>`、`<atomic>`、`<mutex>`、`<condition_variable>`、`<future>`、`<ctime>`，以及 OS／平台標頭（`<windows.h>`、`<unistd.h>`、`<pthread.h>`、`<sys/*>`、SDL 標頭）在 (c) 檔一律違規（S-8：(c) 區除 `Diagnostics` 外不得有 effect，所以 OS 標頭出現就能抓）。(c) 檔的時間型別用整數 ns（`std::int64_t`），不 include `<chrono>`。
2. **匿名命名空間與命名空間範圔的非 const 變數**（不只 `static` 關鍵字）：`namespace { int counter; }`、`inline int x;`、`static int y;`、`thread_local`。排除 `constexpr`／`const`／函式宣告（參考 `tests/common/entity_store/entity_store_guard.py` 的 W4 判法，它已處理 `static int x(0);` 等邊界）。
3. **第三方全域呼叫**：`ImGui::`、`SDL_Get*`／`SDL_*State` 類全域查詢、`TTF_*`、`Mix_*`；清單要可擴充、放資料檔不放程式。
4. **時鐘別名**：`Time::Now`、`Time::NowNs`、`Clock::now`、`steady_clock`、`system_clock`、`high_resolution_clock`、`SDL_GetTicks*`、`SDL_GetPerformanceCounter`、`QueryPerformanceCounter`、`clock_gettime`、`gettimeofday`。
5. **候選列（人工確認）**：`std::function`／lambda 捕獲 `this` 並存入成員；pimpl 裡的跨 tick 裸指標（audit 漏掃的第二類）。候選不算失敗，只列出。
6. **亂數**：`<random>` 之外的 `rand(`／`srand(`、`random_device`、`mt19937`、`std::uniform_`。

每條規則一個代號（Z1～Z6）、一句話說明、違規的回報格式「檔:行 規則 文字」。規則的來源在 spec：J-17、S-8、B.5、G.2 Math、G-5、determinism.md Design Invariant 3（DI 3 的 fast-math／FMA 是建置旗標，不在本掃描器範圍，寫進限界）。

### 2. 清單的資料契約（Kernel ↔ 守衛；v1，架構師 2026-11-12 擬定，本線實作時可提修正）

Kernel 的 G-5 啟動報告之一是「(c) 區 System 來源檔清單」。kernel 本體還沒有，所以 v1 先定格式、由手寫或腳本產生（AGENTS §8：契約資料可由手工、Script、別的工具產生）：

```json
{ "format": "gyo.zone-c-sources", "version": 1,
  "generated_by": "manual | <tool name>", "repo_commit": "<sha>",
  "files": ["apps/object_fps_pvp/src/Pvp/Movement.cpp", "..."] }
```

- 路徑相對 repo 根、正斜線、排序、去重；只列來源檔，標頭由掃描器沿 include 展開到同一 owner 目錄內（展開深度與範圍由本線定、寫明）。
- 版本進格式；掃描器拒絕未知版本（明確錯誤，不猜）。
- 格式檔放 `docs/architecture/target/`（它是 Kernel 與守衛之間的契約）；本線寫草案，架構師核定後併入。

### 3. 放置與接法

- 掃描器與自測：`tests/common/zone_scan/zone_scan.py`、`test_zone_scan.py`、規則資料檔（第三方名稱清單）。接進 ctest 兩個測試：`zone_scan.selftest`（合成檔）與 `zone_scan.baseline`（見第 5 點；先只報告不判失敗）。接法照 `tests/common/entity_store/CMakeLists.txt` 第 23～25 行。
- 不進 `engine/`（Engine 不承擔工具職能）、不進 `tools/`（它是守衛，不是使用者工具）。kernel 本體落地後若要搬到 `tests/common/kernel/`，是檔案移動，不是 Delta。
- 共通層零修改：除新增目錄與 `tests/common/CMakeLists.txt` 加一行 `add_subdirectory` 外，不改任何既有檔。要改就停下回報架構師。

### 4. 自測（spec J-17 的「驗證方式」欄，照原文）

合成的 (c) 檔各放一種違規，每種都必須被抓：`static int`、`std::random_device`、匿名命名空間的 `int counter`、`Time::Now()` 呼叫、`ImGui::GetIO()`、`SDL_GetMouseState()`、把 `[this]` lambda 存進成員（最後一種列為候選）。
另加反例：`static constexpr`、`const` 全域、函式宣告、註解裡的 `ImGui::`、字串裡的 `<chrono>`、`std::int64_t` 的 ns 時間——都不得誤報。
突變：把掃描器每條規則各弄壞一次（刪規則、改正規式），對應的自測必須失敗（照 `tests/object_fps_pvp/mutations.json` 與 `run_mutations.py` 的做法，跨平台用 Python）。

### 5. 基線（報告，不是門）

用 audit §4.5 區表與 §1.3 判為 (c) 的 pvp 模組（`ClientSimulation`、`Movement`、`Combat`、`PvpMatch` 等——以 audit.zh-Hant.md 的表為準，本線據此手寫 v1 清單）跑一次，產出「檔、規則、行數」的報告存進證據目錄。
今天的 pvp 不是 (c) 合規的（spec J-17 自己寫了 `Combat.hpp`、`Movement.hpp` 的 `<chrono>` 型別用法要改），所以基線一定有命中；基線的用途是給 pvp「位置＝區」整理批當起點與驗收門檻（整理後命中要歸零）。
不改 pvp 的任何檔。

## 判定（看結果前宣告；開始實作前由本線補齊數字門檻，架構師核定）

1. 自測：7 種合成違規各被對應規則抓到，6 種反例零誤報；任一不成立＝不成立。
2. 突變：每條規則至少一個突變，全部 killed。
3. 共通層零修改（第 3 點）。
4. 基線報告：命中數與逐行清單存證據目錄；件数の突き合わせ寫「掃了幾檔、幾行、幾命中、每規則幾命中」。
5. 三平台：L1 跑 `zone_scan.selftest` 綠（Windows 的路徑分隔與編碼要過；參考 `portable-verification` 的做法）。

## 範囲の限界

- 靜態文字掃描；執行期才出現的繞路、經模板或巨集間接產生的呼叫、`using` 改名後的別名，掃不到——這些留給 A.6 的驗證層與審查。
- DI 3 的編譯旗標（fast-math、FMA、ISA intrinsic）不在本掃描器；由 CMake 層的守衛另做。
- v1 清單是手寫的；Kernel 的 G-5 匯出落地後再切換來源，格式不變（版本進格式）。

## 模型／檔位（建議；由該線的會話與使用者確認）

下段（新增守衛，不碰 Design Invariants 的條文與 kernel 本體；但規則的正規式與遮註解的邏輯是「気づかないうちに誤りやすい」）。

| 角色 | 模型／檔位 |
|---|---|
| 規則與遮罩的實作 | 主會話的模型／**xhigh**（只在正規式與遮罩這段） |
| 其餘（接 ctest、基線報告、文件） | 主會話的模型／high；定型作業 low |
| 審查者 | 與實作不同的模型／high，重點是誤報與漏報的反例 |

估時：規則與自測 4 小時、突變 1 小時、基線與接 ctest 1.5 小時、文件 1 小時。單一規則磨超過 2 小時就停下報告。
證據：主 checkout `build/evidence/zone-scan/`。

## 與其他線的接點

- 架構師：清單格式（第 2 點）要核定；掃描器若發現 spec J-17 規則本身有矛盾或漏洞，回報架構師改規格，本線不自行擴大規則。
- 產品線（之後的「位置＝區」整理）：用本線的基線報告當起點。
- 圖書管理員：dev_log 與本卡現況由本線寫；缺漏由圖書管理員點檢。

## 開場白（給使用者開會話時貼；會話確認職責後等「開始」）

> 你是 GYO-Engine 的 J-17 掃描器線。交接卡：`docs/architecture/plans/kernel/j17-scanner.zh-Hant.md`；規格出處 `docs/architecture/plans/kernel/spec.zh-Hant.md` 的 J-17、S-8、G-5；參考實作 `tests/common/entity_store/entity_store_guard.py`、`docs/architecture/plans/kernel/scripts/audit_scan.py`。先讀卡、補齊判定門檻與清單格式草案給架構師核定，再開始。工作分支從 origin/master 開，每批寫 dev_log，現況寫回本卡。

## 現況（本線更新）

- 2026-11-12 交接卡建立（架構師）。未開始。
