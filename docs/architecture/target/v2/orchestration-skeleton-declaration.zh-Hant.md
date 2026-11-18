# 事前宣告：編排層的骨架（`v8-arch-gyo2-orchestration-skeleton`）

更新：2026-11-18（修訂 0）。Owner：架構（GYO2 主會話開的任務卡）。**狀態：等第一段核准（方向與設計）；核准之前不改程式。**

性質：實作批的事前宣告（AGENTS §3 的 Architecture Delta、§2 的基盤 Architecture、§14.4 的判定變更、Design Invariants 的變更）。實作段第 2 批。

根據：
- [批次表](rebuild-batch-plan.zh-Hant.md)第二段第 2 條（文件分支 `claude/gyo2-time-design` 41b95525）。
- [統合宣告](kernel-app-boundary-declaration.zh-Hant.md)修訂 38（已核准）：關聯表 L5、L6、L15、L16、L17、L19、L20、L21；第 3 節（分層與歸屬）；第 7.2 節 J-19、J-24、J-58 列；第 8 節本批那一列；第 9.1 節 DI-KS 15、22；第 9.2 節 O-1～O-5；第 15.2 節。
- [讀檔宣告](restore-entry-declaration.zh-Hant.md)修訂 11（已核准）：5.1、5.7、5.8、第 8 節第 2 段那一列、第 15 節確認清單。
- [第 1 批的宣告](kernel-subtract-structure-declaration.zh-Hant.md)（已合併，PR #242）：3.2 的兩步與規則 1～10、3.3、第 11 節留給本批的兩件。
- [權威份](authority-module-declaration.zh-Hant.md) 4.4、4.5（編排層 finish 之後跑一次 `Check`；策略在建 World 時選定）；[回滾份](rollback-module-declaration.zh-Hant.md)第 148 行的同一時機。
- HANDOFF 決定表：`v8-arch-gyo2-orchestration-layer-name`、`-game-init-list`、`-two-layers-like-bevy`、`-init-values-like-bevy`、`-restore-entry-new-world-default`、`-save-unaware-means-format-only`、`-module-plugin-registers`、`-module-plugin-owner-definition`、`-module-deps-in-registers`、`-products-pick-modules`、`-dynamic-insert-per-world`、`-thread-binding-by-runner`、`-kernel-subtract-structure-*`、`-immediate-spawn-returns-error`、`-failure-is-not-absence`、`-batch2-after-batch1-merged`。
- [error-handling.md](../../error-handling.md) 規則 1 第 3 步（22f3f392：失敗不用空值）；[共用規則頁](../../../cross-component-rules.md) CCR-3、CCR-4、CCR-5。
- 程式：`origin/gyo2` 0f1f2d21。

**標記**：寫「卡片照 L17 判」的，是任務卡照命名規範、別家原始碼與已定的原則自己判的，不是使用者的話（使用者 2026-11-17：「什麼都要我來定，乾脆代碼我也自己寫好了。名字你有命名規範。例子你有源碼。」）。交使用者的只有第 13 節。

證據（主 checkout `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-orchestration-skeleton/`，git 不追蹤）：`inventory-startup-plugin.md`（開局與登記單位相關的測試案例、檔案與突變的盤點；關鍵字掃描，會多算）。

---

## 1. 這一批做什麼（先用遊戲講）

**單機 2D 橫版。** 產品的 `main` 只寫「用預設的 Module、跑起來」。編排層照 Module 登記檔把每個 Module 接進主 World：每個 Module 先登記（build），Kernel 建 Plan，每個 Module 再 finish，最後跑選了的檢查；全部過了才建 World。runner 先跑遊戲初始化清單一次（讀按鍵設定；狀態機把「進入主選單」插在它後面），再每幀跑每幀的清單。現在的 Kernel 自己認得「開局」（`ScheduleOptions::startup`），這一批把它拿掉、搬到編排層（使用者 11-17「用Bevy的方式走，」）。

**RPG 在遊戲中讀檔。** 玩家在第 3 關按「讀取存檔 2」。編排層建一個新的 World（用同一份登記），在第一次跑它之前給作者一個時機：把配發器範圍的下界移到存檔記的下一號之後、放好暫存的存檔資料與金幣的預設值 0；之後作者的讀檔 System 用 `SpawnWithId` 照存檔編號放回物品與任務、用 `ResMut` 把金幣改成 120。讀檔失敗（存檔壞了）時新 World 丟掉、舊的照跑，玩家不會掉進一個半空的世界。遊戲初始化不再跑（它是行程一次）。按鍵設定是遊戲初始化放進主 World 的——主 World 換掉之後它怎麼辦，是第 13 節交你的題目。

**將來的指令集。** 輸入一行指令加一個 System。World 宣告准許時，編排層走 Kernel 的兩步（登記、建 Plan），下一幀那個 System 就跑；宣告不准時（PvP 的 Match），插入被拒、回傳錯誤、**記一次警告**、遊戲照跑（使用者 11-17：「插入就拒絕，返回信息（不退出，但是System無法插入，日誌報警告）」）。第 1 批只做了 Kernel 那一半，警告沒人記（第 1 批宣告第 11 節），這一批補上。

**PvP 的 Match。** 產品另選權威 Module，World 的描述帶「策略＝伺服器權威」。權威 Module 在所有 Module finish 之後跑一次 `Check`（權威份 4.5），讀唯讀登記表與 World 的宣告，不過就啟動失敗；Kernel 自己有啟動錯誤時 `Check` 不跑，報告寫「因 Kernel 錯誤而未檢查」（統合 O-5）。這一批沒有權威 Module，只做這個時機與彙整。

**單機不上權威。** Module 預設全帶，產品可以關（使用者 11-17「能，要不然單機遊戲也必須上權威了。」；主會話照 Bevy `DefaultPlugins`／Unity 判定預設全帶可關）。產品在自己的建置描述裡寫「關掉權威」，CMake 就不把權威接進這個產品，編排層也不 build 它；關掉的是別的 Module 依賴的（例：權威依賴時間，關了時間），啟動錯誤。

---

## 2. 範圍

### 2.1 這一批做的（O1～O12）

| # | 內容 | 根據 |
|---|---|---|
| O1 | 新的 Module `engine/orchestration`（`GYO::Engine::Orchestration`，target `gyo_orchestration`／`GYO::Orchestration`），只依賴 Kernel 與 Base | `orchestration-layer-name`；統合第 3 節、O-4；命名規範 N11、N14、N17 |
| O2 | 登記單位的兩種型別 `Module`、`Plugin`（從 Kernel 的 `Plugin` 搬來、拆成兩種）與生命週期：產品層一次的 `Configure`、每個 World 的 `Build`、`Finish`、`Check` | `module-plugin-registers`；統合 L5、O-2、3.2 第 1 列 |
| O3 | `Product`：World 群（每個 World 一份 Registry）、World 的描述（名字、准不准動態插入、用哪些登記單位、給各 Module 的選項包）；build → 建 Plan → finish → 檢查 | 統合 L5、O-2、15.2；權威份 4.4、4.5 |
| O4 | 啟動錯誤的彙整：Kernel 的 `StartupReport`（每個 World 一份）、編排層自己的碼、各登記單位回報的錯誤收成一份 | 統合 O-5 |
| O5 | Module／Plugin 登記檔：格式、CMake 讀取（引擎各 Module 的 link 從它來、替產品產生登記單位的表）、編排層讀出依賴並檢查；Module 預設全帶、產品可以關，Plugin 由產品列出 | `module-plugin-registers`、`module-deps-in-registers`、`products-pick-modules`；統合 L20；批次表「主會話自己判的」第 3 條 |
| O6 | 遊戲初始化清單（產品一份、跑在第一個 World、行程一次、在第一幀之前）與每幀的清單（每個 World 一份）；清單可以在指定的 schedule 前後插入 | `game-init-list`、`two-layers-like-bevy`；統合 O-1 |
| O7 | runner 的介面（由登記單位在 `Configure` 裝、一個 `Product` 至多一個）與預設的「只跑一次」runner | `thread-binding-by-runner`；統合 O-3、第 8 節本批那一列 |
| O8 | 準備時機：建 World 之後、第一次 Run 之前，依序呼叫登記單位與產品給的準備函式（讀檔用：設配發器範圍的下界、放暫存的存檔資料與 Resource 的預設值） | 讀檔宣告修訂 10、11（5.1、5.8）；統合修訂 33 |
| O9 | 執行中替換 World：用同一份 Registry 建新的、跑準備、成功才換上、失敗保留舊的；遊戲初始化不重跑 | 讀檔宣告修訂 11（5.7、第 8 節） |
| O10 | 動態插入的編排半：插入入口；拒絕或建 Plan 失敗時回傳錯誤、記一次警告、World 照跑 | `dynamic-insert-per-world`；統合 L5；CCR-3 |
| O11 | **同一批拿掉 Kernel 的開局排程**：`ScheduleOptions::startup`、D-E21、`StartupNotNestable`、`StartupRunning`、DI-KS 22、World 與 `WorldExecutor` 的開局狀態；J-58 改寫後搬到編排層 | 批次表第 2 條；統合 3.2 第 3 列、7.2 J-58 列 |
| O12 | **同一批拿掉 Kernel 的登記單位**：`Plugin`、`FinishContext`（含 `InstallRunner`）、`Registry(std::vector<Plugin*>)`、`Registry::Finish()`、R-E1、R-E3、R-E6；Registry 與 Plan 指紋改版、golden 重產 | 統合 3.2 第 1 列、7.2 J-19、J-24 列；第 1 批宣告 3.2 規則 9 |

另外一件（第 1 批的遺留，主會話交辦，已隨本宣告提交）：結構段 dev_log 的「驗證與經過」補 G-6 成立、開頭改成已合併。

### 2.2 不在這一批（留到哪一批）

| 留下的 | 隨哪一批 |
|---|---|
| 生成世界（每個 World 各自、可以多次、可以分批）；讀檔 System 掛在生成世界的方式 | 第 5 條「生成世界（重做）」。本批的準備時機與 World 替換是它的前提 |
| 多個 World 之間的交換點、extract 的時機、執行緒的安排；交換函式的登記 | 第 5 條「多個 World、交換點」。本批的 `Product` 已持有 World 群，但每幀只在呼叫的執行緒上依序跑 |
| 每幀迴圈的 runner（跑到收到結束的請求為止）與 System 端的請求通道（結束、替換 World；照 Bevy 的 `AppExit` 是一種訊息） | 第一個需要它的批（視窗的 Plugin 或第 3 條之後）。訊息在第 3 條改名 `Message`，先做會白改一次（第 3.9 節） |
| 時鐘、`Domain`、tick | 第 3 條。遊戲初始化的 schedule 若設了 clock，在第 3 條之前照樣推 tick（第 3.6 節） |
| 寫者數量 D-E2 | 第 4 條。拿掉開局之後、第 4 條之前，遊戲初始化的 System 照樣算寫者（第 7.1 節組 E） |
| 權威、回滾、時間、狀態機這些 Module 本身 | 各自的批。本批的登記檔裡它們還不存在 |
| 產品登記檔（`engine/config/projects.csv`）的欄位 | 不改。產品選 Module 寫在產品自己的建置描述（第 3.2 節） |

---

## 3. 設計

本節的程式是草圖：成員的確切簽名、`const`、`[[nodiscard]]` 與 namespace 的寫法（例：在 `GYO::Engine::Orchestration` 裡要寫 `::Engine::Base::Result`，否則 `Engine::` 會先找到 `GYO::Engine`）在實作時定；定下的是型別的分工、呼叫的順序與錯誤的歸屬。

### 3.1 O1：Module 的位置

