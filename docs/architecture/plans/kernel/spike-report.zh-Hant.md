# Spike 報告：C++ 的契約可靠性（`v8-arch-kernel-spike`）

更新：2026-11-11（對抗檢查 v8-arch-kernel-spike-adversary 的處置已併入）。Owner：架構（架構師會話）；執行：架構師會話開的子代理（Opus 5.5）。**性質：偵察，不是 Architecture Delta；不改現有程式與契約。**
根據：[mechanism.zh-Hant.md](mechanism.zh-Hant.md) §1 第 2 項、§3.5；事前宣告：[README.md](README.md) 批次表的 `-spike` 列（判定照原文，看結果後不放寬）。
程式：`spikes/kernel/`（獨立 CMake 專案，不進任何 GYO target，不 include 任何 GYO 模組；只沿用 `third_party/doctest`）。

## 結論（先讀）

| 判定 | 結論 | 證據 |
|---|---|---|
| 1 簽名推存取集合 | **本機成立，三平台待 L1**。17 個不同簽名（free function 10、lambda 3、member function 3、function object 1），型別集合 `static_assert` 與 id 集合執行期比對都和人手寫的期望逐一相同。本機只有 AppleClang 21（x86_64），Debug 與 Release 兩種組態都過。MSVC、gcc、AppleClang arm64 沒有實測。 | `spikes/kernel/tests/AccessDerivationTests.cpp`；`build/evidence/kernel-spike/` 的 `tests_*.txt` |
| 2 受限句柄＋debug 核對攔繞路 | **部分不成立**。(c) 成立：經未宣告的 id 碰 World，在存取當下斷言。(a)(b) 只對「寫入的位元組淨變化留到 System 邊界」的形狀成立：debug 斷言、release 不攔。攔不到的形狀見〈判定 2 的上限〉，C++ 在這套機制裡沒有辦法攔。 | `spikes/kernel/tests/AccessCheckTests.cpp`（Bypass／Limit／Control 各案） |
| 3 突變 | **成立**。事前宣告的「移除任一宣告→對應測試失敗」：9 條宣告移除突變（七類參數全覆蓋）第一輪就全部被殺；其中 7 條由 debug 核對層殺（6 條在存取當下、1 條在 System 邊界），另 2 條由 const 正確性（編譯錯誤）與判定 1 的期望 `static_assert` 殺。全目錄：第一輪 31/32（1 條存活，屬核對層的偵測力，不是宣告移除），收緊測試後第二輪 32/32；對抗檢查後加到 34 條，第三輪 34/34。 | `spikes/kernel/mutations.json`；`build/evidence/kernel-spike/mutations_*.txt` |

表的註（對抗檢查後；判定 1、2 的結論措辭待使用者決定 R-01、R-06，不改）：
- 判定 1：R-08 加了非 const、noexcept 的成員函式（`Steering::Prepare`），簽名現為 18 個（member function 4）。
- 判定 2：「(c) 在存取當下斷言」**僅限 System 自己的執行緒**。執行中的 System 是 `thread_local`，System 另起的執行緒對 World 的存取一律當成 host，不比對描述（R-03，見〈判定 2 的上限〉）。

判定 2 的「部分」是 mechanism §3.5 事前就寫了的上限（拿到參考就能存指標），不是本批新發現的缺陷；依 AGENTS §14 照實記為不成立的部分，不補救、不加 workaround。對「中間層包或重寫」的意義寫在〈對「中間層包或重寫」的意義〉。

## 停止與需要的決定（AGENTS §14）

