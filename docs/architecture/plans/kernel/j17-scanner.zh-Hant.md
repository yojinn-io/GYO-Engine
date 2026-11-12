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

1. **include 而非函式名**：`engine/time/MonotonicClock.hpp`、`<chrono>`、`<random>`、`<thread>`、`<atomic>`、`<mutex>`、`<condition_variable>`、`<future>`、`<ctime>`，以及 OS／平台標頭（`<windows.h>`、`<unistd.h>`、`<pthread.h>`、`<sys/*>`、SDL 標頭）、效果類標頭（`<fstream>`、`<filesystem>`、`<iostream>`、`<cstdio>`、`<stdio.h>`；架構師 2026-11-12 加，S-8：Diagnostics 走 `DiagnosticsWriter<T>`，不直接開串流；`<sstream>`、`<string>`、`<format>` 純記憶體不列）在 (c) 檔一律違規（S-8：(c) 區除 `Diagnostics` 外不得有 effect，所以 OS 標頭出現就能抓）。(c) 檔的時間型別用整數 ns（`std::int64_t`），不 include `<chrono>`。
2. **匿名命名空間與命名空間範圍的非 const 變數**（不只 `static` 關鍵字）：`namespace { int counter; }`、`inline int x;`、`static int y;`、`thread_local`。排除 `constexpr`／`const`／函式宣告（參考 `tests/common/entity_store/entity_store_guard.py` 的 W4 判法，它已處理 `static int x(0);` 等邊界）。**`const` 要落在變數本身**：`const char* names[]` 的 const 在指向物、陣列本身可寫（System 能重指 `names[i]`），算違規；正確寫法是 `const char* const names[]` 或 `constexpr std::array<const char*, N>`（架構師 2026-11-12 定，維持嚴格）。
3. **第三方全域呼叫**：`ImGui::`、`SDL_Get*`／`SDL_*State` 類全域查詢、`TTF_*`、`Mix_*`；清單要可擴充、放資料檔不放程式。
4. **時鐘別名**：`Time::Now`、`Time::NowNs`、`Clock::now`、`steady_clock`、`system_clock`、`high_resolution_clock`、`SDL_GetTicks*`、`SDL_GetPerformanceCounter`、`QueryPerformanceCounter`、`clock_gettime`、`gettimeofday`。
5. **候選列（人工確認）**：`std::function`／lambda 捕獲 `this` 並存入成員；pimpl 裡的跨 tick 裸指標（audit 漏掃的第二類）。候選不算失敗，只列出。
6. **亂數**：`<random>` 之外的 `rand(`／`srand(`、`random_device`、`mt19937`、`std::uniform_`。
7. **雜湊**（架構師 2026-11-12 加；spec B.5 的清單，W-10 同理：走訪順序不決定）：`std::hash`、`unordered_`。

每條規則一個代號（ZS1～ZS7；ZS＝zone scan，與 spec G.1 的 Kernel 區規則 Z1～Z8 區分，架構師 2026-11-12 定）、一句話說明、違規的回報格式「檔:行 規則 文字」。規則的來源在 spec：J-17、S-8、B.5、G.2 Math、G-5、determinism.md Design Invariant 3（DI 3 的 fast-math／FMA 是建置旗標，不在本掃描器範圍，寫進限界）。

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

## 判定（看結果前宣告；數字門檻由本線 2026-11-12 補齊，待架構師核定）

架構師原文五條保留；每條下面是本線補的數字門檻與判法。看結果後不緩。

1. **自測**（`zone_scan.selftest`＝`python -m unittest`）：
   - 正例 **9**（卡 §4 的七種＋架構師核定加的兩種）：每種一個合成檔，**只**被指定的規則抓到、行號正確（`static int` → ZS2、`std::random_device` → ZS6、匿名命名空間 `int counter` → ZS2、`Time::Now()` → ZS4、`ImGui::GetIO()` → ZS3、`SDL_GetMouseState()` → ZS3、`[this]` lambda 存進成員 → ZS5 候選且**不**計入違規、`std::unordered_map` → ZS7、`std::hash<>` → ZS7）。
   - 反例 **7**（卡 §4 的六種＋`std::map`）：`static constexpr`、`const` 全域、函式宣告、註解裡的 `ImGui::`、字串裡的 `<chrono>`、`std::int64_t` 的 ns 時間、`std::map`——各一個合成檔，**0** 違規、**0** 候選。
   - 另加每條規則各至少 1 個正例與 1 個反例（ZS1 的 include 清單逐項、ZS2 的 `inline int x;`／`thread_local`／類別靜態成員／`extern`、遮罩的邊界：區塊註解、raw string、數字分隔 `1'000`、include 行本身是字串）；數量在報告列出，不設上限。
   - 成立＝unittest 全綠且 9＋7 逐一對應；任一不成立＝判定 1 不成立。