`engine/orchestration/`，底下 `include/engine/orchestration/` 與 `src/`（N11、N12）；namespace `GYO::Engine::Orchestration`（N17：新寫的 Module 照 N17，帶過來的九個模組的 `Engine::<模組>` 不在這一批改）；target `gyo_orchestration`、別名 `GYO::Orchestration`（N14）。依賴：Kernel、Base（統合 O-4：「`engine/orchestration` 只 link Kernel 與 base」）；寫在登記檔（第 3.2 節），不在 CMakeLists 另寫。

### 3.2 O5：Module／Plugin 登記檔

**事實**：gyo2 上還沒有這兩份檔；建置層的產品登記檔是 `engine/config/projects.csv`、`tools.csv`（CSV，CMake 用 `_gyo_registry_csv_records` 讀，`build/cmake/GyoAppRegistry.cmake`；CI 的 `build/ci/common/app_registry.py` 也讀）。11-14 定的：兩份、放建置層、只有一處；每列寫名字、標記（進 World／不進 World）、依賴；CMake 照它 link；執行時的 Registry 只收進 World 的登記單位、由清單產生、不另外手寫；Module 在 Plugin 前、Plugin 可以依賴 Module、反過來不行（`module-plugin-registers`）。11-17 定的：依賴只寫在登記檔、編排層讀出並在 build 時檢查、缺或排在後面是啟動錯誤、不重排、程式碼不寫 `Requires`（`module-deps-in-registers`）。格式在本批或建置層的批定（統合 L20）——**本批定**（批次表第 2 條寫了「登記檔的讀取」）。

**格式**（卡片照 L17 判：照 repo 既有的產品登記檔）：`engine/config/modules.csv`、`engine/config/plugins.csv`，欄位

| 欄 | 意思 |
|---|---|
| `name` | 名字＝`engine/<name>/` 或 `plugins/<name>/` 的目錄名（N11、N21）；CMake target 是 `gyo_<name>`（N14） |
| `description` | 一句話 |
| `library_only` | `yes`＝不進 World（只當函式庫用，沒有登記單位）；`no`＝進 World（11-14：標記叫「不進 World」，不叫「純函數」） |
| `depends` | 依賴的 Module／Plugin 名字，`;` 分隔；CMake 以 PUBLIC（標頭只有的 Module 以 INTERFACE）link |
| `private_depends` | 同上，CMake 以 PRIVATE link（現在 `io` → `math`、`os`，`threads` → `os` 是 PRIVATE；不分開寫的話，改成 PUBLIC 會把 OS 層的標頭漏給 io 的使用者） |
| `unit` | 進 World 的那一列：登記單位的完整類別名（例 `GYO::Engine::Time::TimeModule`）；不進 World 的列留空 |
| `header` | 進 World 的那一列：登記單位的標頭（例 `engine/time/TimeModule.hpp`）；不進 World 的列留空 |

本批寫進 `modules.csv` 的列：`base`、`math`、`entity_store`、`kernel`、`orchestration`、`os`、`time`、`threads`、`io`、`audio`，全部 `library_only=yes`（現在沒有任何 Module 有登記單位；時間的登記單位隨第 3 條）。順序＝現在 `engine/CMakeLists.txt` 的 `add_subdirectory` 順序（被依賴的在前）。`plugins.csv` 只有表頭（gyo2 上沒有 Plugin）。

**CMake 讀取**（新檔 `build/cmake/GyoUnitRegistry.cmake`，可以用 `cmake -P` 跑，照產品登記檔的慣例）：
1. **驗證檔案**（configure 時 `FATAL_ERROR`，訊息寫檔名、列、原因）：表頭不對；兩份檔裡名字重複；`depends`／`private_depends` 指到不存在的名字、或指到檔案裡排在後面的列（排序就是 build 的順序，不重排）；Module 依賴 Plugin；`library_only=yes` 卻有 `unit`／`header`，或 `no` 卻缺。
2. **引擎各 Module 的 link 從它來**：每個 `engine/<name>/CMakeLists.txt` 的 `target_link_libraries(... GYO::<Module> ...)` 換成 `gyo_link_registered_dependencies(<name>)`；第三方的 link（`Threads::Threads`、`shell32`）照舊留在 CMakeLists。卡片照 L17 判：AGENTS §9「同じ事実を手動で維持する場所は、一か所に限定する」——依賴同時寫在登記檔與 CMakeLists 就是兩處；11-14 的原話是「CMake 照它 link」。別家：O3DE 把 Gem 的依賴寫在 `gem.json`、CMake 的 target 依賴另寫（兩處）；Unreal 的 Module 依賴寫在 `.Build.cs`、Plugin 依賴寫在 `.uplugin`、由 UBT 照它們建（一處）。取 Unreal 的一處，不取 O3DE 的兩處。
3. **替產品產生登記單位的表**：`gyo_product_units(<target> [DISABLE_MODULES <名字>…] [PLUGINS <名字>…] [REGISTERS <modules.csv> <plugins.csv>])`：
   - 預設帶 `modules.csv` 裡**所有進 World 的 Module**；`DISABLE_MODULES` 關掉列出的；Plugin 預設不帶，`PLUGINS` 列出才帶。
   - link 帶上的登記單位的 target（不進 World 的 Module 經依賴已經 link）；在 `<target>` 的 binary 目錄產生 `<target>_units.cpp`，定義 `RegisteredUnits MakeRegisteredUnits()`：兩份登記檔的**全部列**（名字、種類、`library_only`、依賴）與帶上的登記單位的實例（照登記檔的順序；`std::make_unique<unit>()`，`#include` 那一列的 `header`）。
   - `REGISTERS` 給測試用（共通測試用合成的登記檔，AGENTS §7）；省略時讀 `engine/config/` 的兩份。
   - 產品選什麼寫在**產品自己的建置描述**（它自己的 `CMakeLists.txt` 裡這一個呼叫）：產品的新增、刪除只動它自己的內容與登記（AGENTS §7），不改產品登記檔的欄位。
4. 「Module 預設全帶、產品可以關」：照主會話的判定寫明（批次表「主會話自己判的」第 3 條；Bevy `DefaultPlugins` 加 `PluginGroupBuilder::disable`，`crates/bevy_app/src/plugin_group.rs:551`；Unity 內建模組預設帶、可關；O3DE 要列 `enabled_gems` 是少數）。**Plugin 預設不帶**（卡片照 L17 判）：Plugin 是第三方、產品選用（同詞異義表 Plugin 列「在 Engine 外，產品選用」，11-16）。

**編排層讀出依賴並檢查**（執行時，`Product` 建立時；使用者定的「編排層從登記檔讀出依賴並在 build 時檢查」）：`Product` 收到 `RegisteredUnits`（產生的或測試手寫的），對每個 World 實際用到的登記單位檢查：
- 名字重複（O-E1，舊 R-E1）；
- 登記單位的 `Name()` 不在登記檔、在錯的那一份（`Module` 卻列在 `plugins.csv`）、或那一列是 `library_only=yes`（O-E2）；
- 依賴：每個依賴若是進 World 的，必須是這個 World 也用到、而且排在前面的登記單位；不進 World 的依賴視為已經在（CMake 已經 link）；Module 依賴 Plugin（O-E3，舊 R-E3；同一件事在 configure 時也擋，執行時再擋一次是因為 `RegisteredUnits` 可以手寫，例：測試）。

**退掉的選項**（AGENTS §4）：
- 格式：CMake 語法的檔（`gyo_module(io DEPENDS …)`）——退掉：CI 的 Python 讀不了；JSON——退掉：repo 的登記檔都是 CSV，CMake 已有讀 CSV 的函式。系譜：既有的 `engine/config/projects.csv`。
- 登記單位怎麼產生：每個 Module 在自己的程式裡靜態自我登記——退掉：靜態初始化的順序與連結器丟掉沒被參照的物件檔，兩者都會讓「登記檔看得到的」和「實際接上的」不一致；產品 `main` 手寫清單——退掉：11-14「由清單產生，不另外手寫」。系譜：O3DE 與 Unreal 都由建置工具照清單產生接上的程式。
- 依賴的檢查只在 configure 時做——退掉：使用者定的是編排層在 build 時檢查（`module-deps-in-registers`）；configure 時只驗檔案本身。

### 3.3 O2：登記單位的兩種型別

```cpp
namespace GYO::Engine::Orchestration {
namespace Detail { class RegistrationUnit; }   // 兩種型別共用的基底；作者不直接繼承
class Module : public Detail::RegistrationUnit { /* 同下，無新成員 */ };
class Plugin : public Detail::RegistrationUnit { /* 同下，無新成員 */ };

class Detail::RegistrationUnit {
public:
    virtual ~RegistrationUnit();
    // 等於登記檔那一列的 name（O-E2 比對）。
    [[nodiscard]] virtual std::string_view Name() const = 0;
    // 產品層、一次、在任何 World build 之前：插入遊戲初始化清單、裝 runner。
    virtual void Configure(ProductSetup& setup);
    // 每個用到它的 World 一次：setup.Registrar() 交給 Kernel 登記；
    // setup.Options<T>() 讀 World 描述裡給它的選項；插入這個 World 的每幀清單、準備函式。
    virtual void Build(WorldSetup& setup) = 0;
    // 這個 World 的 Plan 建好之後（版本 1），依同順序：讀唯讀登記表，可以回報錯誤；不能登記。
    virtual void Finish(const Kernel::FrozenRegistry& registry, WorldFinish& context);
    // 這個 World 所有登記單位 finish 之後，依同順序：策略的檢查（權威份 4.5 的 Check）。
    virtual void Check(const Kernel::FrozenRegistry& registry, WorldCheck& context);
};
}
```

- **兩種型別**照 11-14「拆成兩種型別，優先也包含依賴方向」；名字照 N20（子類別以 `Module`／`Plugin` 結尾）。兩種的成員一樣，差在 `Product` 收它們的位置（Module 一份清單、Plugin 一份，Module 先）與依賴方向的檢查。共用基底放 `Detail`（N13：模組內部的東西放 `Detail`），作者只看得到 `Module`、`Plugin`。
- **`Requires()` 拿掉**（`module-deps-in-registers`）。
- **生命週期的順序**（統合 L5、O-2：build → 建 Plan → finish → 選了的檢查）：見 3.4。和現在 Kernel 的 `Registry::Finish()` 不同的地方：現在 `Plugin::Finish` 在**建 Plan 之前**跑、讀到版本 0（`Registry.cpp:1843-1852`）；改成在建 Plan 之後跑、讀到版本 1 與 Plan。這是統合 L5 定的順序（第 1 批宣告 3.2 規則 9：「下一批搬到編排層時改成 build → 建 Plan → finish」）。
- **`Finish` 與 `Check` 分開**（卡片照 L17 判）：`Finish` 不能登記，所以任何一個登記單位的 `Finish` 都讀到最後的登記；分開是為了讓策略的檢查在**所有** `Finish` 之後（權威份 4.5「依同順序 finish → 權威檢查（`Check` 跑一次）」），而且 Kernel 有啟動錯誤時兩者都不跑（O-5）。
- **為什麼多一個 `Configure`**（卡片照 L17 判）：遊戲初始化清單與 runner 屬於 `Product`（不屬於某個 World），而 `Build` 每個 World 跑一次。Bevy 的 `Plugin::build(&mut App)` 一次涵蓋兩者，因為 Bevy 的 Plugin 只 build 一次、要碰子 App 時自己 `sub_app_mut`（`crates/bevy_app/src/app.rs:1235` 一帶）；GYO 的每個 World 各有一份 Registry，所以分成產品層一次與每個 World 一次。
- 系譜：Bevy `Plugin` 的 `build`／`finish`／`cleanup`（`crates/bevy_app/src/plugin.rs:59`、`:70`、`:77`）；Kernel 現在的 `Plugin`（`Registry.hpp:72-87`）。