- **停止理由**：判定 2 部分不成立。事前宣告「任一不成立＝結論做不到，照 §14 停下回報，不補救」。本批到此為止，沒有為了判定 2 加任何手段。
- **用的時間**：約 25 分鐘（估 4 小時）。
- **知道了什麼**：簽名推存取集合在本機做得到，且 17 種簽名、七類參數、編譯期與執行期兩層都和手寫期望相同。debug 核對對「System 自己的執行緒經 World 的存取」是精確的（存取當下攔；其他執行緒的存取當成 host）；對「不經 World 的寫入」只能在 System 邊界看淨變化；對「不經 World 的讀取」完全看不到。
- **下一次試行會改變的判斷**：如果再做頁保護（見〈判定 2 的上限〉末段），「寫了再寫回」與「經存下的參考讀」也能在 debug 攔到，`-audit` 對「持有跨 tick 指標的模組」的判斷會從「只能審查」變成「debug 可攔」；全域狀態的判斷不變。
- **選項**（架構師／使用者決定）：
  1. 接受上限：照 mechanism §3.5 的三層（API 讓繞路很醜＋debug 核對＋審查）前進，`-audit` 的第二問改成可檢查的兩問（是否持有跨 tick 的指標或參考、是否有全域可變狀態）。
  2. 追加一個頁保護的 spike（本批範圍外，要新的事前宣告；代價見〈判定 2 的上限〉）。
  3. 重看計畫前提（改範圍）：把 spike 要回答的問題改成「經 World 的存取能不能攔」，承認不經 World 的存取只能靠審查。這是改計畫的問題，不是放寬本批判定；本批的紀錄維持「判定 2 部分不成立」。
  4. 不論選哪個：判定 1 的三平台要 L1，L1 目前不建 `spikes/kernel/`，接上要改 `.github/workflows/`（`spikes/` 以外，先給使用者）。

## 原型做了什麼

- **Registry**：`ComponentId`／`ResourceId`／`EventId` 依登記順序配發（第 n 個登記拿 n）。型別只用一個 per-type 靜態物件的位址當查表鍵，**鍵不是 id**；查一個沒登記的型別是 Programmer Error（斷言），不在首次使用時配發。名稱由登記時傳入，不用 RTTI。
- **參數型別七類**：`Read<T>`、`Write<T>`、`Res<T>`、`ResMut<T>`、`EventReader<T>`、`EventWriter<T>`、`Deferred`。每類同時是宣告與存取：`ParamAccess<P>` 給出它讀寫什麼，P 本身是那份儲存唯一的句柄。
- **`SystemDescriptor { reads, writes }`**：由函式型別推出，free function、lambda（含捕獲、mutable）、function object、member function（含 const、noexcept）都做。沒有從手寫描述建 System 的路（`System` 的建構子是私有的）。
- **World**：只有型別擦除的欄（`TypeOps`：size／align／move／destroy，加位元組）、resource 表、事件 channel、延後佇列；欄依 row index 疊代。
- **debug 存取核對**（`KERNEL_SPIKE_ACCESS_CHECKS`，預設跟 `NDEBUG` 反向；release 為 0）：
  1. 執行中的 System 的描述放在 `thread_local`；System 執行期間、在**同一個執行緒**上經 World 的每次存取都比對描述，未宣告的 id 斷言；碰到另一個 World 也斷言。讀的權限是「宣告讀或宣告寫」，寫的權限只有「宣告寫」。其他執行緒沒有執行中的 System，存取被當成 host。
  2. 每個 System 執行前後比對儲存的位元組快照：執行後，沒宣告寫入的欄、resource、事件必須不變；執行前，自上次快照以來除了經 World API 的寫入外必須不變。
- **不做**：Scheduler、archetype、並行、hook、change tick。測試用的型別（Position、Velocity 等）只在 `tests/SpikeTestSupport.hpp`。

## 判定 1：簽名推存取集合

做法：`FunctionArgs<F>` 以偏特化拆出參數列（函式指標、noexcept、成員函式指標的 const／noexcept 四種組合；lambda 與 function object 經 `decltype(&F::operator())`）；每個參數查 `ParamAccess<P>`，用 fold expression 串成 `Reads`、`Writes` 兩個 `TypeList<Access<Kind, T>...>`。`Describe<F>(registry)` 再把型別換成 id、排序、去重。

