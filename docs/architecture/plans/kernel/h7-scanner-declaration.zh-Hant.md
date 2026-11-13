# 事前宣告：`v8-arch-h7-scanner`（H-7 呼叫點掃描器）

日期：2026-11-13。Owner：架構（架構師會話開出的任務卡）。狀態：**已核定**（架構師 2026-11-13，附 5 點修訂；修訂已併入本檔，見末節「核定與修訂」）。
出處：遷移計畫 `migration-plan.zh-Hant.md` §3.4、§3 共通（#199 已合併）；spec H-7、H.7、J.1、J.4；`target/kernel.zh-Hant.md` DI-KS 15（Host 不排 System 順序；已遷移的 host 只呼叫 `Run`、`Rewind`／`Replay` 與交換點）；J-17 先例（`j17-scanner.zh-Hant.md`「給 -zones 批」節、`tests/common/zone_scan/`、`tests/object_fps_pvp/zone-c-*.json` 與 `test_zone_c_lists.py`）；`guards-inventory.zh-Hant.md`；`target/zone-c-sources.zh-Hant.md` §5。
基準：origin/master `899b3e38`（#199、#201、#202 已併入）；分支 `claude/arch-h7-scanner`。

## 0. 模型／檔位（計畫 §3.4 與任務卡）

- 實作者：**Opus 5.5**／high（規則正規式與型別解析 xhigh）。本會話開卡時是 Fable，使用者為節省 Fable 用量改以 Opus 執行，依任務卡規定**審查者改為 Fable／high（只讀）**。
- 突變一覽的敵對檢查者（AGENTS §14.2、計畫 §3 共通第 3 點）：**Fable**／high（與一覽作者 Opus 不同、不是小模型）。**審查與敵對檢查合成一次 Fable／high 只讀**（架構師修訂 1：節制 Fable 用量），報告分兩節。
- §14.4 驗證者：本批不改既有判定，不出場。
- 段：下段（守衛工具，不改契約；計畫 §3.4）。

## 1. 放置（與計畫字面的一處差異）

| 物件 | 位置 | 說明 |
|---|---|---|
| 掃描器 | `tests/common/h7_scan/h7_scan.py` | 計畫寫「`plans/kernel/scripts/h7_scan.py`（或 `tests/common/h7_scan/` 內）」，spec H-7 寫白名單「寫在掃描腳本 `plans/kernel/scripts/h7_scan.py`」。採 `tests/common/h7_scan/`：與 zone_scan 同形、守衛不放 docs、ctest 從測試樹找得到。**spec H-7 的路徑字樣由架構師決定是否改（本批不改 spec）** |
| 規則資料（含白名單） | `tests/common/h7_scan/h7_scan_rules.json` | 白名單是資料、不寫在程式裡（spec「白名單寫在掃描腳本」的意圖是「白名單只有一份、在掃描器那邊」，資料檔滿足它） |
| 合成自測、突變 | `tests/common/h7_scan/test_h7_scan.py`、`mutations.json`、`run_mutations.py` | 只用合成檔；共通 Test 不前提任何 Product（AGENTS §7） |
| 共通 ctest | `tests/common/h7_scan/CMakeLists.txt`；`tests/common/CMakeLists.txt` **+1 行** `add_subdirectory(h7_scan)` | 兩個 ctest：`h7_scan.selftest`、`h7_scan.empty`（見 §5 判定 4） |
| owner 清單與註冊 | `tests/<owner>/h7-hosts.json`；該 owner 的 `tests/<owner>/CMakeLists.txt` 加一個 `add_test` | 五個 owner；內容 owner＝該產品或工具，執行者是本線 |

重用：遮罩（註解與字面值）、引號 include 的解析、路徑檢查**直接 import `tests/common/zone_scan/zone_scan.py` 的函式**（`blank_comments_and_literals`、`resolve_quoted_include`、`validate_path`），不複製第二份。zone_scan 模組找不到或缺這三個名字時，h7_scan 以結束碼 2 與明確訊息失敗、不靜默降級（自測一條；架構師修訂 2）；`tests/common/zone_scan/` 不留任何改動。兩者都在共通層，依賴方向不變；zone_scan 的遮罩若被改壞，h7 的遮罩自測也會失敗（多一道檢出，不是耦合問題）。