### 3.4 O3：`Product`

```cpp
struct WorldDescription {
    std::string name;                         // Product 內唯一（O-E6）
    bool dynamicInsert = true;                // 轉成 Kernel::WorldOptions（L5）
    std::vector<std::string> units;           // 空＝Product 帶上的全部登記單位
    OptionBag options;                        // 各 Module 自己解讀的選項（例：權威策略）；編排層轉交、不解讀
    std::vector<std::string> frame;           // 這個 World 的每幀清單（schedule 名），登記單位可再插入
    std::vector<PrepareFn> prepare;           // 產品給的準備函式，排在登記單位的之後（3.8）
};
struct ProductDescription {
    std::string name;
    std::vector<WorldDescription> worlds;     // 第一個是「第一個 World」：遊戲初始化在它上面跑
    std::vector<std::string> gameInit;        // 遊戲初始化清單（schedule 名），登記單位可再插入
};

class Product {
public:
    static Engine::Base::Result<std::unique_ptr<Product>, ProductStartupReport>
        Build(ProductDescription description, RegisteredUnits units);
    RunnerExit Run();                                         // 呼叫 runner（3.7）
    // 給 runner 與 Product 的持有者（兩次 Run 之間）：
    Engine::Base::Result<void, Kernel::KernelError> RunGameInit();   // 只能一次（第二次斷言）
    Engine::Base::Result<void, Kernel::KernelError> RunFrame();      // 遊戲初始化還沒跑時先跑它
    Kernel::WorldExecutor& World(std::string_view name);
    Engine::Base::Result<void, OrchestrationError> ReplaceWorld(std::string_view name, PrepareFn extra = {});  // 3.9
    Engine::Base::Result<void, OrchestrationError> InsertSystems(std::string_view world, Kernel::RegistrationOwner owner,
                                                                 InsertFn build);                           // 3.10
    void SetDiagnosticHandler(std::function<void(const ProductDiagnostic&)> handler);  // 警告的唯一出口
};
```

**`Product::Build` 的順序**（一次收齊所有錯誤；卡片照 L17 判，順序本身照統合 O-2）：
1. 檢查 World 描述（沒有 World、名字重複、`units` 指到沒帶上的登記單位：O-E6）與登記單位對登記檔（O-E1、O-E2、O-E3，第 3.2 節）。有錯就停在這裡（後面的步驟都依賴「登記單位是對的」）。
2. 每個登記單位依序 `Configure`（Module 先、各自照登記檔的順序）：收集遊戲初始化清單的插入與 runner（第二個是 O-E4，舊 R-E6）。
3. 每個 World 依序：新建一份 `Kernel::Registry`；這個 World 用到的登記單位依序 `Register(owner, unit.Build)`（owner＝登記單位的名字與它在 Product 裡的序號，統合 3.5）；`BuildPlans`。**成功**：依序 `Finish`，再依序 `Check`。**失敗**：收下這個 World 的 Kernel `StartupReport`，這個 World 的 `Finish`、`Check` 不跑，報告寫「因 Kernel 錯誤而未檢查」（O-5）；別的 World 照做。
4. 檢查清單：遊戲初始化清單的每個名字在第一個 World 的 Registry 裡是 schedule；每幀清單的每個名字在它那個 World 的 Registry 裡是 schedule（O-E5）。
5. 有任何錯誤：回傳 `Err(ProductStartupReport)`，沒有任何 World 被建（所以第 1 批宣告第 11 節的「`BuildPlans` 換上之後策略檢查失敗、Kernel 撤不回」在啟動時不會發生：整個 Product 不成立、Registry 一起丟掉）。
6. 每個 World 依序建 `Kernel::WorldExecutor(registry, name, WorldOptions{dynamicInsert})`，跑準備（3.8）；準備失敗是 O-E7，整個 `Build` 失敗。

**每個 World 一份 Registry**（卡片照 L17 判）：統合 15.2「登記、Plan、Registry 都是每個 World 一份」；Kernel 允許一份 Registry 建多個 World，但那樣准與不准動態插入的 World 不能共用（Kernel DI-KS 26：一個不准就擋整份），也讓「每個 World 用哪些登記單位」說不清。

**選項包**（`OptionBag`）：以型別為鍵、各 Module 讀自己的型別（`setup.Options<AuthorityOptions>()`）；編排層只存、只轉交（統合 O-2 的「`Product` 的 World 描述帶各 Module 自己解讀的選項，編排層在建 World 時轉交、不解讀」）。策略在建 World 時選、在各 Module build 之前（統合 15.2）——選項包在 `Build` 之前就在描述裡，滿足這個時機。

**`WorldCheck` 交給 `Check` 的 World 宣告**：`Check` 跑的時候 World 還沒建（第 6 步才建），所以 `WorldCheck` 交出描述裡的 `dynamicInsert` 與選項包——和之後建 World 用的是同一份值。權威份寫的「讀 World 建立時的宣告」由這一條滿足（統合 L6 的「另交出每個 World 建立時的宣告」，Kernel 那一半是 `World::Options()`）。

**錯誤的出口**：`Product` 把每個 `WorldExecutor` 的錯誤處理器接到自己的 `SetDiagnosticHandler`（預設寫 stderr，和 Kernel 的預設一樣）；Kernel 的錯誤照舊只在處理器記一次（error-handling DI 2），編排層自己的警告（3.10）也只在這裡記。

**執行緒**：這一批的 `Product` 全部在呼叫的執行緒上做——登記、建 World、執行、替換、插入（CCR-5：同一份 Registry 的登記與 World 的建立、銷毀、執行在同一條執行緒）。多執行緒的安排在第 5 條。

**系譜**：Bevy `App` 持有主 World 與子 App（`crates/bevy_app/src/app.rs:1235` `insert_sub_app`）；Unity 建 World 時帶 flag 定角色、`ICustomBootstrap` 讓作者建自己的 World 群（`Unity.Entities/World.cs:90`、`DefaultWorldInitialization.cs:122`）。**退掉的**：Bevy 的「Plugin 自己決定碰哪些子 App」——GYO 每個 World 的描述寫它用哪些登記單位（預設全部），看描述就知道 World 裡有什麼；Unity 用 `WorldSystemFilter` 屬性讓 System 自己選 World，GYO 沒有屬性的機制。

### 3.5 O4：啟動錯誤的彙整

`ProductStartupReport`：
- 每個 World 一段：Kernel 的 `StartupReport`（原封不動，不擴充 Kernel 的 `StartupCode`，O-5），或「通過」；Kernel 失敗時加一行「因 Kernel 錯誤而未檢查：Finish、Check」。
- 編排層自己的錯誤 `OrchestrationCode`：

| 碼 | 意思 | 從哪來 |
|---|---|---|
| O-E1 | 登記單位的名字重複 | 舊 R-E1（`Registry.cpp:753`） |
| O-E2 | 登記單位對不上登記檔（不在、在錯的那一份、那一列不進 World） | 新 |
| O-E3 | 依賴缺、在後面，或 Module 依賴 Plugin | 舊 R-E3（`Registry.cpp:766`），來源從 `Requires()` 換成登記檔 |
| O-E4 | 兩個登記單位裝 runner | 舊 R-E6（`Registry.cpp:1536`） |
| O-E5 | 清單錯誤：插入的錨點不在、同一個名字兩次、名字不是那個 World 的 schedule | 新（Bevy 的 `insert_startup_before` 錨點不在時 panic，`main_schedule.rs:283`；GYO 改成啟動錯誤、一起收齊） |
| O-E6 | World 描述錯誤：沒有 World、名字重複、`units` 指到沒帶上的登記單位 | 新 |
| O-E7 | 準備函式回傳錯誤 | 新 |

- 各登記單位在 `Finish`／`Check` 回報的錯誤：`{登記單位的名字、它自己的碼（字串）、訊息}`（權威的碼是權威 Module 自己的，權威份 4.6）。
- 碼的寫法照 Kernel 的「R-E1」「D-E1」（卡片照 L17 判，前綴 O 取編排層 orchestration）。

### 3.6 O6：遊戲初始化清單與每幀的清單

**遊戲初始化清單**（O-1）：一個 `Product` 一份，是一串 schedule 名。初始內容來自 `ProductDescription::gameInit`，登記單位在 `Configure` 裡用 `setup.GameInit().Append(name)`、`.InsertBefore(anchor, name)`、`.InsertAfter(anchor, name)` 插入。Engine 不給預設的名字（使用者：「內容有作者自己訂，Engine只提供機制」）。
- 跑在**第一個 World**（描述裡的第一個；使用者 `game-init-list` 選「跑在第一個 World」）。
- 行程一次：`RunGameInit()` 依清單順序對第一個 World 各呼叫一次 `WorldExecutor::Run`；第二次呼叫是斷言。`RunFrame()` 在遊戲初始化還沒跑時先跑它（所以「在所有 World 開始每幀之前跑完」由機制保證，不靠 runner 記得）。主程式也可以先自己呼叫 `RunGameInit()` 一次。
- 任何一張回傳錯誤：停下、回傳那個錯誤（第一個 World 已中斷，照 Kernel E.5），遊戲初始化記為已跑，`RunFrame` 不跑每幀的清單。
- 每張 schedule 是一次獨立的最外層 `Run`：各自在開頭重讀 `KernelSettings`（DI-KS 24），遊戲初始化裡改的設定從下一張生效。
- 系譜：Bevy `MainScheduleOrder::startup_labels`（預設 PreStartup、Startup、PostStartup）、`insert_startup_before`／`_after`（`crates/bevy_app/src/main_schedule.rs:218`、`:260`、`:274`），`Main::run_main` 只在第一次跑它們（`:289-297`），只在主 World（子 App 的 update schedule 不是 `Main`）。flecs 的 `OnStart` 在第一次 `ecs_progress` 跑一次（`src/addons/pipeline/pipeline.c:715`、`:995`）。**退掉的**：Bevy 給三個預設名字——使用者定內容由作者訂；「每個 World 各跑一次」——使用者 `game-init-list` 比過、沒選。

**每幀的清單**：每個 World 一份（`WorldDescription::frame`，登記單位在 `Build` 裡用 `setup.Frame()` 插入，同樣三個函式）。`RunFrame()` 依 World 的順序、每個 World 依清單順序 `Run`；有錯就停下回傳。交換點與執行緒在第 5 條。系譜：Bevy `MainScheduleOrder::labels`（`main_schedule.rs:250` `insert_before`）與子 App 各自的 update schedule。

**和現在的 Kernel 開局（K0）的差別**（J-58 的改寫，第 7.1 節組 A）：

| 現在（Kernel，DI-KS 22） | 之後（編排層，O-1） |
|---|---|
| 每個 World 至多一張 Startup（D-E21） | 一個 `Product` 一份清單、可以多張、只在第一個 World |
| 第一次跑**任何**別的 schedule 時，Kernel 先跑它（在同一次 `Run` 裡） | `RunFrame` 先跑它；直接呼叫 `WorldExecutor::Run` 的程式（繞過編排層）不會觸發 |
| System 不能跑它（`StartupNotNestable`）；它執行中不能跑別的（`StartupRunning`） | Kernel 不認得它，這兩條都沒有了；巢狀跑照一般規則（CCR-2） |
| 在推進 tick 之前 | 遊戲初始化的 schedule 是一般的 schedule；設了 clock 就照樣推（第 3 條之前） |
| 只掛在 Startup 的 System 不算寫者（DI-KS 6） | 照樣算（第 4 條拿掉寫者數量之前） |

### 3.7 O7：runner

```cpp
using Runner = std::function<RunnerExit(Product&)>;
// ProductSetup：
void InstallRunner(std::string_view name, Runner runner);   // 一個 Product 至多一個；第二個是 O-E4
```