驗證（`AccessDerivationTests.cpp`）：
- 18 個簽名（對抗檢查 R-08 後；原 17 個加非 const、noexcept 的成員函式），每個寫兩次期望：型別層 `static_assert(DerivesExactly<F, 期望 reads, 期望 writes>)`（集合相等，與順序無關）；id 層 `CheckDescriptor(Describe<F>(...), {手寫 id}, {手寫 id})`。手寫 id 由 `MakeRegistry()` 的登記順序決定（Position 0、Velocity 1……），寫在測試裡，不從程式推。
- `MakeSystem` 存的描述與 `Describe` 相同（free function、lambda、member function、function object 各至少一個）。
- 拒絕的形狀（編譯期）：`World&` 參數、裸元件參考、以參考傳句柄、非句柄參數、非 void 回傳、不可呼叫。同一個 System 同時讀寫同一鍵、或寫兩次（含 `EventReader<T>`＋`EventWriter<T>`、兩個 `Deferred`）是 aliasing，`static_assert` 拒絕；重複讀不算。
- 推出來的句柄真的能跑：8 個 System 在一個 World 上執行，debug 核對不誤報，結果數值正確。

做不到的（C++ 本身的限制，不是這版沒寫）：
- **泛型 lambda**（`[](auto)`）、多載的 `operator()`、取多載函式的位址：沒有單一參數列，`FunctionArgs::deducible` 為 false，`MakeSystem` 以 `static_assert` 擋下並說明。使用者必須寫出具體型別。
- **ref-qualified 成員函式**（`&`、`&&`）沒有特化；需要時再加四個偏特化，屬機械工作。
- 句柄只能**以值**接收（不可複製也不可移動，靠 C++17 保證的複製省略）；以 `const Read<T>&` 接收會被拒絕。
- 引數求值順序未指定，所以句柄的建構不能有順序相依的副作用（目前只解析指標）。

## 判定 2：受限句柄＋debug 核對

### 三層

1. **API 讓繞路很醜**（編譯期，所有組態）：句柄不可複製、不可移動、建構子私有（只有 `ParamFactory` 能造），所以 System 不能把句柄存到外面，也不能自己造一個沒宣告的句柄；`Read`／`Res` 只給 const 參考，要寫就得 `const_cast`；World 不能當參數，要碰 World 只能靠捕獲或全域指標。World 的讀取 API 回傳 const 位元組，要可寫的位元組必須用寫入 API（會被核對）。
2. **debug 核對**（`KERNEL_SPIKE_ACCESS_CHECKS=1`）：上一節的 1 與 2。
3. **審查**：以下〈上限〉的形狀只能靠它。

### 結果（`AccessCheckTests.cpp`；debug 目標 `gyo_kernel_spike_tests_debug`，release 目標 `gyo_kernel_spike_tests_release`）

| 繞路 | 形狀 | debug | release |
|---|---|---|---|
| (a1) | 在 System A 由 `Read<Position>` 取得參考存到外面，System B（只宣告 `Read<Velocity>`）經 `const_cast` 寫 | 斷言（B 結束時：「System 'Tamper' changed component 'Position' without declaring a write」） | 不攔，值被改 |
| (a2) | 同上，但由 host 程式在兩次 System 之間寫（不經 World API） | 斷言（下一個 System 開始時，與那個 System 宣告什麼無關） | 不攔，值被改 |
| (b1) | System 內對 `Read<T>` 回傳的參考 `const_cast` 寫 | 斷言（System 結束時） | 不攔 |
| (b2) | System 內經 `reinterpret_cast` 的裸位元組指標改唯讀欄（同時有一個無關的宣告寫入） | 斷言 | 不攔 |
| (b3) | System 內對 `Res<T>` `const_cast` 寫 | 斷言 | 不攔 |
| (c1) | 捕獲 `World&`，讀未宣告的元件 | 斷言（存取當下：「System 'Peek' read component 'Velocity' without declaring it」） | 不攔，讀到值 |
| (c2) | 捕獲 `World&`，以 id 讀未宣告的 resource | 斷言（存取當下） | 不攔 |
| (c3) | 捕獲 `World&`，寫只宣告讀的元件 | 斷言（存取當下） | 不攔 |
| (c4) | 捕獲 `World&`，讀未宣告的事件 | 斷言（存取當下） | 不攔 |
| (c5) | System 執行中讀另一個 World | 斷言（存取當下） | 不攔 |