## 2. 規則的精確定義

前處理：每個被掃的檔先以 zone_scan 的遮罩把註解、字串與字元字面值換成空白（保留換行，行號不變）；兩條規則都只讀遮罩後的程式碼。

### H7-a：include 規則

- 判定：遮罩後的 `#include` 行（`<…>` 與 `"…"` 兩形），其路徑的**最後一段**等於規則資料 `H7a.headers` 的任一名（v1：`IRuntimeClient.hpp`、`RuntimeLoop.hpp`），大小寫不敏感（與 zone_scan ZS1 同；fail closed）。
- 範圍：清單的檔，加上沿引號 include 展開到 `owner_root` 之內的標頭（遞移、每檔一次，與 zone_scan 同法）；報告附含入鏈（`via`）。角括號 include 不展開。
- 以「最後一段」判而不是完整路徑：產品若自己另寫同名的 `RuntimeLoop.hpp` 也會被報（寧可誤報；H-7 的意圖是 host 不用這組動詞）。

### H7-b：呼叫點規則

- 呼叫形：遮罩後的程式碼中 `<receiver> (. | ->) <Method> [<template-args>] (`，`<Method>` ∈ 規則資料 `H7b.methods`（v1：`Tick`、`Advance`、`Update`、`Step`、`Run`；整字比對，`RunLiveFrame`、`UpdateTitle` 不算）。
- `<receiver>`：成員存取鏈的最後一個識別字（`session_.clock.Advance` 的 receiver 是 `clock`；`impl_->ui.Update` 是 `ui`）；`receiver` 是 `this`（`this->Run(`）→ 不算（host 呼叫自己的成員，不是模組的）；`receiver` 是呼叫結果或下標（`)`、`]` 結尾，例 `Get().Run(`、`v[i].Tick(`）→ 型別無法得知 → **報**（fail closed，標 `type=?`）。
- 不算的形：無 receiver 的呼叫（`Tick(x)`、`Run()`，host 自己的函式或自由函式）、限定名呼叫（`Foo::Run(`；含 `void Host::Tick() {` 這類定義）、宣告。寫進限界 L3。
- 範圍：**只有清單列的檔**（標頭不展開為 H7-b 的掃描對象；展開的標頭只用來找 receiver 的宣告型別）。host 的 inline 程式若寫在標頭，owner 要把該標頭列進清單（限界 L5）。

### 白名單（依型別，Python 的近似法）

規則資料 `whitelist`：`[{ "type": "<完整限定名>", "methods": [...] }]`。v1：

| type | methods | 出處 |
|---|---|---|
| `Engine::Runtime::FixedTickRuntime` | `Advance` | spec H-7、H.7 第 2 列（固定步累加器） |
| `GYO::Kernel::Runtime`（**暫定名**） | `Run`、`Rewind`、`Replay` | spec H-7、DI-KS 15；命名空間 `GYO::Kernel`（target/naming 第 1 條）。型別名未定：target/naming 第 5 條禁止新程式以 `Runtime` 當型別名尾，最後的名字一定不是它；定名的批（registry-schedule 或 world-exec）改規則資料一行與反例 ❷（限界 L7） |

判定一個 H7-b 呼叫是否在白名單（依型別、不依名字）：

1. **找宣告**：在「該檔＋其展開的 owner 標頭」的遮罩後程式碼中，找 receiver 名的變數／成員／參數宣告：`<Type> [*&]* <name> (; | { | = | ( | , | ))`，`<Type>` 是（可帶 `const`、`::` 限定、模板引數的）型別字樣。找到 `auto <name> = <other>;`（`auto`／`const auto`／`auto&`）時，把 `<other>` 當新 receiver 再找一次（**只追一跳**；`auto preview = ticks_;` 今天在 `LocalPlayerPrediction.cpp:372`）。
2. **剝殼**：`std::unique_ptr<T>`、`std::shared_ptr<T>`、`std::optional<T>`、`std::reference_wrapper<T>`、`T*`、`T&`、`const T` 剝成 `T`；其他模板（`std::vector<T>` 等）不剝。
3. **比對**：宣告型別 `D` 與白名單型別 `Q＝N1::…::Nk::T` 相符，當且僅當 (i) `D == Q`（可帶前導 `::`），或 (ii) `D` 是 `Q` 的帶至少一段限定的尾段（`Runtime::FixedTickRuntime`），或 (iii) `D == T`（未限定）且同一組掃描文字有 `using N1::…::Nk::T;` 或 `using namespace N1::…::Nk;`。方法也必須在該條目的 `methods` 裡（`FixedTickRuntime` 的 `Run` 仍報）。
4. **結論**：receiver 找到的**所有**宣告都相符 → 不報；**任一**不相符 → 報，並在文字欄附宣告型別；**找不到宣告**或只能追到第二跳 → 報（`type=?`）。同名 receiver 有多個宣告時採「全部相符才放行」。