- 由登記單位在 `Configure` 裡裝（將來視窗的 Plugin 裝每幀迴圈的 runner）；沒人裝時用預設的**只跑一次**：`RunFrame()` 一次（遊戲初始化先跑），成功回 `RunnerExit::Success`，失敗回 `Failure` 帶錯誤。
- `Product::Run()` 呼叫 runner 一次；第二次呼叫是斷言。
- runner 擁有迴圈、決定哪條執行緒跑哪個 World；只呼叫 `RunGameInit`、`RunFrame`、`WorldExecutor::Run`、`ReplaceWorld`、`InsertSystems`（之後加交換點），不排 System 的先後（統合 O-3）。
- 系譜：Bevy `App::set_runner`（`crates/bevy_app/src/app.rs:224`）、預設的 `run_once`（`:1516`）只 `update` 一次；現在 Kernel 的 `FinishContext::InstallRunner` 只記「誰裝」、沒有可呼叫物（`Registry.hpp:59-62`）。**和 Kernel 現在不同**：裝的時機從 `Plugin::Finish` 移到 `Configure`（runner 屬於 `Product`，不屬於某個 World 的 Plan；Bevy 也在 `build` 裡設）。

### 3.8 O8：準備時機（建 World 之後、第一次 Run 之前）

```cpp
using PrepareFn = std::function<Engine::Base::Result<void, OrchestrationError>(Kernel::World&)>;
// WorldSetup（Build 裡）：
void Prepare(PrepareFn fn);
```

- 每個 World 建好之後（啟動時，以及每次替換，3.9）、第一次 `Run` 之前，依序呼叫：**登記單位在 `Build` 裡給的**（照登記單位的順序）→ **World 描述裡產品給的** → （替換時）這一次替換帶的 `extra`。
- 這時 World 是 Ready，`World&` 的 API 都可以用：`SetResource`（放 Resource 的預設值、暫存的存檔資料；也包括配發器——配發器是 Resource，Ready 時設定是 DI-KS 15 第 (7) 項的機制，範圍的下界就在這裡移）、`Spawn` 等外部寫入。Kernel 不知道這是「準備」——它只是 Ready 時從外面寫，Kernel 不多一項職責（L17）。
- 任何一個回傳錯誤：啟動時是 O-E7（`Build` 失敗）；替換時見 3.9。
- 讀檔宣告的三件要求（5.1、5.8）都由這裡給：設配發器範圍的下界、放暫存的存檔資料、放 Resource 的預設值。實體由讀檔的 System 放回——本批由 runner 或每幀清單去跑那張 schedule；第 5 條之後由生成世界跑（讀檔宣告第 8 節）。
- 系譜：Unity `ICustomBootstrap.Initialize` 讓作者在 World 建好後放東西（`Unity.Entities/World.cs:90`）；Bevy 在 `App::run` 之前由 Plugin 或 `main` 用 `insert_resource` 放（Bevy 沒有 World 的替換，所以只有啟動時一次）。名字（卡片照 L17 判）：`Prepare`，不叫 setup（N8 的「組裝（setup）」已指產品啟動的那一段）。

### 3.9 O9：執行中替換 World

`Product::ReplaceWorld(name, extra)`，只能在兩次 `Run` 之間呼叫（在任何一個 World 的 `Run` 裡呼叫是斷言；CCR-5）：
1. 用**同一份 Registry**、同一份 `WorldOptions` 建新的 `WorldExecutor`。
2. 依 3.8 跑準備：登記單位的、描述的，最後是 `extra`（讀檔：讀這一份存檔）。
3. 準備全部成功：銷毀舊的 World，新的放進同一個位置（同一個名字、同一份每幀清單）。**任何一個失敗**：銷毀新的、舊的照跑、回傳錯誤（O-E7）並記一次警告。
4. 遊戲初始化**不重跑**（它是行程一次；第一個 World 被換掉也一樣）。

**「經開局」對新 World 的意思**：`new-world-default` 的原話（11-16）是「建一個新的 World，經開局與 Loading，由作者的讀檔 System 把存檔內容放回」。當時「開局」是 Kernel 的每個 World 一次的 Startup；之後 `game-init-list` 把它切成遊戲初始化（行程一次、第一個 World）與生成世界（每個 World 各自），`two-layers-like-bevy` 放進編排層。讀檔宣告第 15 節的確認清單（使用者 2026-11-17 核准）寫明：讀檔建的新 World 走生成世界、遊戲初始化不再跑。所以本批的讀法：**「經開局」＝經這個 World 的準備與生成世界，不經遊戲初始化**。生成世界在第 5 條；本批之前，新 World 是準備之後就進 runner。

**同一份 Registry**（卡片照 L17 判）：
- Kernel 自己把「登記」（Registry：型別、System、Plan）與「資料」（World）分開，DI-KS 14 寫「世代切換＝重建 World」——換的是資料，不是登記。
- 第 5 條的 extract 讓一個 World 的 Registry 指向另一個 World 的 Registry 當來源（`SetExtractSource`，只能在第一次建 Plan 之前設）；換 World 時換掉 Registry，依賴它的 Registry 會指向已銷毀的物件。
- 讀兩次相同（讀檔宣告判定 1 案例 10）：同一份 Registry，編號與 Plan 一樣。
- 代價：之前動態插入的 System 留著（新 World 用的是插入之後的登記）。要連登記都重來，重建整個 `Product`。說明頁寫明。
- 別家：Unity 換 World 時由 bootstrap 重新建 System（Unity 的 System 屬於 World）；Bevy 沒有替換 World。GYO 的 System 屬於 Registry，照 Kernel 自己的分法。

**先建新的、再丟舊的**（卡片照 L17 判）：讓讀檔失敗時舊的照跑，玩家不會掉進半空的世界；也讓「帶過去」（第 13 節第 1 件）做得到。代價：替換的當下兩個 World 同時在記憶體裡（峰值約兩倍）。

**System 端的請求通道不在本批**：玩家在選單按「讀取」，是 System 處理輸入；它要讓 runner 知道「換 World」。本批之前，作者用自己的 Resource，自己的 runner 在兩幀之間讀它、呼叫 `ReplaceWorld`（Engine 不用加東西）。Engine 給的通道照 Bevy `AppExit`（`bevy_app` 登記的一種訊息，`app.rs:130`、`:1406` `should_exit`）——編排層登記進每個 World 的一種訊息、每幀之後讀；在第一個每幀迴圈的 runner 那一批做（第 3 條之後，訊息已改名 `Message`）。

### 3.10 O10：動態插入的編排半

`Product::InsertSystems(world, owner, build)`，兩次 `Run` 之間：
1. `registry.Register(owner, build)`：回 `DynamicInsertRefused` → 記一次警告（`ProductDiagnostic`，寫 World 名、owner、「這個 World 宣告不准動態插入」），回傳錯誤；Registry 與 World 都沒變（CCR-3）。
2. `registry.BuildPlans()`：失敗（例：R-E7）→ 記一次警告（帶 Kernel 的報告）、回傳錯誤；Registry 與 World 留在上一個版本（DI-KS 26）。
3. 成功：World 下一次被用到時自己接上（DI-KS 26）；編排層持有的唯讀登記表參照照 CCR-4 重讀。
- **不重跑 `Finish` 與 `Check`**（卡片照 L17 判；和統合 O-2 的「執行中的插入（指令等）照同一條路再走一次」不同，**列進第 13 節第 2 件請你確認**）。理由：Kernel 的 `BuildPlans` 成功就換上新版本（第 1 批 3.2 規則 1），換上之後才跑的檢查失敗，Kernel 撤不回（第 1 批宣告第 11 節）——只能「報錯但已經插進去了」；需要檢查保證的策略本來就要求它的 World 不准動態插入（伺服器權威，權威份 4.5）；Bevy、flecs、Unity 執行中加 System 都不重跑 Plugin 的 finish 或任何檢查。
- 指令集從 System 呼叫插入（「現在啟動的world沒有系統，但是動態插入可以讓我使用指令」）同樣需要 System 端的請求通道，和 3.9 一起留到每幀迴圈的 runner 那一批。

### 3.11 O11、O12：Kernel 拿掉的

| 拿掉 | 現在 | 之後 |
|---|---|---|
| `ScheduleOptions::startup`、D-E21 | `Declarations.hpp:125`；`Registry.cpp:979-987` | 遊戲初始化清單（3.6） |
| 執行期的開局：`WorldExecutor::RunStartup`、`startup_`、Run 開頭的自動觸發、主程式跑 Startup 的斷言；`World::startupRan_`、`startupRunning_` | `Executor.cpp:39-40`、`:129-136`、`:201-213`、`:233-235`；`World.hpp:475-477` | `Product::RunGameInit`／`RunFrame`（3.6） |
| `StartupNotNestable`、`StartupRunning` 與巢狀入口的兩個檢查 | `KernelError.hpp:64-67`；`Executor.cpp:513-524` | 沒有（Kernel 不認得開局） |
| 寫者計數的開局豁免 | `Registry.cpp:1461-1469`（DI-KS 6） | 沒有（遊戲初始化的 System 照樣算寫者，到第 4 條拿掉 D-E2 為止） |
| R-E7 的「開局的 schedule」「開局 schedule 上的掛載」兩支 | `Registry.cpp:1624-1640` | 沒有（沒有開局的 schedule 這個概念） |
| `Plugin`、`FinishContext`（含 `InstallRunner`）、`Registry(std::vector<Plugin*>)`、`Registry::Finish()`、`data.plugins`、`CheckPluginList`、`CheckRunners`；R-E1、R-E3、R-E6 | `Registry.hpp:51-87`、`:657-705`；`Registry.cpp:753`、`:766`、`:1536`、`:1823-1860` | 編排層的 `Module`／`Plugin`、`Product::Build`（3.3、3.4）；碼換成 O-E1、O-E3、O-E4 |
| 指紋裡的 `startup` 旗標 | `Fingerprint.cpp:194`、`:432` | Registry 指紋 3→4、Plan 指紋 5→6（Plan 的規則變了：Kernel 不再在第一次 Run 前跑開局，照 hook-flush 定的「規則變就升版」）、C 指紋 7→8（它和 Registry 指紋共用 schedule 的位元組）；golden 重產 |

留在 Kernel：`RegistrationOwner`（owner 由編排層給，統合 3.5）、`Registry()`、`Register`／`BuildPlans`／`DiscardRegistrations`、`WorldOptions`、R-E2、R-E4、R-E5、R-E7（其餘理由）。Kernel 的測試輔助 `Finish(builds)`（`KernelTestSupport.hpp:143`）改成每一對 `(名字, build)` 一次 `Register`、最後 `BuildPlans`；用到 `Plugin::Finish` 或清單順序的案例搬到編排層（第 7 節）。

---

## 4. 要改的程式與文件