(c1)–(c5) 的「存取當下」只在 System 自己的執行緒上成立（R-03）；同樣的存取從 System 另起的執行緒做，不比對（見下表）。

對照（兩個目標都不斷言）：宣告的讀寫（七類都用到）不誤報；宣告 `Write<T>` 的 System 經 World 讀同一欄不誤報（寫的宣告包含讀，R-12）；host 在 System 之間經 World API 的寫入（`GetMut`、`ResourceMut`、`Push`、`ClearEvents`）不誤報。
所有組態都斷言的 Programmer Error：System 執行中做結構變更（`Push`）、System 執行中再跑 System。

### 判定 2 的上限（兩個目標都**不**斷言；「Limit 測試」欄是固定現況的測試）

(a)(b) 的攔法是「System 邊界比對位元組的淨變化」，不是「存取當下」。所以：

| 形狀 | 為什麼攔不到 | 屬於 | Limit 測試 |
|---|---|---|---|
| 寫了再在 System 結束前寫回原值 | 邊界時位元組相同；中間讀到的值已經被影響 | (a)(b) | 有 |
| 存下的參考在**有宣告 `Write<T>`** 的 System 裡寫 | 對存取集合是合法的；但繞過了句柄，將來的 change tick 會漏記 | (a) | 有 |
| host 在兩次 System 之間經 World API 碰過同一欄（含只拿 `GetMut` 不寫）後，再經存下的參考寫那一欄的任何一列（R-02） | `MarkWritten` 以整欄為單位設 dirty，下一次比對把整欄當成新基準，繞路寫入被一起吸收 | (a) | 有 |
| 欄重新配置後經舊參考寫（懸空） | 未定義行為；要 ASan 之類的工具，不是 World 能看的 | (a) | 無 |
| System 另起的執行緒存取 World（R-03） | 執行中的 System 是 `thread_local`，其他執行緒的 World 存取一律當成 host，不比對描述：未宣告的讀完全看不到；寫入只在 System 邊界以淨變化看到（執行緒在 System 返回前 join 時），返回後才寫則和快照比對競爭 | (a)(b)(c) | 有（讀） |
| component、resource、事件 payload 裡的堆積成員（例：`std::vector` 的元素）被改（R-04） | World 只知道每個物件的 size 個位元組，比的是 handle（指標、大小），不是它指向的內容；事件 channel 比的是元素的位元組，元素裡的堆積成員一樣只比 handle | (b) | 有（resource） |
| 經存下的參考**讀**沒宣告的元件 | 讀不改位元組；這不是 (a) 的定義（「再改」），但它是並行時資料競爭的來源 | (a) 的鄰居 | 有 |
| 全域變數、World 以外的狀態（`GlobalCache::Update()` 型） | 不在 World 裡 | mechanism §3.5 | 有 |

能更進一步的已知手段（**本批沒有做**，不是建議採用，只是給判斷用）：debug 建置把每個欄放在自己的記憶體頁，System 執行中把沒宣告寫入的欄設成唯讀、沒宣告的欄設成不可存取（`mprotect`／`VirtualProtect`）。這能把「寫了再寫回」與「經存下的參考讀」也在存取當下攔下（以 SIGSEGV／存取違規的形式），代價是頁粒度的配置、平台相依的程式、以及測試要改成 death test。它仍然攔不到全域變數與 World 以外的狀態。

