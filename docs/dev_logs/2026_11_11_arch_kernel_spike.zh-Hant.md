# 架構 v8：v8-arch-kernel-spike（C++ 能不能從簽名推出存取集合、能不能攔住繞路）

日期：2026-11-11。Owner：架構（kernel 研究線）。計畫：[kernel README](../architecture/plans/kernel/README.md) 批次表的 `-spike` 列（事前宣告）、[mechanism](../architecture/plans/kernel/mechanism.zh-Hant.md) §1 第 2 項與 §3.5。
報告：[spike-report.zh-Hant.md](../architecture/plans/kernel/spike-report.zh-Hant.md)。
- 執行：架構師會話開的子代理（Opus 5.5），在獨立工作樹 `.claude/worktrees/agent-a4ee79ef0e8af500e`（分支 `worktree-agent-a4ee79ef0e8af500e`，切自 master `1cff5413`）。只 commit，不 push、不開 PR；審查（README：Fable／xhigh）與 L1 由架構師接手。
- 性質：偵察。新目錄 `spikes/kernel/` 是獨立 CMake 專案，根 CMakeLists 不加入、不 include 任何 GYO 模組；不改現有程式、preset、契約。

## 起因

kernel 研究線要決定中間層（現有模組的更新介面與資料取得方式）逐模組「包」還是「重寫」，決定變數是 C++ 的契約可靠性（mechanism §1）。本批只回答兩題：(1) 能不能從 System 函式的參數型別在編譯期推出 `{reads, writes}`，不手寫第二份；(2) 受限句柄＋debug 建置的 World 存取核對，能不能攔住三種繞路。

## 做了什麼

- `spikes/kernel/include/kernel_spike/`（純標頭）：`Registry.hpp`（依登記順序配發 id；型別鍵是 per-type 可寫靜態物件的位址，只用來查表）、`World.hpp`（型別擦除的欄、resource、事件 channel、延後佇列；debug 存取核對與 System 邊界的位元組快照比對）、`Params.hpp`（七類句柄，不可複製、不可移動、建構子私有）、`Access.hpp`（函式型別拆參數、`AccessOf<F>`、aliasing 檢查、id 解析）、`System.hpp`（`Describe`、`MakeSystem`、`System::Run`）、`TypeList.hpp`、`Descriptor.hpp`、`Assert.hpp`（自己的最小斷言，可換處理器）。
- `spikes/kernel/tests/`：`AccessDerivationTests.cpp`（判定 1：17 個簽名的 `static_assert`＋執行期比對）、`AccessCheckTests.cpp`（判定 2：Bypass (a1)(a2)(b1)–(b3)(c1)–(c5)、Control、Limit、Always）、`RegistryWorldTests.cpp`。同一份原始碼建兩個目標：`gyo_kernel_spike_tests_debug`（核對開）與 `gyo_kernel_spike_tests_release`（`NDEBUG`，核對關）。
- `spikes/kernel/mutations.json`：32 條突變，照 `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py` 的格式。

## 受理判定（本機 Intel Mac，AppleClang 21.0.0，Debug 與 Release 兩組態）

- **判定 1（簽名推存取集合）：本機成立，三平台待 L1**。17 個簽名（free function 10、lambda 3、member function 3、function object 1）的型別集合 `static_assert` 與 id 集合執行期比對，都和手寫期望相同。Debug、Release 兩組態 × debug、release 兩目標，四個執行檔都是 30/30 case 通過（debug 目標 145 assertions、release 目標 155），clean 建置零警告。
- **判定 2（受限句柄＋debug 核對）：部分不成立**。(c1)–(c5)（經捕獲的 `World&` 讀寫未宣告的元件、resource、事件，或碰另一個 World）在存取當下斷言；(a1)(a2)(b1)–(b3)（存下的參考在別的 System 或 host 寫、System 內 `const_cast` 或裸位元組寫）在 System 邊界斷言；release 目標全部不攔、值被改。攔不到的形狀（寫了再寫回、經存下的參考讀、在有 `Write<T>` 的 System 經存下的參考寫、懸空、並行、resource 的堆積內容、全域狀態）各有一個 `Limit:` 測試固定現況。這是 mechanism §3.5 事前寫的上限；照 §14 記為不成立的部分，不補救。
- **判定 3（突變）：成立**。宣告移除 9 條（七類參數全覆蓋）第一輪全殺。全目錄第一輪 31/32、第二輪 32/32（**件數の突き合わせ：一致**——兩輪都是要求 32、結果 32，過期 0）。

## 經緯

- 原型一次編譯通過；4 個警告（mutable lambda 的變數只寫不讀、三個只出現在 `decltype` 裡的函式）→ 讓它們在 `MakeSystem` 的測試裡被用到，之後零警告。
- CMake 的 MSVC 選項原本寫成含空格的 generator expression（會被拆開）→ 改成分號分隔。沒有 MSVC 可驗證，L1 時看。
- 突變第一輪 `ks-check-write-accepts-read` 存活：(c3) 的寫入同時被存取當下核對與 System 邊界的位元組比對看到，測試只看「有沒有斷言」，第二層遮住了第一層的退化。處置：(c1)–(c5) 加「斷言時 System 本體沒有繼續」的檢查（debug 目標）與「本體跑完」的檢查（release 目標）。只收緊、不放寬；第一輪結果保留。
- 第二輪前兩處可攜性修正：型別鍵從 `static constexpr char` 改成可寫的 `static inline char`（避免 MSVC `/OPT:ICF`／lld `--icf=all` 把唯讀資料摺成同一位址）；`IsSystemFunction` 從變數模板裡的立即呼叫 lambda 改成 constexpr 函式。本機都沒有觀測到問題；因改了 `Registry.hpp`、`System.hpp`，第二輪跑全目錄。
- 估時 4 小時（上限 6 小時）；實際約 25 分鐘。沒有碰到 §14 的停止條件。

## 交給架構師的事

- L1 三平台：L1 目前不建 `spikes/kernel/`；要看 MSVC、gcc、AppleClang arm64，需要改 `.github/workflows/`（`spikes/` 以外，照 `v8-arch-kernel-delegation` 先給使用者）或實機手動跑。
- kernel README 的批次表與 HANDOFF 在分支 `claude/arch-kernel-mechanism`，本分支切自 master，沒有改這兩份（避免衝突）；完成記錄與 dev_log 的列入由架構師在合併時補。
- 判定 2 部分不成立：照 §14 停下回報，不補救。是否追加「頁保護」之類的下一個 spike，由架構師／使用者決定。

## 證據

主 checkout `build/evidence/kernel-spike/`（git 不追蹤；本機 Intel Mac）：`toolchain.txt`（編譯器、CMake、Ninja 版本）；`build_config-debug.txt`、`build_config-release.txt`（clean 建置，零警告）；`tests_config-{debug,release}_target-{debug,release}.txt`（四個執行檔的 doctest 輸出）；`ctest_config-debug.txt`、`ctest_config-release.txt`（突變後還原的樹再建再跑）；`test_cases.txt`（30 個 case 名）；`mutations_round1.txt`（31/32）、`mutations_round2.txt`（32/32）。建置樹在工作樹 `build/target/_build/kernel-spike-debug`、`kernel-spike-release`（git 不追蹤）。第一輪突變前的測試輸出只是開發中的執行，未保存。