| 檔 | 改什麼 |
|---|---|
| `engine/orchestration/`（新）：`CMakeLists.txt`、`include/engine/orchestration/{Module,Plugin,Product,ProductDescription,RegisteredUnits,OrchestrationError}.hpp`、`detail/RegistrationUnit.hpp`、`src/Product.cpp` 等 | O1～O4、O6～O10（檔名照 N16，實作時可以合併） |
| `engine/CMakeLists.txt` | `add_subdirectory(orchestration)`（在 kernel 之後） |
| `engine/config/modules.csv`、`plugins.csv`（新）；`build/cmake/GyoUnitRegistry.cmake`（新）；`build/cmake/GyoBuild.cmake`（include 它） | O5 |
| `engine/{base,math,entity_store,kernel,os,time,threads,io,audio}/CMakeLists.txt` | 引擎內的 `GYO::<Module>` link 換成 `gyo_link_registered_dependencies(<name>)`；第三方照舊 |
| `engine/kernel/include/engine/kernel/{Registry,Declarations,KernelError,World,WorldExecutor}.hpp`、`detail/{RegistryData,Context}.hpp`；`src/{Registry,Executor,Fingerprint,World}.cpp` | O11、O12 |
| `tests/common/orchestration/`（新）：`CMakeLists.txt`、`*Tests.cpp`、合成的登記檔 `fixtures/` | 第 7.3 節的新案 |
| `tests/common/cmake/UnitRegistryTests.cmake`（新） | 登記檔格式的 `cmake -P` 測試 |
| `tests/common/kernel/*`（`NestedRunTests`、`RegistryTests`、`CoverageTests`、`RegistrationRoundTests`、`HookFlushTests`、`ZoneRuleTests`、`SchedulerTests`、`SignatureTests`、`KernelTestSupport.hpp`、`FinishRecord.cpp`、`WorldExecRecord.cpp`、`ThreadBoundProbe.cpp`）；守衛腳本 `kernel_guard.py`、`naming_guard.py`、`kernel_symbols.py` 與其測試 | 第 7 節；掃描根加 `engine/orchestration` |
| `docs/architecture/plans/kernel/scripts/mutations.json` | 第 8 節 |
| `docs/architecture/target/kernel.zh-Hant.md` | DI-KS 6、15（過渡表那一列）、22、24、25、26，RC 7、13（第 9.1 節） |
| `docs/architecture/target/v2/orchestration.zh-Hant.md`（新） | 編排層的 Design Invariants 與 Replacement Criteria（第 9.2 節） |
| `docs/architecture/target/v2/system-contract-page.zh-Hant.md` | 說明頁加「編排層」一節：用遊戲講生命週期、遊戲初始化、準備、替換、動態插入的警告 |
| `docs/cross-component-rules.md` | CCR-3 的 Owner 欄加上編排層是第一個實作者；新增一列（如有）由實作時照登記頁的手順 |
| `docs/dev_logs/` | 本批的 dev_log |

---

## 5. 基盤 Architecture 的根據（AGENTS §2）

### 5.1 為什麼算基盤

編排層的登記單位型別、生命週期、`Product` 的介面與登記檔格式，上面會積：權威、回滾、時間、狀態機、網路、audio 這些 Module（全部以登記單位接進來）；生成世界、多個 World 與交換點、讀檔的實作（全部在 `Product` 上）；每一個產品的建置描述（`gyo_product_units`）。改它們，上面全要跟著改，而且積得越多改得越多。判斷不出的部分照基盤處理。

### 5.2 預見的根據 → 基盤的要求

| 根據（出處；種類） | 對編排層的要求 | 這一批 |
|---|---|---|
| 玩家存檔、遊戲中讀檔（使用者 11-16；名指的 Feature；Product：單機） | 建 World 後、第一次 Run 前的時機；執行中替換 World；失敗時保留舊的；遊戲初始化不重跑 | 做（3.8、3.9） |
| 指令集（使用者 11-17「未來我自己要做指令集」；名指的 Feature；Product：未指定） | 插入入口；拒絕時回錯誤、記一次警告、World 照跑 | 做 Kernel 之外的那一半（3.10）；System 端的通道留介面形狀 |
| 伺服器權威（使用者 11-11、11-16；名指的 Feature；Product：PvP Match） | 策略在建 World 時選；build → Plan → finish → 檢查；檢查讀 World 的宣告；Kernel 錯誤時不檢查 | 做時機與彙整（3.3～3.5）；權威 Module 在第 4 條 |
| 單機不上權威（使用者 11-17 L15；名指；Product：單機） | 產品能只選部分 Module；預設全帶可關 | 做（3.2） |
| 多個 World（使用者 11-11、11-14；名指；Product：PvP Client 三個、Editor 兩個） | `Product` 持有 World 群；每個 World 自己的登記與登記單位 | 做 World 群與每個 World 一份 Registry；交換點、執行緒在第 5 條 |
| 狀態機的「進入初始狀態」要讀到設定（使用者 11-15 的 Bevy 例子、11-16 `game-init-list`；名指；Product：所有遊戲） | 遊戲初始化清單可以在指定的 schedule 前後插入 | 做（3.6） |
| 每幀迴圈由視窗的 Plugin 推（舊組成文件；參考實作 Bevy `WinitPlugin` 設 runner） | runner 由登記單位裝、一個 Product 一個 | 做介面與只跑一次的 runner；每幀迴圈的 runner 留到第一個需要它的批 |
| 讀取畫面、分批載入（使用者 11-11；名指；Product：PvP Client） | 生成世界在編排層、每個 World 各自 | 不做（第 5 條）；本批的準備時機與替換是它的前提 |
| Module 之間的依賴（使用者 11-17 L20） | 依賴只寫在登記檔；編排層讀出並檢查 | 做（3.2） |
| 參考實作（Bevy 6a911653f 的 `bevy_app`；Unity Entities 1.5.0；flecs；O3DE、Unreal 的清單機制只讀過文件與統合的證據） | 生命週期分 build／finish；預設的 run-once runner；開局只跑一次、只在主 World；清單產生接上的程式 | 「怎麼做」的根據，見各節系譜 |

規模與預算：本批不碰執行效率；替換 World 的當下兩個 World 同時在記憶體裡（3.9），沒有預算數字可以比，不量。

### 5.3 Feature 與 Product

| Feature | Product | 誰點名 |
|---|---|---|
| 玩家存檔（含遊戲中讀檔） | 單機遊戲 | 使用者 11-16 |
| 指令集 | 原話沒有指定 Product | 使用者 11-17 |
| 伺服器權威 | PvP Match／Client | 使用者 11-11、11-16 |
| 不選權威時沒有限制、只選部分 Module | 單機 | 使用者 11-14、11-17 |
| 多個 World | PvP Client、Editor | 使用者 11-11、11-14 |
| 遊戲初始化可插入 | 所有遊戲（狀態機的例子） | 使用者 11-16 |

---

## 6. Architecture Delta（AGENTS §3 的七項）

1. **需要它的 Feature 或目的**：第 5.3 節；批次表第二段第 2 條（統合核准的分層落地）。
2. **現在的問題**：Kernel 同時是 ECS 與組裝層——它認得開局（`ScheduleOptions::startup`、DI-KS 22）、登記單位（`Plugin`、`Requires`、`InstallRunner`），和使用者定的「Kernel 的職責是 ECS」（L17）、「編排層認得階段、Kernel 不認得」（`two-layers-like-bevy`）相反；讀檔要的「建 World 後、第一次 Run 前的時機」與「執行中替換 World」沒有地方放；第 1 批留下的 `DynamicInsertRefused` 警告沒人記；登記檔寫了依賴、CMakeLists 又寫一次會是兩處。
3. **變化的邊界**：新 Module `engine/orchestration`（新的 Subsystem）；Kernel 的公開介面減少（第 3.11 節）；建置層新增 Module／Plugin 登記檔與 `gyo_link_registered_dependencies`、`gyo_product_units`；引擎各 Module 的 CMake 依賴改由登記檔產生（Build Graph 的構造變更）；Registry、Plan、C 指紋的格式。
4. **影響的 Module／Product／Tool**：`engine/kernel`；新 `engine/orchestration`；`engine/` 下九個 Module 的 CMakeLists（只換 link 的寫法，依賴邊不變）；`build/cmake`；`tests/common/kernel`（約 20 案改寫或搬走、測試輔助改兩步）、新 `tests/common/orchestration`、`tests/common/cmake`；突變清單；之後的每個 Module 與產品（以登記單位與 `gyo_product_units` 接進來）。gyo2 上沒有產品與工具。
5. **Dependency Direction**：新增 編排層 → Kernel、編排層 → Base；建置層的產生物（產品的 `<target>_units.cpp`）→ 編排層與它帶上的登記單位。Kernel 不依賴編排層；編排層不依賴任何 Module 的內部、任何第三方。引擎內既有的依賴邊一條都不變（只換寫法），由 G-3 的掃描與 configure 的成功確認。
6. **Ownership**：登記單位與生命週期、runner、遊戲初始化、World 群、準備、替換、動態插入的警告、啟動錯誤的彙整：Kernel（或沒有人）→ 編排層。Module 之間依賴的事實：各 CMakeLists → 登記檔。owner 都是架構（架構師會話）。
7. **為什麼不能更小**：拿掉開局與 `Plugin` 而不建編排層，Kernel 的測試與遊戲初始化沒有新家（統合第 8 節「拿掉某條限制和接手它的新家放在同一條線」）；只建 `Product` 不做登記檔，依賴只能寫在程式碼（`Requires`），和使用者定的 L20 相反；登記檔寫依賴而 CMakeLists 不讀，依賴兩處（AGENTS §9）；準備與替換不在本批，讀檔的實作（第 5 條）沒有前提。比過、沒採的較小做法：「`Kernel::Plugin` 原地留著、編排層包一層」——Kernel 照樣認得登記單位與 runner，L17 的職責清單不成立。

**和統合（與批次表）不同的**：
1. 動態插入不重跑 `Finish` 與 `Check`（統合 O-2 草案寫「照同一條路再走一次」；第 13 節第 2 件）。
2. 登記單位多一個產品層一次的 `Configure`（統合 O-2 只寫 build／finish／檢查；3.3）。
3. runner 在 `Configure` 裝，不在 finish（Kernel 現在的 `FinishContext::InstallRunner`；3.7）。
4. golden 重產多一次（統合第 8 節算三次：結構段、時鐘那條線、權威那條線；拿掉開局的 `startup` 旗標讓本批也升版，3.11）。
5. 引擎各 Module 的 CMake link 改由登記檔產生（統合 L20 只說「給 CMake 與編排層兩邊讀的格式」，沒有說既有 Module 的 link 要不要改；3.2）。
6. 準備時機的形狀（統合修訂 33 說「形狀在這一批的宣告定」；3.8）與 World 替換（讀檔卡修訂 11 的要求，統合沒有寫；3.9）。
7. 時間估計 16～23 h（統合第 8 節 10～14 h；第 11 節）。

---

## 7. 守衛與測試的變更（AGENTS §14.4）

開局與登記單位相關的案例由本卡讀程式盤點（`inventory-startup-plugin.md`，關鍵字掃描、會多算）；逐案的處置在實作開始時由實作者照下表的組再核一次，和表不同的照 AGENTS §14 停下回報。**緩的面**＝以前判失敗、之後判通過的情形；**緊的面**＝以前判通過、之後判失敗的情形。

### 7.1 有緩的面的（要你逐組核准）