## 判定 3：突變

目錄：`spikes/kernel/mutations.json`，34 條（第一、二輪 32 條，對抗檢查後加 2 條），格式與跑法照 `docs/architecture/plans/time-threads-trace/scripts/run_mutations.py`（突變只在預期的失敗＋預期訊息時算 killed；編譯期的檢查用 `expect_build_failure`）。第三輪起，每條 `expect_build_failure` 的 regex 寫出該條必須觸發的那個 requirement（例：`IsSealedHandle<Read<Position>>`、只取 `Res<Stats>` 的 `Steering::Report` 的期望），不再用泛用的 `static assertion failed`（R-10）；regex 以 AppleClang 的輸出逐條核對過：16 條都命中自己的輸出，除了 `ks-decl-integrate-write-to-constcast` 的 regex 也命中 `ks-decl-integrate-write-to-read`（兩者拿掉同一個宣告、觸發同一個期望；後者另以 const 正確性的錯誤區分），沒有命中別條的輸出（`build_failure_probe_round3.txt`）。全部在本機（Intel Mac）以 Debug 組態的建置樹跑。

| 類 | 條數 | 內容 |
|---|---|---|
| 移除宣告（事前宣告的對象） | 9 | System 拿掉某個參數卻仍使用那份存取。debug 核對層殺 7 條：`Write<Health>`→`Read`＋`const_cast`（邊界比對），拿掉 `Res`／`EventReader`／`ResMut`／`EventWriter`／`Deferred`／`Read` 改經捕獲的 `World&`（存取當下，6 條）。另 2 條不經核對層：`Write<Position>`→`Read<Position>` 直接賦值由 const 正確性（編譯錯誤）殺，改用 `const_cast` 由判定 1 的期望 `static_assert` 殺（R-11）。七類參數都有 |
| 參數宣告本身 | 7 | 七類句柄的 `ParamAccess` 各改一處（讀變寫、種類錯、不宣告）→ `static_assert` 失敗 |
| 推導機制 | 6 | noexcept free function、const 成員函式、非 const noexcept 成員函式（R-08）不再推導；aliasing 不檢查；非句柄參數被接受；執行期 id 不去重 |
| 句柄封閉 | 2 | `Read` 可複製；`Write` 的建構子公開 |
| 核對 | 8 | 讀不核對；寫的宣告不再包含讀（R-12，對偶）；寫接受讀宣告；另一個 World 不報；執行後不比對；執行前不比對；World API 的寫入不重設基準（誤報）；release 開核對 |
| Registry | 2 | id 不依登記順序；可重複登記 |

**件數の突き合わせ：一致**——第一輪：要求 32、結果 32（被殺 31、存活 1、過期 0、錯誤失敗 0）；第二輪：要求 32、結果 32（被殺 32、存活 0、過期 0、錯誤失敗 0）；第三輪：要求 34、結果 34（被殺 34、存活 0、過期 0、錯誤失敗 0）。

`ks-noexcept-function-not-deduced`（與 `ks-noexcept-member-not-deduced`、`ks-const-member-not-deduced`）是由 `RequireSystemFunction` 的導引 `static_assert`（實例化 `Describe`／`MakeSystem` 時的硬錯誤）殺的，不是期望層：`DerivesExactly` 會命名 `AccessOf<F>::Reads`，F 推不出時那是不合法的型別，期望的 `static_assert` 根本不會被求值（R-09；catalog 的 `note` 也寫了）。