所有近似都往「多報」那邊錯（fail closed）：會漏報的情形只有限界 L1、L3、L4（見 §7），其餘的不確定一律報。

## 3. owner 清單的資料格式（v1）

```json
{ "format": "gyo.h7-hosts", "version": 1,
  "generated_by": "manual", "repo_commit": "<sha>",
  "owner_root": "apps/object_fps_pvp",
  "include_roots": ["apps/object_fps_pvp/include"],
  "files": ["apps/object_fps_pvp/main.cpp", "..."] }
```

- 欄位與檢查照 zone-c-sources v1（路徑相對 repo 根、正斜線、排序、去重、檔案必須存在、`files` 必須在 `owner_root` 之下）；`format` 不同名，因為語義不同（host 檔 vs. (c) 檔）。未知 `format`／`version` 是契約錯誤（結束碼 2），不猜。
- **owner 清單不帶白名單**（與計畫 §3.4 字面「傳自己的檔案清單與白名單」的差異：比計畫字面更嚴的讀法，架構師核定 Q2；報告「與計畫的差異」記一行）：白名單只有規則資料一份。理由：owner 能自加白名單，就能把自己的模組型別白名單掉，守衛失去意義；spec H-7 的白名單只有 Kernel 型別。
- 契約寫在掃描器的 docstring（之後唯一長存的地方；本宣告是過程紀錄）；不開 `target/` 檔（架構師核定 Q3）。
- 輸出照 zone-c-sources §5 同形：stdout 一行一個發現 `<path>:<line>: <rule> <text>`，最後一行摘要 `h7_scan: <N> file(s) listed, <M> scanned (<M−N> expanded headers), <H> finding(s), <W> whitelisted`；`--report FILE` 寫 JSON（`list`、`files_listed`、`files_scanned`、`findings[{file,line,rule,text,via?,type?}]`、`whitelisted[...]`、`per_rule`、`expanded`、`unresolved_includes`）；結束碼 0 乾淨、1 有發現、2 契約或用法錯誤；`--no-fail` 把 1 改 0。
- `--expect-finding "<path>|<rule>|<regex>"`（可重複）：該發現不在報告中 → 結束碼 1（即使 `--no-fail`）。用途：owner 把「今天已知必報的點」釘住，防止清單或掃描器靜默壞掉；歸零時由 owner 自己拿掉。
- 不給 `--list` → 印 `h7_scan: nothing to scan`、結束碼 0。

## 4. 五個 owner 的清單 v1（host＝runner 的角色加產品 main，spec H.7 v0.3 註）

| owner | `files` | 理由 |
|---|---|---|
| object_fps_pvp | `main.cpp`、`match_main.cpp`、`src/Pvp/ClientConnection.cpp`、`src/Pvp/ClientSimulation.cpp`、`src/Pvp/ClientSimulationRole.cpp`、`src/Pvp/IpcHost.cpp`、`src/Pvp/MatchRuntimeHost.cpp`、`src/Pvp/PvpApplication.cpp`（8） | 兩個行程入口；H.7 表的多時鐘、執行緒角色、交換點、pump 所在。`ClientSimulation.cpp` 列入（架構師 Q4：-zones 把它歸 P5「Host 的時鐘」，spec L.3，手寫時鐘迴圈所在）。**不列** `LocalPlayerPrediction.cpp`（預測核心，不是 host） |
| voxel_map_editor | `src/Main.cpp`、`src/window/EditorWindow.cpp`、`src/window/PreviewMeasure.cpp`（3） | 入口、ImGui shell client、自己 `PumpEvents` 的量測模式（tool-hosts-inventory 發現 1） |
| model_viewer | `src/Main.cpp`、`src/ViewerWindow.cpp`、`src/PreviewMeasure.cpp`（3） | 同上 |
| audio_editor | `src/Main.cpp`、`src/AudioEditorWindow.cpp`（2） | 入口、shell client |
| ui_editor | `src/Main.cpp`、`src/EditorWindow.cpp`（2） | 入口、shell client（`EditorApp*.cpp` 是 Draw 的內容，不列；架構師同意） |