2. **突變**（`tests/common/zone_scan/mutations.json`＋`run_mutations.py`，照 pvp 的做法）：至少 **9** 條——ZS1～ZS7 各 1（刪規則或改正規式）、遮罩 1（註解不遮）、清單版本檢查 1（接受未知版本）。killed 只算「`zone_scan.selftest` 失敗且輸出含該突變宣告的測試名」；survived／stale／wrong_failure 各自計數。成立＝**9/9 killed**（或實際條數全殺）。
3. **共通層零修改**：`git diff --stat origin/master...HEAD` 中，`tests/`、`engine/`、`framework/`、`apps/`、`tools/`、`build/` 之下**既有檔**的變更只有 `tests/common/CMakeLists.txt` 一個檔、**+1 行**（`add_subdirectory(zone_scan)`）；新增檔全在 `tests/common/zone_scan/`；`apps/object_fps_pvp/` **0** 變更。文件（本卡、target 草案、dev_log、kernel README、HANDOFF）不計。成立＝這三個數字成立。
4. **基線報告**（`zone_scan.baseline`，`--no-fail`，證據目錄 `build/evidence/zone-scan/baseline-<date>/`；**兩份清單各一份報告**：`confirmed.json`、`candidates.json`，架構師 2026-11-12）：每份報告寫「清單 N 檔、展開後 M 檔、L 行、H 違規、C 候選、每規則 n(ZS1)…n(ZS7)」。件数の突き合わせ三條、兩份各算：(a) `H ＝ Σ n(ZSk)` ＝ 違規行數；(b) `M ＝ N ＋ 展開標頭數`，與報告 `expanded` 清單長度一致；(c) **獨立計數**：對同一組 M 檔，以 `git grep -nP` 各算一次 ZS1 的 `^\s*#\s*include\s*<(chrono|random|thread|atomic|mutex|condition_variable|future|ctime)>` 與 ZS4 的時鐘別名，件數須等於報告的 n(ZS1) include 子集與 n(ZS4)（差異只允許在遮罩差——註解或字串裡的樣本——且逐行列出）。三條皆一致＝「一致」；否則寫「不一致（…）」並停下不確定判定 4。基線有命中是預期（spec J-17 自己點名 `Combat.hpp:5`、`Movement.hpp:8`），**命中數不是門檻**。
5. **三平台**：L1（linux-x64、windows-x64、macos-arm64）的 ctest 都含 `zone_scan.selftest` passed 與 `zone_scan.baseline` passed（**3/3 平台、2/2 測試**）；Windows 上 baseline 報告的路徑全是正斜線、檔案以 UTF-8 讀寫（在 CI 的 tests.xml 讀印出值）。任一平台 skipped 或 failed＝不成立。

## 基線清單 v1（手寫；依 audit §1.3 的 zone 欄與 §4.5；架構師 2026-11-12 核定，P9 改為第二份「候選」清單）

| 來源檔（`apps/object_fps_pvp/`） | audit 列 | zone 欄 | 列入 |
|---|---|---|---|
| `src/Collision/CharacterCollision.cpp` | §1.3 `src/Collision` | C | 是 |
| `src/Gameplay/Player/PlanarMovement.cpp` | §1.3 `src/Gameplay` | C | 是 |
| `src/Pvp/PvpMatch.cpp` | P1 | C | 是（展開到 `PvpMatch.hpp` → `Combat.hpp`、`Movement.hpp`、`MovementTrace.hpp`、`MovementDiag.hpp` 等） |
| `src/Pvp/ClientSimulation.cpp` | P5 | Host 時鐘（B）；內部跑 (c) 預測 | 是（卡 §5 點名） |
| `src/Pvp/ClientSimulationRole.cpp` | P5 | 同列；RoleThread 與 `Body` 是 Host | **否**（Host 執行緒；(c) 的部分在 `ClientSimulation.cpp`） |
| `src/Pvp/LocalPlayerPrediction.cpp` | P6 | C（PredictStep）＋B（TrackPhase） | 是 |
| `src/Pvp/Movement.cpp` | P7 | C | 是 |
| `src/Pvp/PlayerAnimation.cpp` | P9 | C（推進進 digest） | 是 |
| `src/Pvp/ShotQuery.cpp`、`ShotGate.cpp`、`PlayerHitModel.cpp`、`PlayerHitbox.cpp`、`PlayerPose.cpp` | P9 | 中間地帶（DET-1：E1 前明示為 (b)） | **候選清單**（架構師：遷移批要翻成 (c) 的候選，不靜默排除；第二份清單檔，報告分開） |
| `src/Pvp/Arena.cpp`、`ArenaMesh.cpp`、`MovementRulesFile.cpp`、`PlayerAnimationContent.cpp`、`PlayerAnimationDigest.cpp` | P13 | C（內容身分、契約資料） | 是 |
| `src/Pvp/NativeFiles.cpp` | P13 | 包（`Io:file`，(b) System 或 Host） | **否** |
| `src/Pvp/MatchRuntimeHost.cpp` | P2 | 混合：Host（B）＋Ingress（B）＋Judge（(c)） | **否**（Judge 是 Host 檔內的一段函式；拆成 System 後再列。列入會讓基線被 Host 的時鐘命中淹沒） |
| 其餘 `src/Pvp/*.cpp`、`src/App/*.cpp` | P2～P4、P8、P10～P12、`src/App` | B | 否 |