| 組 | 1. 守的性質 | 2. 緩的面 | 3. 緊的面 | 4. 性質之後由誰守 | 案例（處置） | 5. 根據 |
|---|---|---|---|---|---|---|
| A 開局（**DI-KS 22 的守衛**） | 一個 World 至多一張 Startup；Kernel 在第一次跑別的 schedule 前、在同一次 Run 裡、推 tick 之前跑完它一次；System 不能跑它、它執行中不能跑別的；主程式明確呼叫只准一次 | 一個 World 可以有多張「只跑一次」的 schedule（D-E21 沒有了）；System 可以巢狀跑遊戲初始化的 schedule、遊戲初始化中可以巢狀跑別的；直接呼叫 `WorldExecutor::Run` 不會先跑遊戲初始化；遊戲初始化的 schedule 設了 clock 就推 tick；第二個以後的 World 不再各自自動跑一張 | 清單的順序（含 before／after 插入）成為守的性質；錨點不在、名字不是第一個 World 的 schedule 是啟動錯誤（O-E5）；`RunFrame` 一定先跑完遊戲初始化 | 「整份一次、在第一幀之前、只在第一個 World、明確呼叫只准一次、錯誤時不跑每幀」：編排層 O-1 的新案（7.3）。巢狀與 tick 的部分：不由機制守（Kernel 不認得開局，`two-layers-like-bevy`） | `NestedRunTests` :1370、:1425、:1474、:1480、:1486（改寫搬到編排層）；:1393（拿掉：tick 隨第 3 條，遊戲初始化是一般 schedule）；:1410（拿掉：「最後一個同步點的 hook 紀錄由它自己的清空輪套用」是任何 schedule 的性質，`HookFlushTests` :337 改寫後守）；:1433、:1450、:1464、:867（拿掉）；:1500（拆：「先於每幀」改寫搬走；clock／巢狀的部分拿掉）；:1792、:1832（拿掉：隱含觸發沒有了，驗證對任何 schedule 一樣） | `game-init-list`、`two-layers-like-bevy`；統合 7.2 J-58 列 |
| B 登記單位的清單（J-19 的登記單位那半、J-24 的 R-E1／R-E3／R-E6） | 清單裡名字重複是 R-E1、重複的不 build；`Requires` 缺或在後是 R-E3、不重排；兩個 runner 是 R-E6；build 每個一次、全部 build 完才 finish、照清單順序 | 不經編排層、直接用 Kernel 兩步登記的程式不受這些檢查（Kernel 不認得登記單位）；`Finish` 移到建 Plan 之後，「`Plugin::Finish` 讀到版本 0」不再成立 | 依賴的來源從程式碼改成登記檔：登記單位不在登記檔、在錯的那一份、那一列不進 World 是 O-E2（新）；Module 依賴 Plugin 是 O-E3（新）；`Finish` 讀得到 Plan 與版本 1 | 編排層 O-2 的新案（O-E1、O-E3、O-E4 與順序，7.3） | `RegistryTests` :142、:164、:177、:227 的 R-E6 段、:251 的 R-E1／R-E3 段（搬）；`CoverageTests` :86 的 `FinishContext` 段（搬：`WorldFinish` 不能複製、移動）、:130（搬：`Finish` 看得到解析後的集合與存取）；`RegistrationRoundTests` :836（拿掉：Kernel 沒有 `Plugin::Finish`；編排層新案「插入之後不重跑 `Finish`」）、:901 的「version inside Plugin::Finish」段（拿掉；編排層新案改成「`Finish` 讀到版本 1」；同一案的「之後每次成功加一、失敗不加」留在 Kernel） | `module-deps-in-registers`、`module-plugin-registers`；統合 L5、L20、7.2 J-19、J-24 列 |
| C R-E7 的開局兩支 | 之後一輪登記開局的 schedule、或在開局 schedule 上掛 System，是 R-E7 | 沒有「開局的 schedule」，之後一輪登記任何 schedule 都照一般規則 | 無 | 不由機制守（概念沒有了） | `RegistrationRoundTests` 的三處（拿掉開局那兩支的斷言，其餘留） | 同 A |

### 7.2 沒有緩的面的（同一個性質換觀察方式、期待值隨定義變、或只緊不緩）

§14.4：一般測試由實作者改、事後報告；**Design Invariants 的守衛測試不論有沒有緩的面都要你核准**——下表的「DI-KS 15」「DI-KS 24」「DI-KS 25」「DI-KS 26」各列與指紋的 golden（DI-KS 3）都在第二段核准裡。

| 組 | 案例 | 改什麼 | 為什麼沒有緩的面 |
|---|---|---|---|
| D DI-KS 15 的碼清單 | `RegistryTests` :274、:288 | 清單拿掉 D-E21、R-E1、R-E3、R-E6；「隨哪一批拿掉」那一列刪掉 | 准許的碼變少，只緊 |
| E 開局的寫者豁免（DI-KS 6） | `ZoneRuleTests` :188（拆：Startup 那半拿掉、Shared 那半留）；`NestedRunTests` :1918（拿掉：開局豁免沒有了；`Writes<…>` 不算寫者已由第 1 批的案守） | — | 以前不算寫者的現在算，只緊（到第 4 條拿掉 D-E2 為止） |
| F `KernelSettings` 在開局改 | `NestedRunTests` :1324（拿掉；編排層新案「遊戲初始化改的設定從下一張 schedule 生效」） | — | 性質「下一次最外層 Run 生效」由 J-50 的一般案守；遊戲初始化每張是獨立的 Run，不再有「同一次 Run 裡先跑開局」的特例 |
| G hook 清空輪跑一次的 schedule | `HookFlushTests` :337 | 拿掉 `startup = true`，斷言不變 | 同一個性質（任何 schedule 最後一個同步點的 hook 紀錄由它自己的清空輪套用） |
| H 測試輔助改兩步 | `KernelTestSupport.hpp:143` 與用它的全部案例；`FinishRecord.cpp`、`WorldExecRecord.cpp`、`ThreadBoundProbe.cpp` | `Finish(builds)` 改成每對一次 `Register`、最後 `BuildPlans`；`TestPlugin` 刪掉 | 輔助沒有用到 `Plugin::Finish`；owner 的名字與順序照舊，所以 id 與指紋的輸入不變（golden 只因格式升版而重產） |
| I 指紋 | `RegistryTests` :43；`SchedulerTests` :152、:618；`HookFlushTests` :449、:865；`SpawnRejectTests` :789；`InitialValueTests` :684、:897；`CoverageTests` :501（第 1 批 5.2 的同一批 golden） | golden 與版本（Registry 4、Plan 6、C 8） | 格式改版，判定（跨平台一致、決定性、不等式）不變 |
| J 掃描 | `kernel_guard.py`、`naming_guard.py`、`kernel_symbols.py` 與其測試 | 掃描根加 `engine/orchestration`；`test_kernel_symbols.py` 裡的 `Plugin` 例改掉 | 範圍變大，只緊 |

### 7.3 新增的案例

Red 的做法（§14.2、§14.3）：新介面先加宣告與空的實作（建置得過、行為沒有），新案在它上面跑出失敗，再補實作看到通過；每個新案至少被一個專屬的突變在預期的訊息下殺掉（第 8 節、G-4）。測試用合成的登記單位與合成的登記檔（AGENTS §7）。

| 項 | 案例（`tests/common/orchestration/`） |
|---|---|
| O-2 生命週期 | 兩個 Module、一個 Plugin、兩個 World：`Configure` 各一次、在任何 `Build` 之前；每個 World 裡 `Build` 照順序（Module 先）、全部 build 完才 `BuildPlans`、之後 `Finish` 照順序、全部 finish 完才 `Check`；`Finish` 讀到版本 1 與 Plan；World 的 `units` 只列一個時另一個不 build |
| O-5 彙整 | 一個 World 的 Kernel 啟動錯誤加另一個 World 的 `Check` 錯誤加 O-E5：一份報告三段都在；Kernel 失敗的那個 World 的 `Finish`、`Check` 沒被呼叫、報告寫「未檢查」；沒有任何 World 被建（`Product::Build` 回 Err、沒有任何準備函式被呼叫） |
| O-E1～O-E7 | 每個碼一個最小的 `Product`；全部出錯的一份描述一次回報（照 J-24 的「全部收齊」） |
| 依賴（O-E3） | 依賴缺、依賴在後（不重排）、Module 依賴 Plugin、不進 World 的依賴視為在、World 的 `units` 漏掉依賴（另一個 World 有也不算） |
| O-1 遊戲初始化 | 清單三張、一張用 `InsertBefore` 插、一張 `InsertAfter`：跑的順序；只在第一個 World 跑（第二個 World 的同名 schedule 沒跑）；`RunFrame` 兩次只跑一次、在第一幀的第一張之前；先 `RunGameInit` 再 `RunFrame`：一次；`RunGameInit` 第二次、`RunFrame` 之後再 `RunGameInit`：斷言；一張回 Err：`RunFrame` 回它、每幀清單一張都沒跑、處理器只被呼叫一次；清單空：`RunFrame` 直接跑每幀；遊戲初始化改的 `KernelSettings` 從下一張生效 |
| O-3 runner | 沒人裝：`Run()` 跑遊戲初始化與一幀、回 Success；裝了：runner 被呼叫一次、拿到同一個 `Product`；`Run()` 第二次斷言 |
| O-8 準備 | 準備的順序（登記單位照順序、再描述的）；準備時 World 是 Ready、還沒有任何 schedule 跑過（每幀 System 遞增的計數不在）；第一幀的 System 讀到準備放的值；準備回 Err：O-E7、`Build` 失敗 |
| O-9 替換 | 換掉之後：舊的實體與被丟的紀錄都不在、Resource 回到準備給的值；同一份 Registry（`Types().Version()` 不變、指紋相同）；遊戲初始化沒有重跑（計數）；每幀清單照舊；`extra` 在最後跑；`extra` 回 Err：舊的 World 照舊（實體、Resource、還能再跑一幀）、回傳 O-E7、警告一次；在 Run 裡呼叫：斷言 |
| O-10 動態插入 | 准的 World：`InsertSystems` 成功、新 System 下一幀跑；不准的 World：回 `DynamicInsertRefused`、警告處理器被呼叫一次（帶 World 名與 owner）、World 照跑、Registry 版本不變；建 Plan 失敗（R-E7）：回錯誤、警告一次、World 留在舊版本；`Finish`／`Check` 沒有被再呼叫 |
| 預設全帶（產生的表） | 合成的登記檔（兩個進 World 的 Module、一個 Plugin、一個不進 World 的 Module）建一個測試 target：預設帶兩個 Module、不帶 Plugin；`DISABLE_MODULES` 關一個；`PLUGINS` 帶 Plugin；關掉被依賴的那個 → `Product::Build` 回 O-E3 |
| 登記檔格式（`cmake -P`） | 表頭不對、名字重複（跨兩份）、依賴不存在、依賴在後、Module 依賴 Plugin、`library_only=yes` 有 `unit`、`no` 缺 `header`：各自 `FATAL_ERROR` 且訊息含檔名、列、原因；正確的檔通過 |
| 讀檔（**讀檔宣告要求的案例**） | 見第 10 節判定 2 |

---

## 8. 突變的處置

依 D184：每批只跑相關的突變（新的、`find` 在改動範圍的、殺它的測試被改的），PR 線合併前背景完整跑一次。

| 處置 | 突變 | 理由 |
|---|---|---|
| 退役（性質拿掉） | `ns-startup-nestable`、`ns-startup-running-allows-nested`、`ns-st-clock-only`、`ns-st-tick-first-read`、`ns-st-tick-first-error`、`ns-st-tick-stamp`、`ns-st-running-not-cleared`、`ns-st-no-retake`、`ns-two-startups-allowed`、`ks-s2-re7-startup-schedule`、`ks-s2-re7-startup-mount`、`rs-j15-startup-counted`、`ks-s7-startup-change-same-run`、`ns-st-settings-after-startup`、`hf-startup-skipped`（`HookFlushTests` :337 改寫後若它的 `find` 仍在，改寫而不退役） | 第 7.1 節組 A、C，第 7.2 節組 E、F、G |
| 搬到編排層、改寫 `find`／`expect` | `ns-st-not-auto`、`ns-st-not-marked-auto`、`ns-st-not-marked-explicit`、`ns-st-explicit-again`、`ns-st-after`、`ns-st-error-ignored`、`ns-st-error-not-reported`（→ O-1）；`rs-j19-duplicate-built`、`rs-j19-requires-unchecked`、`rs-j24-re6-removed`、`ks-s2-plugin-finish-again`（→ O-2、O-E1、O-E3、O-E4）；`ks-s3-version-before-finish`（→「`Finish` 讀到版本 1」） | 第 7.1 節組 A、B |
| 改寫（`find` 碰到改動，性質留在 Kernel） | `rs-j18-plugins-reversed`（id 照 `Register` 的順序）、`rs-j19-type-twice-allowed`、`rs-j19-unregistered-silent`、`ks-diks15-code-without-text`（拿掉的 `DE21` 換成留下的碼）、`rs-j24-*` 裡 `find` 在 `Registry::Finish` 附近的 | `find` 的那一行被改或刪 |
| 新增 | 編排層每個新案至少一個專屬突變；代表性的：`Configure` 在 `Build` 之後、`Finish` 在 `BuildPlans` 之前、`Check` 夾在 `Finish` 之間、Kernel 失敗時照樣 `Finish`、O-E1～O-E7 各拿掉一個、依賴不看順序、不進 World 的依賴也要求在清單、遊戲初始化每幀都跑、遊戲初始化跑在最後一個 World、`InsertBefore` 插成 after、`RunFrame` 不先跑遊戲初始化、遊戲初始化 Err 時照跑每幀、準備在第一次 Run 之後才呼叫、準備的順序反過來、替換時先丟舊的再建新的、`extra` 失敗時換上新的、替換時重建 Registry、替換時重跑遊戲初始化、拒絕時不記警告、拒絕時記兩次、插入後重跑 `Finish`、預設不帶 Module、`DISABLE_MODULES` 無效、Plugin 預設帶、登記檔驗證的每一條拿掉一個 | 第 7.3 節 |