第一輪存活的 `ks-check-write-accepts-read`（「只宣告讀，經 World 寫也放行」）：(c3) 的寫入同時會被兩層看到——存取當下的核對，與 System 結束時的位元組比對。測試原本只看「有沒有斷言」，第一層退化後第二層照樣斷言，所以分不出來；報告說「(c) 在存取當下攔」這件事其實沒有被測到。處置：(c1)–(c5) 加一個檢查——debug 目標要求斷言發生時 System 本體沒有繼續執行（`!passed`），release 目標要求本體跑完（`passed`）。**只收緊**：舊判定會失敗的輸入，新判定都仍失敗；新判定另外會讓「只在邊界攔到」失敗。第一輪的結果保留在 `mutations_round1.txt`，不被第二輪覆蓋。

第二輪前另有兩處可攜性修正（`Registry.hpp` 的型別鍵、`System.hpp` 的 `IsSystemFunction`，見〈兩個以上編譯器的差異〉），因此第二輪跑全目錄而不是只跑存活的一條。

跑法（從 repo 根目錄；先照〈怎麼重跑〉configure 與建置）：

```sh
python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py \
    --build-dir build/target/_build/kernel-spike-debug \
    --cmake <cmake 的絕對路徑> --catalog spikes/kernel/mutations.json
```

（runner 呼叫 `cmake --build`，Ninja 要在 PATH 上；本機以 `PATH=<ninja 目錄>:$PATH` 執行。）

## 兩個以上編譯器的差異

本機只有一個編譯器：**AppleClang 21.0.0（clang-2100.1.1.101），x86_64-apple-darwin25.6.0（Intel Mac）**，CMake 4.4.3＋Ninja 1.13.2，C++20。Debug 與 Release 兩種 `CMAKE_BUILD_TYPE` 各建兩個目標，四個執行檔全過、零警告（`-Wall -Wextra -Wpedantic`）。

**MSVC、gcc（linux-x64）、AppleClang arm64（macos-arm64）待 L1**。目前 L1 不建 `spikes/kernel/`（根 CMakeLists 不加這個目錄，workflow 也沒有步驟）。要讓 L1 看三平台，需要改 `.github/workflows/`（`spikes/` 以外，照 `v8-arch-kernel-delegation` 要先給使用者），或在 Windows／Linux 實機照 `spikes/kernel/CMakeLists.txt` 開頭的三行命令手動跑。

預先看到、在其他編譯器上可能出問題的點（沒有實測，供 L1 判讀）：
- **型別查表鍵的位址唯一性**：Registry 用 per-type 靜態物件的位址當查表鍵。原本寫成 `static constexpr char`（唯讀資料）；MSVC `/OPT:ICF` 或 lld `--icf=all` 可能把內容相同的唯讀資料摺成同一個位址，讓兩個型別撞鍵（Registry 會以「registered twice」斷言）。第二輪前改成可寫的 `static inline char`（可寫資料不被摺疊）。這是事前的可攜性修正，本機沒有觀測到問題。另外，Windows 上跨 DLL 時每個模組各有一份物件、鍵不同；spike 只有單一執行檔，Engine 化時 Registry 由誰擁有、鍵怎麼跨模組，是規格（`-spec`）的事。
- `Detail::IsSystemFunction` 原本是變數模板裡立即呼叫的 lambda（含 `if constexpr`），第二輪前改成 constexpr 函式，避開部分編譯器在這種寫法上的已知問題；行為不變（第二輪 32/32）。
- MSVC 需要 `/Zc:preprocessor`（測試用的 `SPIKE_CHECK_ASSERTS` 是 variadic macro，照 repo 的寫法）；CMakeLists 已對 MSVC 加上。`KERNEL_SPIKE_ASSERT` 本身不是 variadic。
- 不可移動的 prvalue 經 pack expansion 直接初始化以值參數（保證的複製省略）：C++17 規則，三家都應支援。
- noexcept 函式型別的偏特化（C++17 起 noexcept 是型別的一部分）。
- MSVC `/W4` 對 `alignas(64)` 測試型別可能出 C4324（填補警告），只是警告。

## 用了哪些 C++ 手段