結果：已確認的 (c) 清單 **12** 檔、候選清單 **5** 檔。兩份都 `owner_root = apps/object_fps_pvp`、`include_roots = [apps/object_fps_pvp/include]`、`generated_by = "manual"`。
清單檔放 `tests/common/zone_scan/baseline/object_fps_pvp.zone-c-sources.json` 與 `object_fps_pvp.zone-c-candidates.json`（守衛持有；pvp 不改）。`zone_scan.baseline` 跑兩份、都 `--no-fail`，報告分開存。

## 提給架構師的問題與處置（2026-11-12 送出；同日架構師核定，處置在每題末尾的「→」）

1. **規則代號衝突**：spec G.1 已用 `Z1～Z8` 指 Kernel 的區規則（Z4＝無 effect、Z6＝rollback）；本卡的掃描規則也叫 `Z1～Z6`。同一份報告裡「Z4」會有兩個意思。（建議改用 `ZS1～ZS6`，ZS＝zone scan；本卡與 target 草案的代號隨決定改。核定前本卡沿用 Z。）→ **採 ZS1～ZS7**；spec G.1 不動。
2. **`std::hash`**：spec B.5 的 (c) 檔掃描清單含 `std::hash`，spec 修訂記錄第 1389 行也寫「五條規則是對 B.5 既有清單（`thread_local`、`std::hash`、`rand`）的追加，不取代」，但卡的六條沒有它。（建議加第七條「雜湊」：`std::hash`、`unordered_`；entity_store 守衛 W2 已有同樣的正規式可借。）→ **加 ZS7**；自測正例加 `std::unordered_map`、`std::hash<>`，反例加 `std::map`；突變至少 9 條。
3. **效果類標頭**：S-8 說 (c) 除 Diagnostics 外不得有 effect，卡的 Z1 只列時鐘、執行緒、OS 與 SDL 標頭；`<fstream>`、`<filesystem>`、`<iostream>` 是檔案與串流效果。（建議加進 ZS1 的資料檔；不加也可，但要寫進範囲の限界。）→ **加進 ZS1**：`<fstream>`、`<filesystem>`、`<iostream>`、`<cstdio>`、`<stdio.h>`；`<sstream>`、`<string>`、`<format>` 不列。
4. **清單格式加兩欄**：`owner_root`、`include_roots`（target 草案 §2）。沒有它們，掃描器解析不了 `"RetroFPS/Pvp/Movement.hpp"` 這種引號 include；從 CMake 讀是工具職能，不做。→ **核定**。
5. **註解不掃**：六條規則都在遮掉註解與字串之後掃（卡 §1 留給本線決定）；理由是卡 §4 的反例「註解裡的 `ImGui::`」要求零誤報。→ **核定**；遮罩是突變對象。
6. 基線清單的四個「否」是本線的判讀，請確認。→ `ClientSimulationRole`、`NativeFiles`、`MatchRuntimeHost` 三個「否」**同意**；P9 五檔**不靜默排除**，改第二份候選清單（同格式 v1），`zone_scan.baseline` 跑兩份、報告分開、件数の突き合わせ各算。另：`repo_commit` 不比對 HEAD、`files` 必須存在、展開標頭附含入鏈，都保留。

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

## 給 -zones 批（產品線「位置＝區」整理；架構師 2026-11-12 定）

- **基線清單搬家**：`tests/common/zone_scan/baseline/object_fps_pvp.zone-c-sources.json` 與 `object_fps_pvp.zone-c-candidates.json` 搬到 `tests/object_fps_pvp/`，由產品的 CMake 註冊基線測試（呼叫 `tests/common/zone_scan/run_baseline.py --lists …` 或直接 `zone_scan.py --list …`）；共通層只留掃描器、規則資料、自測與 `run_mutations.py`，`run_baseline.py` 在 `baseline/` 空時報「nothing to scan」並通過。這是 AGENTS §7（刪 Product 不改共通層）與 spec J.4（產品端掃描由 owner 持有）的要求；本批放共通目錄只因 pvp 零變更是硬規則。
- **驗收門檻**：整理後確認清單的違規歸零（本批基線 26；候選清單 9 是 E1 前的參考值，不是門檻）；清單的 `repo_commit` 跟著更新。
- **`const char* names[]` 類**：照嚴格讀法改成 `const char* const names[]` 或 `constexpr std::array`（一行修法）。
- **掃描器的已知限界**（dev_log「範囲の限界」）：識別字引數的括號初始化讀成函式、多重宣告子只看最後一個 const、大小寫不敏感檔案系統的 include 拼法；碰到時以人工補看，不在 -zones 批改規則（規則變更回本線或架構師）。