註冊：各 owner 的 CMake 加 `<owner>.h7_scan`（`--no-fail --report <binary>/h7_scan/h7-hosts.json`，標籤 `cpu` 加該 owner 既有的標籤）。**基線只報告不判**：命中數不是門檻。唯一的例外是 object_fps_pvp 以 `--expect-finding` 釘住計畫 §3.4 判定 (6) 的兩個正例：`MatchRuntimeHost.cpp` 的 `match_.Tick`、`ClientSimulationRole.cpp` 的 `loop_.Run`（這是計畫指定的「在產品自己的基線測試中必報」）。

## 5. 受理判定（看結果前宣告；看結果後不緩）

| # | 判定 | 成立的條件 |
|---|---|---|
| 1 | 合成自測 | `h7_scan.selftest`（`python -I -B -m unittest`）全綠，且下列每例逐一對應（正例：**只**被指定規則報、行號正確；反例：0 發現）。**正例 13**：H7-a ① `#include "engine/runtime/IRuntimeClient.hpp"` ② `#include <engine/runtime/RuntimeLoop.hpp>` ③ 經 owner 標頭間接 include（報在標頭行、帶 via）；H7-b ④ `foo_.Tick()`（`Product foo_;`）⑤ `loop_.Run()` ⑥ `p->Update()`（`std::unique_ptr<Product> p;`）⑦ `s.Step()` ⑧ `m.Advance()`（產品型別）⑨ `x.Run<int>()` ⑩ `FixedTickRuntime` 的 `Run`（方法不在白名單）⑪ 未限定 `FixedTickRuntime t;` 且無 `using` 的 `t.Advance` ⑫ 產品自己的 `game::Runtime r; r.Run()` ⑬ 找不到宣告的 receiver（`Get().Run()`）。**反例 11**：❶ `Engine::Runtime::FixedTickRuntime ticker_; ticker_.Advance(…)` ❷ Kernel `GYO::Kernel::Runtime rt; rt.Run()` ❸ `rt.Rewind()` ❹ `rt.Replay()` ❺ `auto preview = ticks_; preview.Advance(…)`（一跳別名）❻ `std::unique_ptr<Engine::Runtime::FixedTickRuntime> t; t->Advance()` ❼ `using Engine::Runtime::FixedTickRuntime; FixedTickRuntime t; t.Advance()` ❽ 註解與字串裡的 `x.Tick()`、`#include "RuntimeLoop.hpp"` ❾ `this->Run()`、無 receiver 的 `Tick(x)`、定義 `void Host::Tick() {` ❿ `app.RunLiveFrame()`（不在方法名單）⓫ `#include "engine/runtime/FixedTickRuntime.hpp"`。另加：未知 `format`／`version` 被拒（結束碼 2）、清單檔不存在被拒、`--expect-finding` 缺發現時結束碼 1、多宣告「全部相符才放行」各至少 1 例；其餘補充例在報告列數、不設上限 |
| 2 | 突變 | `tests/common/h7_scan/mutations.json`＋`run_mutations.py`（照 zone_scan：`-I -B`、清 `__pycache__`、只在預期測試名出現時算 killed，另計 survived／stale／wrong_failure／timeout）。至少 **16** 條：H7-a 刪 `IRuntimeClient.hpp`、刪 `RuntimeLoop.hpp`、停用標頭展開；H7-b 刪方法 `Tick`、刪 `Run`、去掉 `->` 形、去掉模板引數形；白名單判別：刪 `FixedTickRuntime` 條目、刪 Kernel 條目、不檢查方法、未限定名不要求 `using`、不剝 `unique_ptr`、不追 `auto` 一跳、找不到宣告改成放行、多宣告改成「任一相符就放行」；遮罩停用；版本檢查接受未知版本；`--expect-finding` 被忽略；`nothing to scan` 改成失敗。每條在一覽寫預期失敗的測試名。成立＝**全殺**（N/N）。突變一覽交 **Fable**／high 敵對檢查一次（找「改壞實作而自測不失敗」的處，加進一覽並確認被殺）；報告寫檢查者模型、找到處數、加入後的檢出數／總數 |
| 3 | 掃描方法 | 掃描器是 Python（`-I`），讀檔錯誤不吞（結束碼 2 並印路徑）；結束碼區分「0 發現」（0）與「失敗」（2）；本宣告與報告裡的獨立計數用 `git grep -nP`，不丟 stderr，用之前先以已知例（`MatchRuntimeHost.cpp` 的 `match_.Tick`）命中一次 |
| 4 | 共通層、無 owner | `h7_scan.empty`：不給清單執行 → stdout 含 `nothing to scan`、結束碼 0（ctest `PASS_REGULAR_EXPRESSION`）。`git diff --stat origin/master...HEAD` 中 `tests/common/` 之下既有檔只有 `tests/common/CMakeLists.txt` **+1 行**；`apps/`、`tools/`、`engine/`、`framework/` **0** 變更；每個 owner 的既有檔只動該 owner 的 `CMakeLists.txt` |
| 5 | Removability（合成假 owner） | 自測內建一棵合成 repo（兩個假 owner A、B，各有 host 檔與清單）：A、B 各自掃描的結果 → 刪除 A 的目錄與清單 → 共通層（掃描器、規則資料）位元組不變、B 的掃描結果與刪除前完全相同、`nothing to scan` 路徑仍綠。另在實際建置上：只選 voxel_map_editor 一個 owner 時（不建 object_fps_pvp），`ctest -R h7_scan` 共通兩個加該 owner 一個全綠——即刪去其他 owner 對共通層與彼此零影響 |
| 6 | 五個 owner 的基線 | 每個 owner 的 `<owner>.h7_scan` 在本機 passed，報告存證據目錄；object_fps_pvp 的 `--expect-finding` 兩點在報告中、`MatchRuntimeHost.cpp` 的 `ticker_.Advance`（`FixedTickRuntime`）在 `whitelisted` 而不在 `findings`。基線命中數**只報告**（給 match-host／client-frame／工具四批當歸零目標） |
| 7 | 件数の突き合わせ（每個 owner 各算） | (a) `H ＝ n(H7-a) ＋ n(H7-b)` ＝ findings 長度；(b) `M ＝ N ＋ expanded 長度`；(c) **獨立計數**：對清單的 N 檔，以 `git grep -nP '(?:\.|->)\s*(?:Tick|Advance|Update|Step|Run)\s*(?:<[^;()]*>)?\s*\('` 數「候選呼叫」C，須 `C ＝ n(H7-b) ＋ W ＋ 遮罩差 ＋ this 差`（遮罩差＝註解或字串裡的樣本、this 差＝`this->` 形，兩者逐行列出）；對 M 檔以 `git grep -nP '^\s*#\s*include\s*[<"][^>"]*(?:IRuntimeClient|RuntimeLoop)\.hpp[>"]'` 數，須等於 n(H7-a)（差只允許在遮罩差，逐行列）。全部相等＝「一致」；否則寫「不一致（…）」並停下不確定判定 6 |
| 8 | 三平台 | L1（linux-x64、windows-x64、macos-arm64）的 ctest 含 `h7_scan.selftest`、`h7_scan.empty` 與 L1 有建置的 owner 的 `<owner>.h7_scan` 全 passed（以 tests.xml 讀；Windows 上報告路徑是正斜線、UTF-8）。任一平台 skipped 或 failed＝不成立 |