- 偏特化拆函式型別（函式指標、成員函式指標 × const × noexcept）；`decltype(&F::operator())` 處理 lambda 與 function object；`std::void_t` 做 SFINAE。
- `TypeList` 加 fold expression：計數、包含、串接、集合相等；`static_assert` 比對期望。
- `ParamAccess<P>` 特化的封閉集合（七類句柄）；非句柄參數、aliasing、非 void 回傳在 `MakeSystem` 以 `static_assert` 擋下並附訊息。
- 句柄：刪除複製（因而也不可移動）、私有建構子＋`friend` 的工廠；以值傳入靠 C++17 保證的複製省略。
- 型別擦除：`TypeOps` 的函式指標（move-construct、destroy）、`::operator new` 的對齊配置、`std::launder`。
- `thread_local` 的「目前 System 描述＋World」；RAII 在測試處理器拋出時也復原。
- 位元組快照比對（debug）；`dirty` 旗標讓 World API 的寫入成為新基準。
- 斷言：自己一個最小的 `KERNEL_SPIKE_ASSERT`（可換處理器），測法照 `tests/common/support/AssertTestSupport.hpp`（處理器改成拋出，`SPIKE_CHECK_ASSERTS`）。

## Architecture 上的註記

- `spikes/` 是新的 top-level 目錄（AGENTS §10 視為 Architecture Concept）。位置是 README 批次表核准的候選（`spikes/kernel/`），`v8-arch-kernel-delegation` 也以 `spikes/` 為範圍；它不在任何 build graph 裡、沒有 owner 登記、不被任何 target 依賴，刪掉整個目錄不影響其他東西（Removability 成立）。spike 結束後 `spikes/` 要留成概念還是刪除，由架構師／使用者決定。
- 沒有其他 Delta：不改現有程式、preset、CMake 模組、契約、Design Invariants。

## 對「中間層包或重寫」的意義（給 `-audit`）

- 判定 1 的結果表示：**新寫的 System**（組織層與重寫的模組）可以只寫一份簽名，存取集合由編譯器推出，Scheduler 可信任它（本機；待 L1）。
- 判定 2 的結果表示：**包起來的舊模組**，如果它的實際存取都經過 World（或都經過交給它的句柄），debug 核對攔得住未宣告的寫入與未宣告的 World 讀取（限 System 自己的執行緒；模組若自己起執行緒碰 World，那些存取不被核對）；如果它**持有指向 World 資料的指標**、**有全域狀態**、或**在 World 外快取並讀取**，核對攔不到讀取與短暫寫入，只能靠審查。所以 `-audit` 的第二問「實際存取能不能被一份契約框住」，在 C++ 裡要改成可檢查的問法：這個模組是否持有跨 tick 的指標或參考、是否有全域可變狀態（以及是否自己起執行緒碰 World）。兩者皆否，包起來的契約可以靠 debug 核對；任一為是，契約只能靠審查，判斷偏向重寫或先拆掉那些狀態。

## 對抗檢查（v8-arch-kernel-spike-adversary）

審查：Fable／xhigh（架構師安排）。本代理只收到下列各條的處置要求；沒有轉交到本代理的條目，內容與處置由架構師記錄。