每個退役或搬走的突變在清單裡寫理由；改寫與新增的突變的預期失敗寫成該突變專屬的訊息或案例名（§14.2）。新的守衛測試（第 9.2 節 O-1～O-9 的守衛）與突變清單，交和作者不同的模型做一次敵對式檢查（G-8）。

---

## 9. Design Invariants 的變更（逐條要你核准；最終文字在第二段核准）

### 9.1 Kernel（`target/kernel.zh-Hant.md`）

| DI／RC | 處置 | 改寫後的要旨 |
|---|---|---|
| DI-KS 6 | 刪「非 Startup」「只掛在 Startup 的 System 不算寫者…（DI-KS 22）」 | 寫者以 System 計，整個 World 的所有 schedule；其餘照舊、整條隨第 4 條拿掉 |
| DI-KS 15 | 過渡表刪第一列（開局、`Plugin`、`InstallRunner`、R-E1、R-E3、R-E6）；正文的「不認得開局」照舊、加「不認得登記單位與 runner」 | 守衛「DI-KS 15: the StartupCode values …」的清單同步 |
| DI-KS 22 | **拿掉**，保留編號（同 DI-KS 23 的寫法）：「移到編排層的 O-1（`target/v2/orchestration.zh-Hant.md`）」 | — |
| DI-KS 24 | 刪「開局在同一次 `Run` 裡先跑時不再讀」與守衛「開局改的值下一次 Run 生效」 | — |
| DI-KS 25 | 刪「`Plugin::Finish` 裡 0」「`Plugin::Finish` 讀的就是正在建的表」 | 「建好前 0、第一次成功 1」 |
| DI-KS 26 | 刪「schedule（開局的除外）」「開局的 schedule 與它上面的掛載」 | R-E7 的清單同步 |
| RC 7「Host 又長回來」 | 移到編排層的 RC（統合 9.1：「保留原條件、改寫主詞」） | 見 9.2 RC-O1 |
| RC 13 | 「plugin」改「登記單位」 | — |

### 9.2 編排層（新頁 `target/v2/orchestration.zh-Hant.md`，見出し `Design Invariants`）

- **O-1 遊戲初始化**：一個 `Product` 一份清單（schedule 名），行程一次、在所有 World 的第一幀之前跑完、跑在第一個 World；`RunFrame` 先跑完它；明確呼叫只准一次；清單內的先後照插入（before／after／append），錨點不在是啟動錯誤；編排層保證「整份一次、在前、清單的順序」，不保證清單外的事。守衛：J-58 改寫的 O-1 各案。
- **O-2 登記單位與生命週期**：產品層一次 `Configure`；每個 World：照順序 `Build`（Module 先）→ Kernel `BuildPlans` → 照順序 `Finish` → 照順序 `Check`；Kernel 有啟動錯誤的 World 不跑 `Finish`、`Check`；有任何錯誤就不建任何 World；動態插入只走 Kernel 的兩步、不重跑 `Finish` 與 `Check`（**看第 13 節第 2 件**）；World 描述的選項包只轉交、不解讀。守衛：O-2 各案。
- **O-3 runner**：一個 `Product` 至多一個 runner，由登記單位在 `Configure` 裝；沒有時只跑一次；runner 擁有迴圈與執行緒的安排，只呼叫 `Product` 的執行入口與 `WorldExecutor::Run`，不排 System 的先後。守衛：O-3 各案；掃描（`engine/orchestration` 不讀 Plan 的步驟、不呼叫 System）。
- **O-4 職責**：**編排層的職責是組裝與流程**（L17）。職責清單：登記單位與生命週期；依賴的讀取與檢查；`Product`（World 群、runner、World 的準備與替換）；遊戲初始化與每幀的清單；生成世界的流程（第 5 條）；交換點、extract 的時機、執行緒的安排（第 5 條）；啟動錯誤的彙整與編排層自己的警告。不帶任何第三方能力；不認得產品、策略的語意、時鐘、回滾。守衛：依賴只有 Kernel 與 Base（登記檔）；`#include` 掃描；`naming_guard`。（「World 的準備與替換」寫進 `Product` 那一項之內，是 O-4 原文「`Product`（World 群與 runner）」的細分；「替換時帶過去的 Resource」若照第 13 節第 1 件的建議做，也寫在這一項之內——**這個歸法一起請你確認**。）
- **O-5 啟動錯誤一次收齊**：Kernel 的 `StartupReport`、編排層的 `OrchestrationCode`、各登記單位的錯誤收成一份；Kernel 的 `StartupCode` 不擴充。守衛：O-5 各案。
- **O-6 依賴只在登記檔**：Module／Plugin 之間的依賴只寫在 `engine/config/modules.csv`、`plugins.csv`；CMake 的引擎內 link 與產品的登記單位表由它產生；編排層建立 `Product` 時檢查（缺、在後、Module 依賴 Plugin 是啟動錯誤，不重排）；Module 預設全帶、產品可以關，Plugin 由產品列出。守衛：掃描（引擎各 CMakeLists 沒有引擎內的 `GYO::<Module>` link）；登記檔的 `cmake -P` 測試；依賴各案。
- **O-7 準備時機**：每個 World 建好之後（啟動與每次替換）、第一次 Run 之前，照「登記單位的 → 描述的 → 替換帶的」順序呼叫準備；失敗時這個 World 不用。守衛：O-8 各案。
- **O-8 替換**：替換用同一份 Registry、先建新的再丟舊的；準備失敗時舊的照跑；遊戲初始化不重跑；只在兩次 Run 之間。守衛：O-9 各案與讀檔案例。
- **O-9 動態插入的警告**：Kernel 拒絕（`DynamicInsertRefused`）或建 Plan 失敗時，編排層記**一次**警告、把錯誤回給呼叫的人、World 照跑；警告只從 `Product` 的診斷處理器出去。守衛：O-10 各案。

**Replacement Criteria**（L17 的形式）：
- RC-O1（原 Kernel RC 7）：第一個真實能力接進機制時仍要改編排層的迴圈或 Kernel 本體；或編排層以外的協調邏輯在兩個以上產品中重複。
- RC-O2：每加一種產品或 World 的組法都要改編排層的程式。
- RC-O3：runner 需要排 System 的先後才做得出某個 Feature。
- RC-O4：編排層開始 include 某個第三方或某個 Module 的內部。
- RC-O5：編排層的職責清單（O-4）要多加一項，或判不出某樣東西屬於哪一層——不數次數，出現就停下來問。
- RC-O6：登記檔的欄位不夠用，需要第二份描述同一件事的檔（例：依賴的種類另寫一份），或 CMake 與編排層讀出的依賴不一致。
- RC-O7：替換 World 的某個用途需要「不重建 World 就換掉內容」（讀檔宣告 RC 第 1 條的同一件事）。

---

## 10. 判定（先宣告，結果出來之後不放寬）

這不是計測的批（沒有閾值、試行次數）；判定都是一次決定的。

### 判定 1：通過與掃描

| # | 判定 | 怎麼量 |
|---|---|---|
| G-1 | 本機 `ctest --preset test -L "cpu|shader"` 全過（macOS，Debug）；新的 `orchestration.*` 與 `cmake.unit_registry` 測試在內 | ctest 的輸出 |
| G-2 | 第 7 節的每一案照表處置；新案照 7.3 的 Red 做法，並各有專屬突變 | 提交順序與輸出；Red 的輸出存證據資料夾 |
| G-3 | 掃描 0 命中（`git grep -n -P`，終了碼區分 0 命中與失敗，每個式子先在 0f1f2d21 用已知例命中一次，§14.2）：`engine/kernel` 對 `\bclass\s+Plugin\b`、`\bFinishContext\b`、`\bInstallRunner\b`、`\bRequires\s*\(`、`std::vector<Plugin\s*\*>`、`Registry::Finish\b`、`\.startup\b`、`\bstartup(Ran|Running)_\b`、`\bRunStartup\b`、`\bStartupNotNestable\b`、`\bStartupRunning\b`、`\bDE21\b`、`\bRE[136]\b`、`"H-1[23]"`；`engine/{base,math,entity_store,kernel,os,time,threads,io,audio}/CMakeLists.txt` 對 `GYO::(Base|Math|EntityStore|Kernel|Orchestration|Os|Time|Threads|Io|Audio)\b`；`engine/orchestration` 對 `#include\s+"engine/(?!kernel/|base/|orchestration/)`、`\bPlanOf\b`、`\bStepKind\b` | 掃描腳本（Python 或 `git grep`，不丟 stderr） |
| G-4 | 突變：第 8 節「要跑」的全部照 D184 跑，每一個都在預期的失敗下 killed | `tests/common/mutation/run_mutations.py` 的輸出 |
| G-5 | 守衛腳本的 Python 測試全過 | unittest 的輸出 |
| G-6 | CI 的 L1 三平台通過（PR 上；Windows、Linux 第一次跑編排層與登記檔的 CMake） | GitHub Actions |
| G-7 | 第 7.1 節有緩的面的三組與第 7.2 節的 DI 守衛各列，由檢證者用實際的測試與程式確認 1～5 的記述（§14.4） | 檢證報告 |
| G-8 | 編排層的新守衛測試（O-1～O-9）與它們的突變清單，由和作者不同的模型做一次敵對式檢查（§14.2）；報告寫檢查者的模型、找到的處數、加進去之後的結果（killed／總數） | 檢查報告 |

### 判定 2：讀檔的案例（讀檔宣告修訂 10、11 要求；`tests/common/orchestration/RestoreFlowTests.cpp`）

測試寫一個合成的「單機遊戲」：一個 Module 登記 `Player`、`Monster`、`ChaseTarget`、`Gold`（Resource）、配發器（範圍型）、一張 `Load`（作者的名字，一般 schedule）與一張每幀的 `Play`；存檔是測試裡的結構（玩家 12、怪物 57、58，57 的 `ChaseTarget` 是 12，金幣 120，下一號 900）。