## 6. 守衛與突變的去處（計畫 §3 共通，四選一）

| 對象 | 去處 | 說明 |
|---|---|---|
| 既有守衛 | **無**（本批不碰任何既有守衛；guards-inventory 21 列與補遺都不動） | — |
| `h7_scan` 掃描器、規則資料、合成自測 | **新增**（target 側：spec H-7 的守衛本體） | 突變一覽敵對檢查者 Fable／high |
| 五個 owner 的 `<owner>.h7_scan` 與 `h7-hosts.json` | **新增**（產品守衛，owner 持有；spec J.4） | 只報告；pvp 的兩個 `--expect-finding` 由 owner 在 match-host、client-sim／client-frame 批拿掉 |

## 7. 範囲の限界

- **L1 間接使用**：工具今天經 framework 的 `WindowShell`／`ImGuiShell` 間接用 `RuntimeLoop`（audit product-thirdparty §6 的 H-7 清單第 2 類）；host 檔本身不 include、也不呼叫，**H7-a、H7-b 都抓不到**。這一類由 framework 退役（plugins-foundation、工具四批、framework-delete）處理，判定靠那幾批的守衛（guards-inventory 2.10、2.11）。
- **L2 型別近似**：不是 C++ 解析。宣告找不到、二跳以上的別名、`decltype`、模板參數型別、命名空間別名（`namespace ER = Engine::Runtime;`）、經 `typedef`／`using X = …` 改名的型別，都當「不相符」而**報**（多報，不漏報）。
- **L3 不掃的呼叫形**：無 receiver（`Tick(x)`）、限定名（`Foo::Run(`）、函式指標與 `std::invoke`、巨集展開後的呼叫。今天 host 的已知手排點全是成員呼叫形，但限定名的靜態方法若被拿來排序，會漏。
- **L4 同名宣告的作用域**：只要 receiver 名的所有宣告都相符就放行，不分作用域；同名局部變數遮蔽成員且型別都在白名單時不影響結論，只有「另一個同名宣告也恰在白名單」時才會誤放（需要兩個白名單型別，今天不可能）。
- **L5 標頭裡的 host 程式**：H7-b 只掃清單列的檔；inline 寫在標頭的 host 程式要由 owner 把標頭列進清單。
- **L6 清單是手寫的**：host 檔漏列＝不掃；新 host 檔的登記責任在 owner。runner plugin 落地後（plugins-foundation）由該批把 plugin 的 runner 檔列進它自己的 owner 清單。
- **L7 白名單的 Kernel 條目是暫定名**：`GYO::Kernel::Runtime` 在 Kernel 定名前不會出現在任何程式裡，條目今天只對合成反例生效；定名批改資料一行。