| 條 | 處置 |
|---|---|
| R-01 | 待使用者（判定 1、2 的結論措辭不改） |
| R-02（should） | 上限表加「host 在同一視窗內經 World API 碰過同一欄後，繞路寫入被 `MarkWritten` 當成新基準」；`Limit:` 測試「a bypass write is taken as the new baseline once host code touched the storage」固定現況 |
| R-03（should） | 上限表「另一個執行緒」列改寫（`thread_local`，其他執行緒的存取當成 host，未宣告的讀看不到）；`Limit:` 測試「a World read from another thread during a System run is checked as host access」；報告裡「(c) 在存取當下」都加「僅限 System 自己的執行緒」（結論表的格子不改，以表下的註補）。CMakeLists 加 `Threads::Threads` |
| R-04（nit） | 堆積成員一列改成通則：component、resource、事件 payload 都只比 handle 位元組 |
| R-05 | 未轉交本代理（由架構師記錄） |
| R-06 | 待使用者 |
| R-07 | 未轉交本代理（由架構師記錄） |
| R-08（should） | 加非 const、noexcept 的成員函式 `Steering::Prepare`（`static_assert`＋執行期比對＋`MakeSystem`）；突變 `ks-noexcept-member-not-deduced` |
| R-09（註記） | `ks-noexcept-function-not-deduced` 由導引層的硬錯誤殺，不是期望層：寫在〈判定 3〉與 catalog 的 `note` |
| R-10（nit） | `expect_build_failure` 的 16 條 regex 都改成該條特有的 requirement 片段，並以實際輸出核對命中與誤命中 |
| R-11（nit） | 「宣告移除 9 條全殺」改寫成：debug 核對層殺 7 條，另 2 條由 const 正確性與判定 1 的期望 `static_assert` 殺 |
| R-12（should） | 加 Control 測試「a declared write also allows reading the same storage through the World」；對偶突變 `ks-check-write-not-readable`（`DeclaresRead(key) || DeclaresWrite(key)` → `DeclaresRead(key)`） |
| R-13 | 未轉交本代理（由架構師記錄） |
| R-14 | 未轉交本代理（由架構師記錄） |

處置後：全部測試重跑（Debug、Release 兩組態 × 兩目標，33/33 case），全目錄突變第三輪 34/34。

## 估時與實際

- 估：代理實作 4 小時（實作 1.5、測試 1、突變 0.5、文件 1），上限 6 小時（1.5 倍）。README 的批次估「2～3 天」是含審查與 L1 的整批。
- 實際：2026-11-11 18:02 開始，18:27（JST）完成，約 25 分鐘（含突變兩輪與文件）。遠低於估計：原型是純標頭、一次編譯通過，突變每條只需重建一個小目標。沒有碰到 §14 的停止條件（同一問題只改了一次測試）。
- 對抗檢查的處置：第一次回報（18:28）之後開始，18:59（JST）完成，含 16 條建置失敗突變的輸出探測與第三輪全目錄。

## 證據

主 checkout `build/evidence/kernel-spike/`（git 不追蹤；本機 Intel Mac）：`toolchain.txt`（編譯器、CMake、Ninja 版本）；`build_config-debug.txt`、`build_config-release.txt`（clean 建置，零警告）；`tests_config-{debug,release}_target-{debug,release}.txt`（四個執行檔的 doctest 輸出）；`ctest_config-debug.txt`、`ctest_config-release.txt`（突變後還原的樹再建再跑）；`test_cases.txt`（30 個 case 名，第二輪時）；`mutations_round1.txt`（31/32）、`mutations_round2.txt`（32/32）。對抗檢查後：`tests_round3_config-{debug,release}_target-{debug,release}.txt`（33 個 case）、`test_cases_round3.txt`、`build_failure_probe_round3.txt`（16 條建置失敗突變各自的錯誤行，用來定 regex）、`mutations_round3.txt`（34/34）。建置樹在工作樹 `build/target/_build/kernel-spike-debug`、`kernel-spike-release`（git 不追蹤）。第一輪突變前的測試輸出只是開發中的執行，未保存。

## 怎麼重跑

```sh
cmake -S spikes/kernel -B build/target/_build/kernel-spike -G Ninja
cmake --build build/target/_build/kernel-spike
ctest --test-dir build/target/_build/kernel-spike --output-on-failure
python3 docs/architecture/plans/time-threads-trace/scripts/run_mutations.py \
    --build-dir build/target/_build/kernel-spike --catalog spikes/kernel/mutations.json
```

離線時加 `-DFETCHCONTENT_SOURCE_DIR_DOCTEST=<既有的 doctest-src>`（本機用的是主 checkout `build/target/_build/test/_deps/doctest-src`）。