1. **啟動時讀檔**：World 描述的準備把配發器範圍訂成 `[900, 上限]`、放暫存的存檔資料與金幣的預設值 0；runner 先跑 `Load`（`SpawnWithId` 帶完整 bundle 放回、`ResMut` 把金幣改成 120），再 `RunFrame`（`Play` 生成一個選單實體）。之後：編號 12、57、58 在、值與存檔相同、`ChaseTarget` 仍是 12、選單實體的編號 ≥ 900、金幣 120、沒有被丟的生成。
2. **沒有準備就整批被丟**（對照組）：同上但準備不移配發器的下界（範圍 `[1, 上限]`）：每一筆以 `IdInAllotterRange` 被丟並記紀錄——證明是準備時機讓案例 1 成立。
3. **遊戲中讀檔**：先跑三幀（`Play` 生成實體、金幣改成 50、製造一筆被丟的生成）；`ReplaceWorld("Main", 讀存檔)`；runner 跑新 World 的 `Load`。之後：只有讀檔放回的與之後生成的實體；金幣 120；被丟的紀錄從空的開始；遊戲初始化的計數仍是 1；每幀清單照舊（下一幀 `Play` 跑在新 World）。
4. **讀檔失敗不換**：`extra` 回 Err（存檔壞了）：`ReplaceWorld` 回 O-E7、警告一次；舊的 World 的實體與金幣 50 照舊、再跑一幀沒有錯誤。
5. **讀兩次相同**：同一份存檔換兩次 World、各跑 `Load`：兩次的 `Summary()` 相同。
6. **遊戲初始化放進第一個 World 的東西**（**照第 13 節第 1 件的結果寫**；建議的做法下）：遊戲初始化在第一個 World 放 `KeyBindings`（宣告換 World 時帶過去）與 `Score`（不帶）；`ReplaceWorld("Main", …)` 之後 `KeyBindings` 的值照舊、`Score` 回到準備給的預設值、`KeyBindings` 帶過去發生在描述的準備之後、`extra` 之前。使用者選別的做法時，本案照那個做法改寫（第一段核准時一起定）。

案例 5 守的是「同一份 Registry、新 World」的後果，靠 World 的建構本身，沒有對應的突變（寫明，不算進檢出力）。判定 2 的突變：準備移到第一次 Run 之後（案例 1）、準備不呼叫（案例 1、2 對調）、替換時保留舊實體（案例 3）、替換時沿用舊的被丟紀錄（案例 3）、替換時重跑遊戲初始化（案例 3）、`extra` 失敗時換上新的（案例 4）、替換時重建 Registry（案例 3 的 `Version` 斷言）、帶過去漏掉（案例 6）、帶過去在 `extra` 之後（案例 6）。

---

## 11. 誰做、用什麼、怎麼分段

**上段**（navigator：新 Module 的公開介面、Kernel 公開介面的減少、Design Invariants、建置圖的構造）。

| 角色 | 模型／檔位 | 理由 |
|---|---|---|
| 事前宣告的作者 | Opus／high（本卡） | — |
| 事前宣告的敵對式檢查 | Fable／high | 和作者換模型（§4）；交使用者之前一次 |
| 實作者（三段） | Opus／high | 統合第 8 節本批那一列 |
| 審查者 | Fable／high | 統合第 8 節；和實作者換模型 |
| §14.4 的檢證者（G-7） | Fable／high | navigator「檢證者確認 4 時不用小模型」；和實作者換模型 |
| 突變清單與新守衛的敵對式檢查（G-8） | Fable／high | §14.2；和作者換模型 |

都在統合第 8 節已核准的「Opus／high；Fable／high」之內，不另外請你同意檔位。

**分段**（先後卡片照 L17 判：新家先建、再拿掉舊的，測試才有地方搬；登記檔最後，因為編排層的依賴檢查只吃 `RegisteredUnits` 這份資料、不依賴檔案）。每段結束時 ctest 通過、提交、回報；每段結束時做那一段新守衛的 G-8：
- **段 1 編排層本體**（O1～O4、O6～O10）：新 Module、登記單位型別、`Product`、彙整、清單、runner、準備、替換、動態插入；新案與讀檔案例。7～9 h。
- **段 2 Kernel 減法**（O11、O12）：拿掉開局與登記單位、測試輔助改兩步、J-58 與 J-19 的案搬完、指紋升版與 golden、DI 文字。4～6 h。
- **段 3 登記檔**（O5）：兩份 CSV、CMake 讀取與驗證、引擎各 Module 的 link、`gyo_product_units` 與它的測試 target、`cmake -P` 測試。3～5 h。
- 文件（說明頁、`orchestration.zh-Hant.md`、dev_log）與突變的完整跑：2～3 h。

合計 **16～23 h**（統合第 8 節估 10～14 h；多出的是登記檔的 CMake 與產生、準備與替換、讀檔案例——統合估的時候讀檔卡的修訂 10、11 還沒到）。超過 1.5 倍（35 h）或單一問題累計超過 2 h 就停下回報。

**兩段核准**（照第 1 批）：這一次請你核准的是宣告（方向、設計、要動哪些判定）。實作完、合併前，帶 G-7 的檢證報告、G-8 的檢查與突變結果，再請你核准第 7.1 節各組、第 7.2 節的 DI 守衛各列與第 9 節的最終文字。第二段被你退回的組，合併前把那一組的改動撤回。

---

## 12. 範圍的限界

- 宣告寫在讀程式之後、實作之前；時間是估的，沒有量測。
- 第 7 節的案例清單由本卡以關鍵字掃描與讀測試寫成，沒有像第 1 批那樣由偵察者逐案讀全部 15 個測試檔；實作開始時由實作者照組再核一次（第 7 節開頭）。
- 別家的系譜：Bevy 讀本地原始碼（6a911653f）的指定行；Unity Entities 讀本地 1.5.0 的指定行；flecs 讀本地 checkout 的 `pipeline.c`；O3DE、Unreal 的清單機制與 Unreal 的 GameInstance、seamless travel、Unity 的 `DontDestroyOnLoad`、Godot 的 autoload 只照公開文件的一般知識，**沒有讀原始碼**。
- 空窗：(1) System 端的請求通道（結束、替換 World、從指令插入 System）在每幀迴圈的 runner 那一批之前沒有，作者要用自己的 Resource 與自己的 runner 接（3.9、3.10）；(2) 遊戲初始化的 System 在第 4 條之前照樣算寫者，和每幀的 System 寫同一個 Exclusive 的欄會是 D-E2（以前開局有豁免）；(3) 生成世界之前，讀檔的 System 由 runner 或每幀清單去跑。
- `ReplaceWorld` 時兩個 World 同時存在：綁執行緒的 Resource 帶過去（如果第 13 節第 1 件照建議）時，新的 World 在呼叫的執行緒上綁定（`SetResource` 的規則），和舊的同一條執行緒，沒有跨執行緒的情形；第 5 條有多執行緒之後要重看。
- 產生的 `<target>_units.cpp` 在本批只有測試 target 用；第一個真實產品接上時，`gyo_product_units` 和產品登記檔、打包的互動（AGENTS §7 的刪除檢查）要在那一批核一次。

---

## 13. 要你定的

### 第 1 件：World 被換掉時，遊戲初始化放進去的東西怎麼處理（讀檔卡修訂 11 交給本批的題目）

**遊戲**：RPG 行程啟動時，遊戲初始化在主 World 放 `KeyBindings`（按鍵設定，從設定檔讀）與 `AudioVolume`。玩家打到第 3 關、在遊戲中讀檔：主 World 被換掉。遊戲初始化不重跑（它是行程一次，使用者 `game-init-list`；讀檔宣告第 15 節）。新 World 裡沒有 `KeyBindings`——玩家按不了任何鍵。

| 選項 | 做法 | 好處 | 代價 | 別家 |
|---|---|---|---|---|
| **帶過去（建議）** | 作者在登記時宣告某個 Resource「換 World 時帶過去」（`setup.CarryOver<KeyBindings>()`）；編排層替換時，在描述的準備之後、`extra` 之前，把舊 World 的值移到新 World。其餘一律是新的 | 遊戲初始化放的東西不會不見；明確列出哪些帶，讀檔不會把舊遊戲狀態帶進來；不用改 Kernel（用公開的 `ResourceMut`／`SetResource`，型別在宣告時就知道） | 編排層多一個小功能（O-4「World 的準備與替換」之內，第 9.2 節請你一起確認歸法）；作者要記得標 | Unity `DontDestroyOnLoad`（換場景時明確標的物件留下）、Unreal seamless travel 的「要帶過去的 Actor 清單」與跨地圖存在的 GameInstance、Godot autoload（換場景不受影響的單例）——都是「明確標出要長留的」 |
| 不管、作者自己重放 | Engine 不做任何事；作者在準備函式裡自己從某個地方把值放回 | Engine 最少 | 作者得把遊戲初始化讀到的值另外存在 ECS 外面（全域變數或重讀設定檔）——等於遊戲初始化放進 World 沒有意義；單一 World 的產品最容易踩到 | Bevy 不換 World，沒有這個問題 |
| 第一個 World 不准換 | 換第一個 World 是錯誤；要長留的放第一個 World，遊戲放第二個 World、只換第二個 | 規則簡單 | 單一 World 的遊戲也要開兩個 World 才能遊戲中讀檔；等於回到你 11-16 比過、沒選的「編排層另建一個 World 跑遊戲初始化」 | Unity 的常見寫法（長留的預設 World＋換掉的遊戲 World） |

建議「帶過去」。理由：遊戲初始化「跑在第一個 World」是你定的，換 World 又是讀檔的預設路（`new-world-default`）；兩者同時成立時，必須有一條「哪些東西跨過替換」的明確規則，三家成熟引擎都用「明確標出要長留的」。

### 第 2 件：動態插入要不要重跑 `Finish` 與 `Check`（確認：和統合 O-2 草案的一句不同）

統合 9.2 的 O-2 草案寫「執行中的插入（指令等）照同一條路再走一次」（build → 建 Plan → finish → 檢查）。本宣告改成**只走 Kernel 的兩步、不重跑 `Finish` 與 `Check`**（3.10）。

- **遊戲**：開發中用指令加一個除錯用的 System「顯示碰撞框」。照草案，加完要再跑一次所有 Module 的 `Finish` 與策略的 `Check`。
- **為什麼不照草案**：Kernel 的 `BuildPlans` 成功就換上新版本，換上之後才跑的 `Check` 若失敗，Kernel 撤不回（第 1 批宣告第 11 節）——結果是「報錯了、但 System 已經插進去」；需要檢查保證的策略本來就要求它的 World 不准動態插入（伺服器權威，權威份 4.5）；Bevy、flecs、Unity 執行中加 System 都不重跑 Plugin 的 finish 或檢查。
- **另一條路**：Kernel 加「建好但先不換上」的一步，讓 `Check` 能在換上之前否決。代價：Kernel 的登記介面多一步（DI-KS 15 第 (12) 項「登記」之內，但改的是第 1 批剛核准的兩步）；現在沒有點名的 Feature 需要它（作者自寫、又准動態插入、又要檢查的策略）。

建議照本宣告（不重跑）；你若要「另一條路」，本批停下、先改 Kernel 的兩步。

### 一起確認（不是新的題目）

- 第 9.2 節 O-4 的歸法：「World 的準備與替換」（與第 1 件建議的「帶過去」）寫在 O-4 原有的「`Product`（World 群與 runner）」那一項之內，不算職責清單多一項。你若認為是多一項，照 L17 那就是第 1 件的一部分。

**卡片照 L17 自己判、沒有交你的**（列出來讓你看得到，有意見請更正）：名字（`GYO::Engine::Orchestration`、`Module`／`Plugin`、`Product`、`Prepare`、O-E1～O-E7）照命名規範；登記檔用 CSV、放 `engine/config/`；引擎各 Module 的 link 改由登記檔產生（AGENTS §9 一處）；Plugin 預設不帶；每個 World 一份 Registry；替換用同一份 Registry、先建新的再丟舊的；`Configure`／`Finish`／`Check` 分開；System 端的請求通道留到每幀迴圈的 runner 那一批；分段的先後。

---

## 14. 換模型檢查的處理

（交使用者之前，Fable／high 的敵對式檢查做完後補在這裡。）