## 8. 估時與停止

- 估時：掃描器與規則資料 2～3 h（型別近似 xhigh 段 1～2 h）、合成自測 1.5～2 h、突變與執行器 1 h、五個 owner 清單與 CMake 0.5～1 h、基線與突き合わせ 1 h、文件（dev_log、README、HANDOFF）1 h → **5～10 h**；守衛去處與突變一覽的敵對檢查 **1～2 h**。
- 停止：AGENTS §14 冒頭三條；單一問題累計 >2 h；總時數 >15 h（10 h×1.5）；L1 某平台失敗且原因不在本批。

## 9. 核定與修訂（架構師 2026-11-13）

- 送 parent 時 `send_message` 回「session parent not found」（第三次），改直送「[Job][架構師]例行架構掃描」會話；同日核定，附 5 點修訂，改完即開工、不再送。
- Q1：命名空間 `GYO::Kernel`；暫定 `GYO::Kernel::Runtime`，型別名未定（naming 第 5 條），定名批改資料（§2 表、L7）。
- Q2：owner 清單不帶白名單（比計畫字面更嚴）。Q3：契約寫在掃描器 docstring，不開 `target/`。
- Q4：pvp 清單加 `src/Pvp/ClientSimulation.cpp`，五個 owner 合計 **18** 檔；`--expect-finding` 仍只釘兩點。
- Q5：放 `tests/common/h7_scan/`；spec H-7 的路徑字樣由架構師下次碰 spec 時改，本批不改 spec，報告記一行差異。
- 修訂 1：審查與敵對檢查合成一次 Fable／high（§0）。修訂 2：zone_scan 缺失時結束碼 2 的自測、zone_scan 目錄零改動（§1）。
- 基準改為 origin/master `899b3e38`；出處加 DI-KS 15。