## 與其他線的接點

- 架構師：清單格式（第 2 點）要核定；掃描器若發現 spec J-17 規則本身有矛盾或漏洞，回報架構師改規格，本線不自行擴大規則。
- 產品線（之後的「位置＝區」整理）：用本線的基線報告當起點。
- 圖書管理員：dev_log 與本卡現況由本線寫；缺漏由圖書管理員點檢。

## 開場白（給使用者開會話時貼；會話確認職責後等「開始」）

> 你是 GYO-Engine 的 J-17 掃描器線。交接卡：`docs/architecture/plans/kernel/j17-scanner.zh-Hant.md`；規格出處 `docs/architecture/plans/kernel/spec.zh-Hant.md` 的 J-17、S-8、G-5；參考實作 `tests/common/entity_store/entity_store_guard.py`、`docs/architecture/plans/kernel/scripts/audit_scan.py`。先讀卡、補齊判定門檻與清單格式草案給架構師核定，再開始。工作分支從 origin/master 開，每批寫 dev_log，現況寫回本卡。

## 現況（本線更新）

- 2026-11-12 交接卡建立（架構師）。未開始。
- 2026-11-12 J-17 線開工（分支 `claude/arch-j17-scanner`，自 `2b4618f9`）。讀完卡、spec §G／J-17／J.4／B.5、audit §1.3／§4.5／§5／§6、三份參考實作。補齊「判定」的數字門檻、寫基線清單 v1（12 檔）、寫清單格式 v1 草案 `docs/architecture/target/zone-c-sources.zh-Hant.md`、列六個問題，送架構師核定。**核定前不寫程式。**
- 2026-11-12 架構師核定（六題處置：ZS1～ZS7、加 ZS7 雜湊、效果類標頭進 ZS1、兩欄核定、遮罩核定、P9 改候選清單）。卡與 target 草案照改；開始實作。
- 2026-11-12 實作完成（分支 `claude/arch-j17-scanner`；dev_log [2026_11_12_arch_j17_scanner](../../../dev_logs/2026_11_12_arch_j17_scanner.zh-Hant.md)）。判定：1 自測 **成立**（65 條綠；9 正 7 反逐一對應）；2 突變 **成立**（18/18 killed；第一輪 13/16 的原因是 Python bytecode 快取與一條等價突變，已修）；3 共通層 **成立**（既有檔只動 `tests/common/CMakeLists.txt` +1 行，`apps/` 0）；4 基線 **成立**——確認清單 12 檔→40 檔 4,949 行 26 違規（ZS1 19、ZS2 3、ZS4 2、ZS7 2）、候選清單 5 檔→20 檔 2,353 行 9 違規（ZS1 6、ZS7 3），件数の突き合わせ兩份各三條**一致**（`git grep -nP` 的 ZS1／ZS4 獨立計數逐行相等）；5 三平台待 L1。基線第一輪的 ZS2 誤報（`enum class`、`const T &name`）已修並補測試。證據：主 checkout `build/evidence/zone-scan/`。審查者（Opus／high，只讀）15 項：should 8 全修、nit 修 5、2 項寫進限界；政策點「`const char* names[]` 算可變」架構師定維持嚴格（dev_log 經緯 4）。PR #192。
- **壓力記錄**：兩份基線清單住在共通目錄，刪 pvp 要連帶刪它們（AGENTS §7；spec J.4 說產品端掃描由 owner 持有）。架構師 2026-11-12 同意由 -zones 批搬到 `tests/object_fps_pvp/`（上方「給 -zones 批」節）。
- 2026-11-12 本卡結束：**等 L1 與合併**（PR #192；L1 綠後把 run id 補進 dev_log 判定 5）。
- 2026-11-12 `-zones` 批（`v8-product-kernel-prep-zones`）把兩份基線清單搬到 `tests/object_fps_pvp/`（加第三份 `zone-c-rewrite.json`），由產品的 CMake 以 `zone_scan.py --no-fail` 註冊三個報告 ctest，門在產品的 `test_zone_c_lists.py`（殘留精確比對）；本目錄刪 `baseline/` 與 `zone_scan.baseline`，`run_baseline.py` 保留。確認清單 26→7（[dev_log](../../../dev_logs/2026_11_12_pvp_kernel_prep_zones.zh-Hant.md)）。
