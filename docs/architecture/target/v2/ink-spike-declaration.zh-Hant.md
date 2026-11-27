# 事前宣告：Ink spike（`v8-arch-gyo2-ink-spike`）

更新：2026-11-27（修訂 3：I3 的審查與修正，第 12.4、12.5 節；修訂 2：使用者「開始」、補第 12 節「實作時發現的事」；修訂 1：補第 11 節「使用者的決定」）。Owner：Ink spike 任務卡（GYO2 重建主會話開）。
**狀態：動工前核准（使用者「開始」，HANDOFF `v8-arch-gyo2-ink-spike-start-impl`）；I1、I2 已實作，I3 的審查與修正已做；第 12.3、12.5 節有要交使用者決定的事。**

**核心一句**（每一項都拿來對照）：**這一批驗的是「inkcpp 能不能照 gyo2 的根接進來」：外部組件的型別只在 adapter 裡，對外用遊戲的詞；錯誤用 `Result` 告訴外面發生了什麼，不替外面決定；狀態（亂數、存讀、thread）由外面控制。台本怎麼寫、指令代表什麼、存檔怎麼放，是作者的。**

**用詞**：標「**卡片判**」的是這張卡照命名規範、源碼與已定原則自己判的，不是使用者的話。每一項都寫前提：**〔Engine〕**＝只有 Engine（這裡是 Plugin）做得到；**〔作者〕**＝作者（遊戲或寫手工具）能處理。

## 0. 根據與讀的版本

- 工作樹 `gyo2-ink-spike-9fa94f`，基底 `origin/gyo2` 00738c36。
- 已定的決定：研究 #361（[interaction-dialogue-research.zh-Hant.md](interaction-dialogue-research.zh-Hant.md) 第 2.1、2.6、4.2、6 節；HANDOFF `v8-arch-gyo2-interaction-dialogue-decisions`）：Ink＋inkcpp、當 Plugin 接、介面用遊戲的詞、第一批只給一個遊戲用、台本只在權威端跑、line ID 的約定現在定（字串表等第二種語言）。
- 2026-11-27 主會話轉來：最終產品是有怪物的撤離射擊，NPC 之後會在多人的權威端跑，所以 adapter 不能假設單機（第 5.6 節）。
- inkcpp：`~/Code/GitHome/inkcpp`，HEAD `d38608dffe50de8b6c954419647a5185044fbeee`（`v0.1.12+UEPatch_3` 之後第 10 個 commit），MIT（`LICENSE.txt`）。下面 inkcpp 的行號從該 repo 的根算起。
- gyo2 的先例：`plugins/sdl`（Plugin 自己的目錄裡 FetchContent，SDL-2：沒被選就不下載）、`third_party/yyjson`（釘 commit、`SOURCE_SUBDIR gyo-no-upstream-cmake`，自己列 source）、`config/plugins.csv` 的 `library_only`、Kernel 的 `ResourceOptions::threadBound`（`engine/ecs/kernel/include/engine/kernel/Declarations.hpp:29-36`）、text 的 TX-7（[text-declaration.zh-Hant.md](text-declaration.zh-Hant.md):278）。
- 沒有下載任何東西。inklecate 的大小是用 GitHub release API 讀的資產資料（只讀 metadata，沒有抓檔）。

## 1. 範圍與不做

**做**（單機 spike，四項驗收＋locale）：

- `plugins/ink`：inkcpp runtime 的 adapter，對外是遊戲的詞（第 5 節）。
- 一段有選項、有外部函式（指令）、有 tag（說話者、line ID）、會用亂數的台本，以及它的第二版（驗遷移）。
- 測試：第 7 節的五項，全部是外部可驗的 CTest，在 L1 三平台跑。

**不做**（等有遊戲要用時再開卡）：

- Engine 的台本 Module、多後端抽象（Q5）。
- 字串表、多語系（Q4：只定 line ID 的約定）。
- 會回傳值的外部函式（query）、遊戲讀寫 ink 變數、變數 observer：spike 只做「指令」（沒有回傳值、照順序交出去）。理由見第 5.4 節。
- 正式的資產管線（`.bin` 的 asset loader、寫手的匯入工具）：spike 在測試裡把 `.json` 編成 `.bin`（第 4 節）。
- UI、輸入、網路複製、Client 的顯示。
- 給 inkcpp 打補丁（例：把 `snprintf` 換成 `std::to_chars`）。

## 2. 單機場景放哪裡

| 選項 | 代價 | 好處 | 判斷 |
|---|---|---|---|
| A. `games/object_connect` | 動一個產品：它的 `project.json`、打包、驗收、移植清單都要跟著看；這個遊戲的目的不是 NPC，台本會很突兀；以後要拿掉時是拆產品的一部分 | 有真的遊戲迴圈 | 不推薦 |
| B. 新的最小範例 `examples/ink_dialogue`（console 程式，`--choose 0,1` 照腳本選，ctest 跑它） | 多一個執行檔與它的 CMake（`gyo_add_example`＋`gyo_product_units(... PLUGINS ink)`），約多 2～3 h；印出來的文字要另外比對才算驗收 | 使用者可以自己在終端機玩這段台本 | 可選的補充 |
| C. 只做測試層級：`plugins/ink/tests/`，其中一個測試是「場景」：真的 `Product`＋一個 World（沒有視窗），作者的 System 每 frame 推進台本、把指令變成作者的 `Message` 與 `Event` | 最小；沒有可以「玩」的東西 | 四項驗收都在這裡、外部可驗；跟著 Plugin 的列一起來去（SDL-2 的做法），拿掉 Plugin 不留殘骸 | **推薦** |

**推薦 C**（卡片判）。驗收全部是測試，C 已經是「單機場景」：有 Product、World、System、Message、Event，只是沒有畫面。B 只多一個「能手玩」，沒有多驗任何一項；使用者想手玩時再加（第 10 節 U1）。

## 3. inkcpp 怎麼進 build

### 3.1 放 Plugin 自己的目錄，不放 `third_party/`

卡片判，和工作指示的字面不同，請使用者確認（U2）：

- `third_party/` 底下的東西每次 configure 都會加（`third_party/CMakeLists.txt`：Module 用的 yyjson、freetype…，Module 不管選不選都在）。
- Plugin 用的外部組件放在 Plugin 自己的 `CMakeLists.txt`：SDL 就是這樣（`plugins/sdl/CMakeLists.txt`），沒被選的 Plugin 不 configure，也就不下載（SDL-2）。
- inkcpp 只給 `ink` 這個 Plugin 用，所以照 SDL：FetchContent 寫在 `plugins/ink/CMakeLists.txt`。

### 3.2 vendor 還是 FetchContent

| | FetchContent（推薦） | vendor（複製進 repo） |
|---|---|---|
| repo 的慣例 | 一致：yyjson、fast_float、freetype、harfbuzz、doctest、SDL 都是 FetchContent，repo 裡沒有複製進來的第三方源碼 | 第一個例外 |
| 大小 | repo 不變 | runtime＋shared 約 0.6 MB；測試要的 compiler 另約 1 MB（含 nlohmann `json.hpp` 3.11.2） |
| 升版 | 改一行 commit hash | 手動複製、PR 有大量 diff |
| 網路 | 第一次 configure 要抓（CI 每台機器第一次；有 cache 時不抓） | 不用 |

**推薦 FetchContent**（卡片判），寫法照 yyjson：

- `GIT_REPOSITORY https://github.com/JBenda/inkcpp.git`、`GIT_TAG d38608dffe50de8b6c954419647a5185044fbeee`（釘 commit，不釘 tag，內容不會動）。
- `GIT_SUBMODULES ""`：不抓 `inkcpp_python/pybind11`、`proofing/ink-proof` 兩個 submodule。
- `SOURCE_SUBDIR gyo-no-upstream-cmake`：**不用 upstream 的 CMake**。理由：upstream 的頂層會 `add_compile_options` 影響整個目錄、`add_subdirectory(unreal)` 與 `inkcpp_cl`，`inkcpp/CMakeLists.txt:76-91` 會把檔案複製到 `${CMAKE_BINARY_DIR}/unreal/...`。gyo2 自己列 source（`inkcpp/CMakeLists.txt:3-56` 的清單），建兩個 static library：
  - `gyo_inkcpp`：runtime＋`shared/`，保留例外（不定義 `INKCPP_NO_EXCEPTIONS`）、STL 版（預設的 `INK_ENABLE_STL`／`INK_ENABLE_CSTD`，`shared/public/config.h:9-22`）。只被 `GYO::Ink` PRIVATE 連結。
  - `gyo_inkcpp_compiler`：`inkcpp_compiler/` 的 source（`inkcpp_compiler/CMakeLists.txt:1-8`），**只在 `BUILD_TESTING` 時建、只給 Plugin 的測試連結**（第 4 節）。
- 本機開發用 `FETCHCONTENT_SOURCE_DIR_INKCPP=~/Code/GitHome/inkcpp`，不用下載（和已有的 clone 是同一個 commit）。

**下載量**（要使用者同意，U2）：CI 用 `GYO_PLUGINS=ALL`（`.github/workflows/build-and-validate.yml:175`），所以推上去之後 L1 三台都會 clone inkcpp。釘任意 commit 時 shallow clone 不一定能用，以完整 clone 估：本機 clone 的 pack 是 7.59 MiB（`git count-objects -vH`），GitHub 實際送的量未確認。

**未確認、實作時查**：inkcpp 是 C++17，在 gyo2 的 C++20 與 MSVC `/Zc:preprocessor`（`build/cmake/GyoBuild.cmake` 對之後加的目錄都生效）下能不能編過；`-ffp-contract=off` 也會套到它（對浮點結果只會更一致）。

### 3.3 登記

`config/plugins.csv` 加一列（卡片判）：`ink,ink,GYO::Ink,"Ink stories through inkcpp: ...",yes,no,,,,`。

- `library_only=yes`：spike 不放進 World（第 5.5 節）。
- 名字：Plugin 叫 `ink`（產品選的是「用 Ink 寫台本」），adapter 目錄叫 `src/adapters/inkcpp/`（照 SDL-1 的「`adapters/<組件>/`」，組件是 inkcpp）。namespace `GYO::Plugins::Ink`。

## 4. `.ink` → `.json` → `.bin`：工具端

**事實**（照源碼）：

- inkcpp 的編譯器**不能取代 inklecate**：它只讀 ink.json（`inkcpp_compiler/json_compiler.cpp:48-58` 讀 `inkVersion`），沒有 `.ink` 的 parser。inkcpp 自己的命令列工具碰到 `.ink` 也是去叫 inklecate（`inkcpp_cl/inkcpp_cl.cpp:137-146`）；Unreal 的編輯器匯入也是下載 inklecate 再編（`unreal/inkcpp/Source/inkcpp_editor/Private/InkAssetFactory.cpp:58-92`）。
- inklecate 只有 ink 官方 release 的三個 zip（v1.1.1，SHA256 照 inkcpp 的 `CMakeLists.txt:7-21`）：`inklecate_mac.zip` 29,173,014 bytes、`inklecate_linux.zip` 28,765,222 bytes、`inklecate_windows.zip` 26,402,544 bytes（GitHub release API 的 `size`）。
- inklecate 要不要另外裝 .NET：**未確認**。一個 zip 約 28 MB，像是把 .NET runtime 包進去的 self-contained 版，但沒下載、沒看內容。本機沒有 `dotnet`、`inklecate`、Inky。mac 版是 x86_64 還是 arm64 也未確認（這台 Mac 是 x86_64）。

**推薦的流程**（卡片判）：

```
寫手端（不在 CI）          repo 裡（commit）            測試裡
.ink ──inklecate 1.1.1──▶  .ink 與 .ink.json 都提交 ──inkcpp_compiler（in-memory）──▶ .bin bytes ──▶ GYO::Ink
```

- `.ink` 是寫手的原始檔；`.ink.json` 是 ink 官方的中間格式，當 Tool 與 Game 之間的資料（AGENTS §1-7）。兩個都提交，CI 不需要 inklecate、不需要 .NET。
- `.json` → `.bin` 在測試裡用 `inkcpp_compiler` 的 `ink::compiler::run(std::istream&, std::ostream&)`（`inkcpp_compiler/include/compiler.h:39`）當場編。`GYO::Ink` 本身不連編譯器（研究 4.2：編譯器只在工具端）。
- 重新產 `.json` 的腳本：`plugins/ink/tests/data/regenerate.py`（Python，跨平台；inklecate 的路徑從環境變數 `INKLECATE` 給），本機手動跑。
- 正式的管線（`.bin` 由工具烘焙、asset loader 讀）等遊戲要用時再開卡。

**下載**（要使用者同意，U3）：寫 spike 的台本需要在本機跑一次 inklecate。請求下載 `inklecate_mac.zip`（29,173,014 bytes，來源 `https://github.com/inkle/ink/releases/download/v1.1.1/inklecate_mac.zip`，SHA256 `c516402bca5fa249a7712e62591b048b137eba3098c53f9fb85a4253f9b9e2c0`），放主 checkout 的 `build/tools/inklecate-1.1.1/`（git 不追蹤）。下載後先查要不要 .NET；如果要，**先停下回報**，不自行下載 .NET。

**風險**：`.ink` 與 `.ink.json` 是否一致，CI 檢查不到（CI 沒有 inklecate）。spike 接受這一點：`regenerate.py` 是唯一的產生方式，PR 裡兩個檔一起改。

## 5. adapter 的介面

### 5.1 參考

- inkcpp 自己的 Unreal plugin（同一個 repo，成熟的整合）：`UInkThread` 把台詞、選項、tag 當事件交出去（`unreal/inkcpp/Source/inkcpp/Public/InkThread.h:97-132`），外部函式分成有回傳值的 `RegisterExternalFunction` 和沒有回傳值的 `RegisterExternalEvent`（`InkThread.h:160-183`），都預設 `lookaheadSafe = false`。
- Bevy 生態的 `bevy_yarnspinner`（研究 2.2，只讀網頁、沒有源碼）：runner 是 Component，台詞、選項、指令都變成 Event。這是 ECS 的接法，spike 先不放進 World（5.5）。

### 5.2 對外的型別（遊戲的詞；沒有任何 inkcpp 型別）

```cpp
namespace GYO::Plugins::Ink {

using Value = std::variant<bool, std::int32_t, float, std::string>;

struct Line {               // 一句台詞
    std::string text;       // 已去掉行尾換行
    std::string lineId;     // tag「line:<id>」；沒寫時空字串（第 6.2 節）
    std::string speaker;    // tag「speaker:<id>」；沒寫時空字串
    std::vector<std::string> tags;  // 其餘的 tag，照台本順序
};
struct Choice {             // 一個選項
    std::string text;
    std::string lineId;
    std::vector<std::string> tags;
};
struct Command {            // 台本呼叫的外部函式（沒有回傳值）
    std::string name;
    std::vector<Value> args;  // 照台本寫的順序
};
using Output = std::variant<Line, Command>;

struct Beat {               // 一次 Advance 的結果
    std::vector<Output> outputs;   // 台詞與指令，照台本執行的順序
    std::vector<Choice> choices;   // 空＝沒有要選的
    bool ended = false;            // 台本走到 END／DONE
};

struct DialogueOptions {
    std::uint32_t seed = 0;                 // 亂數種子，外面給；預設是固定值，不是時間
    std::vector<std::string> commands;      // 台本裡 EXTERNAL 的名字
    std::string lineIdTag = "line";         // tag 的前綴，有預設、可以改
    std::string speakerTag = "speaker";
};

class Story;      // 編好的台本（.bin bytes 讀進來），不可變
class Dialogue;   // 一段進行中的對話：move-only

Result<Story, DialogueError> LoadStory(std::span<const std::byte> compiled);

class Dialogue {
public:
    static Result<Dialogue, DialogueError> Start(const Story&, DialogueOptions);
    Result<Beat, DialogueError> Advance();             // 跑到出現選項或結束
    Result<void, DialogueError> Choose(std::size_t);   // 選第幾個
    Result<std::vector<std::byte>, DialogueError> Save() const;
    static Result<Restored, DialogueError> Restore(const Story&, std::span<const std::byte>, DialogueOptions);
};
struct Restored { Dialogue dialogue; bool migrated; };  // migrated：台本換過，退回最近的 knot

}
```

上面是形狀，名字與細節在實作時照命名規範定（卡片判）。

### 5.3 台詞、選項、選第幾個

- `Advance`：一直 `getline` 到 `can_continue()` 為假（`inkcpp/include/runner.h:104`、`:132`），每句收 tag（`runner.h:201-222`）；停下時收選項（`runner.h:172-192`，選項的 tag 在 `inkcpp/include/choice.h:53-59`）。等選擇時再呼叫，回傳空的 `outputs` 與同樣的 `choices`（沒有錯誤）。
- `Choose(i)`：`i` 超出範圍回 `ChoiceOutOfRange`，不交給 inkcpp。〔Engine〕之後在多人時，`i` 來自 Client 的輸入，是不可信的資料，所以是 `Result`，不是 Assert。
- tag：`line:` 與 `speaker:` 兩個前綴由 adapter 拆出來（前綴可以在 options 改，§1-10），其餘照原樣放 `tags`。

### 5.4 外部函式 → 作者的 Message／Event

- 綁定：`DialogueOptions::commands` 裡每個名字用 inkcpp 的陣列呼叫形式綁定 `(size_t n, const ink::runtime::value* v)`（`inkcpp/include/functional.h:206-241`），任意參數個數；`lookaheadSafe = false`（inkcpp 預設，`runner.h:333-337`），避免 lookahead 時重複執行。
- 被呼叫時，adapter 把名字與參數**複製**成 `Command` 放進這次 `Beat::outputs`（字串參數指向 inkcpp 的字串表，要當場複製）。參數的順序實作時用測試釘住。
- 變成作者的 `Message`／`Event` 是作者的事〔作者〕：場景測試裡作者的 System 讀 `Beat`，一個指令經 `MessageWriter<M>::Send`、一個經 `Commands::Trigger<E>`（`engine/ecs/kernel/include/engine/kernel/Params.hpp:523`、`:589`）。Plugin 不知道作者的型別，也就不用替每種 Message 做登記機制。
- **只做沒有回傳值的指令**（卡片判）：有回傳值的外部函式要在 `Advance` 的當下同步回答，等於讓台本在 System 中間讀遊戲狀態，牽涉到 System 的存取宣告；遊戲旗標也還沒設計（研究 4.3）。等有遊戲需求時，再在「query 回呼」和「遊戲寫 ink 變數」之間選。
- 台本呼叫了沒列在 `commands` 的 EXTERNAL、又沒有 fallback：inkcpp 丟例外 → `ScriptFault`（第 6 節）。

### 5.5 不放進 World（spike）

卡片判：`ink` 是 `library_only`。`Story` 與 `Dialogue` 是一般的值，作者把它們放進自己的 Resource 或 Component，由自己的 System 推進。理由：只有一個遊戲要用（Q5），World 端要長什麼樣（一個 Component 一段對話？指令變 Event 的登記？）要看遊戲的需求，現在做是在沒有差異的地方抽象。遊戲要用時再決定要不要加 World 的單位（照 `bevy_yarnspinner` 的形狀比較）。

### 5.6 不假設單機

台本之後在多人的權威端跑，所以：

- adapter 裡沒有「本地玩家」、UI、輸入、視窗、牆鐘。
- `Beat`、`Choice` 是普通資料，沒有 inkcpp 型別，可以複製給 Client（複製的元件或 `ServerMessage<M>`，研究 2.6 第 4 點）。
- 選擇的編號當不可信輸入檢查（5.3）。
- 種子由外面給（7.1），存讀是位元組，放進作者的存檔（`save_version` 只比版本號）。
- thread 的條件寫成契約（7.4），不綁在「主執行緒」上，Match 的模擬執行緒也能用。

## 6. 錯誤（`Result`）

### 6.1 種類（只說發生了什麼）

| 種類 | 什麼時候 | 之後的狀態 |
|---|---|---|
| `NotAStory` | `LoadStory` 的位元組不是 inkcpp 的 `.bin`：magic、endian、版本不符（`inkcpp/header.cpp:15-30`） | 沒有 `Story` |
| `ScriptFault` | inkcpp 在執行台本時丟 `ink_exception`（例：EXTERNAL 沒綁、參數個數不符、壞掉的指令） | inkcpp 把 runner 整個 reset（`inkcpp/runner_impl.cpp:1736-1741`），這段對話停了；之後 `Advance`／`Choose` 回 `Stopped` |
| `Stopped` | 對話因為 `ScriptFault` 停了之後又被呼叫 | 不變 |
| `ChoiceOutOfRange` | `Choose(i)` 的 `i` ≥ 選項數（帶 `i` 與選項數） | 不變，可以再選 |
| `SnapshotUnreadable` | `Restore` 的位元組讀不出來 | 沒有 `Dialogue` |
| `SnapshotNotMigratable` | 台本換過、快照當下在 tunnel／thread 裡（`inkcpp/include/snapshot.h:38`、`inkcpp/story_impl.cpp:206-208` 回空的 globals） | 沒有 `Dialogue` |

每個錯誤帶 inkcpp 的訊息（複製成 `std::string`）。怎麼處理（重開、讀舊檔、記日誌、退出）是作者的。

### 6.2 卡片判的細節

- **例外的接法**：adapter 的每個進入點 `try { … } catch (const ink::ink_exception&)`，翻成上表。`std::bad_alloc` 不接（照 gyo2 其他地方，記憶體用盡交 Product 的處理，`engine/orchestration/src/Product.cpp:619`）。inkcpp 的 `inkFail` 在 CSTD 版最後會 `exit`（`shared/public/system.h:286-291`），但有例外時 `throw` 在前面，走不到 `exit`。
- **沒有 line ID 的台詞**不是 runtime 錯誤：`lineId` 是空字串。理由：「每行都有 ID」是寫作的約定，應該在工具端檢查（spike 的測試檢查 fixture 的每一行都有 ID）；runtime 報錯會讓少一個 tag 的台詞中斷對話，是替作者決定處理。
- **inkcpp 的小漏**：`ink_assert` 帶格式參數時 `malloc` 的訊息沒有釋放（`shared/public/system.h:258-261`），每次 `ScriptFault` 漏一小段。spike 不打補丁，記在風險。

## 7. 驗收（都要有外部可驗的測試，L1 三平台）

### 7.1 固定種子

- 〔Engine〕inkcpp 在 runner 建立時用 `time(NULL)` 當種子（`inkcpp/runner_impl.cpp:530`）。`Dialogue::Start` 建 runner 後立刻 `set_rng_seed(options.seed)`（`runner.h:54`）。
- 測試：台本用 shuffle（`{~a|b|c}`）與 `RANDOM`。(a) 同一個種子跑兩次，輸出完全一樣；(b) 輸出等於寫在測試裡的固定字串（LCG 是整數運算，`inkcpp/random.h:10-34`，所以三平台一樣，L1 會驗）；(c) 兩個不同的種子給出不同的輸出（也是固定字串，不靠機率）。
- `Restore` 用快照裡的亂數狀態（`runner_impl.cpp:742` 寫、`:786-788` 讀），不用 `options.seed`：測試存在中間、讀回後接著跑的輸出，和沒存讀時一樣。

### 7.2 例外翻成 `Result`

- 測試：壞掉的位元組 → `NotAStory`；台本呼叫沒綁的 EXTERNAL（沒有 fallback）→ `ScriptFault`，之後 `Advance` → `Stopped`；`Choose` 超出範圍 → `ChoiceOutOfRange`，之後選對的可以繼續；壞掉的快照 → `SnapshotUnreadable`。每個測試也確認 process 還活著（沒有 `abort`／`exit`）。
- 觸發 `ScriptFault` 的確切台本在實作時照 inkcpp 的測試（`inkcpp_test/` 的 fallback 與外部函式測試）挑一個確定會丟的。

### 7.3 snapshot 存讀與遷移

- 同一個台本：選了第一個選項後存，讀回後 `Advance` 的輸出與沒存讀時一樣；一次性選項（`*`）讀回後不會再出現（拜訪次數在快照裡）。
- 換台本（fixture 的第二版：加一個全域變數、改一個 knot 的內容，hub knot 的名字不變）：在 hub knot 存，讀進第二版 → `migrated == true`，舊的全域變數值保留、新的是預設值，從 hub knot 接著跑（照 `snapshot.h:24-50` 的條件）。
- 不能遷移：在 tunnel 裡存，讀進第二版 → `SnapshotNotMigratable`；同一份快照讀回第一版仍然可以。
- 快照的位元組格式是 inkcpp 內部的。升 inkcpp 時舊存檔還能不能讀：**未確認**，實作時看 `snapshot_impl` 有沒有自己的版本號，寫進 Plugin 的頁。

### 7.4 thread

**更正前提**（卡片判，請使用者確認，U4）：工作指示寫「同一個 story 的物件要留在同一個 thread」。照源碼，inkcpp 沒有 `thread_local`、沒有可寫的全域狀態（研究 2.1），危險只在參照計數是一般的 `size_t`（`inkcpp/include/story_ptr.h:25`、`inkcpp/story_ptr.cpp:52-63`）——也就是**同時**被兩條 thread 碰。所以精確的條件是：

- **同一個 `Story` 和從它產生的所有 `Dialogue`，同一時間只給一條 thread 用**（參照計數掛在 story 的區塊上，兩段對話共用一個 story 時也會互相碰）。輪流用沒問題，只要中間有同步（Kernel 的 executor 一次跑一個 System，有 happens-before）。
- 各自 `LoadStory` 的兩個 `Story`（即使位元組相同）可以同時在兩條 thread。

和 text 的 TX-7 同一種條件（「不在乎哪條 thread，只在乎不同時用」）。放進 World 時：

| 選項 | 內容 | 判斷 |
|---|---|---|
| 一般 Resource，只用寫入存取碰它 | Kernel 的衝突圖保證不會同時；Match 的模擬執行緒也能跑 | **推薦**（text 的先例） |
| `threadBound` Resource（DI-KS 14，Bevy 的 NonSend） | 綁死在放進去的那條 thread，別條碰到是 `GYO_ASSERT` | 比需要的嚴；World 在準備執行緒建、在模擬執行緒跑時會斷言 |

〔Engine〕`Dialogue` 是 move-only、`Story` 不交出 inkcpp 的 `story_ptr`，這是型別的設計，不是守衛（AGENTS §3）。

測試：(a) 兩個各自讀進來的 `Story`，在兩條 `std::thread` 上同時跑同一段固定種子的對話，輸出和單一 thread 跑的一樣（驗「沒有全域狀態」）；(b) 一個 `Story` 的兩段 `Dialogue`，在兩條 thread 上**輪流**跑（中間用 `std::mutex`／`join` 同步），輸出和單一 thread 一樣（驗「輪流可以」）。「同時碰同一個 Story 會壞」不寫測試：那是未定義行為，測不出可靠的失敗，由契約與審查負責。

### 7.5 `"C"` locale

- 〔作者〕inkcpp 浮點轉字串用 `snprintf("%.7f")`（`inkcpp/string_utils.h:62`），跟著 `LC_NUMERIC`。C／C++ 程式啟動時就是 `"C"`，只有呼叫 `setlocale` 才會變。gyo2 的程式沒有呼叫 `setlocale`（`engine/modules/ui/src/UiRuntime.cpp:239` 用的是 `imbue(std::locale::classic())`，只改那個 stream）。
- 做法（卡片判）：寫成 Plugin 的契約「process 的 `LC_NUMERIC` 要是 `"C"`」；測試：台本印 `{1.5}`、`{0.25}`，輸出是 `1.5`、`0.25`。
- 沒選的做法：(a) adapter 每次 `Start` 檢查 `localeconv()->decimal_point` 回錯誤——這是擋作者的設定，不是告知 Plugin 自己的失敗；(b) 給 inkcpp 打補丁改用 `std::to_chars`——多一個要跟著升版重套的補丁，等真的有需要 locale 的產品再說。
- 「設成別的 locale 時會壞」不寫測試：CI 機器上有哪些 locale 不一定，測試會依賴環境。

## 8. 檔案的形（卡片判，照 `plugins/sdl_audio`）

```
plugins/ink/
├── CMakeLists.txt                 FetchContent inkcpp、gyo_inkcpp、GYO::Ink
├── include/plugins/ink/Dialogue.hpp   第 5.2 節的型別（沒有 inkcpp 的 include）
├── src/adapters/inkcpp/Dialogue.cpp   唯一 include inkcpp 的地方（照 SDL-1）
└── tests/
    ├── CMakeLists.txt             gyo_product_units(... PLUGINS ink)、連 gyo_inkcpp_compiler
    ├── CompileStory.cpp/.hpp      測試用：.ink.json → .bin（in-memory）
    ├── DialogueTests.cpp          7.1～7.5
    ├── SceneTests.cpp             第 2 節 C 的場景（Product＋World＋作者的 System）
    └── data/
        ├── merchant.ink / merchant.ink.json        第一版
        ├── merchant_v2.ink / merchant_v2.ink.json  第二版（遷移）
        ├── faults.ink / faults.ink.json            觸發 ScriptFault
        └── regenerate.py
```

Plugin 的 owner 頁 `docs/architecture/target/v2/ink.zh-Hant.md`（Design Invariants、契約、RC）在 I2 寫，最終文字在合併前核准。

## 9. 分批

```
v8-arch-gyo2-ink-spike
├── I1 接線與台詞    Opus／high｜上段：新 Plugin、外部組件、公開介面｜估 6～9 h
│     使用者同意 U2、U3 後：下載 inklecate、寫 fixture；FetchContent、plugins.csv 一列、
│     LoadStory／Start／Advance／Choose、tag、錯誤；測試 7.2、7.5、台詞與選項
├── I2 狀態與場景    Opus／high｜上段：公開介面、存檔資料｜估 6～9 h
│     種子、Save／Restore／遷移、指令、場景測試（Message／Event）、thread；
│     測試 7.1、7.3、7.4；owner 頁 ink.zh-Hant.md
└── I3 審查與收口    審查 Opus／high、純淨 Code Review Fable／high、修正 Opus／high、
                     dev_log Opus／medium｜上段｜估 3～5 h
                     設計審查（批次表〈每批的設計審查〉）＋純淨 Code Review；合併前核准頁
```

| 批 | 實作者 | 審查者 | 檢證者 | 上段／下段與根據 |
|---|---|---|---|---|
| I1 | Opus／high | Opus／high | Opus／high | 上段：新的 Plugin 與它的公開介面、新的外部組件（Architecture 的判斷） |
| I2 | Opus／high | Opus／high | Opus／high | 上段：公開介面、存檔的位元組（資料形式） |
| I3 | Opus／high（修正） | Opus／high（設計審查）；Fable／high（純淨 Code Review） | Opus／high | 上段：審查整條線 |

- 下載 inklecate、跑 `regenerate.py` 是 low 的定型作業，但在上段的批裡，照領航員規則仍用 Opus（不另外分小模型）。
- 突變（卡片判，候選，實作時定）：種子拿掉（回到 `time`）、`catch` 拿掉、`ChoiceOutOfRange` 的範圍檢查拿掉、`migrated` 反過來、`lookaheadSafe` 改成 `true`、指令參數不複製（改存指標）。每批只跑相關的（`mutation-rerun-scope`）。
- PR：這一張 draft PR 先放宣告；核准後實作接在同一條分支、同一個 PR（一條功能線一個 PR）。

## 10. 交使用者的事

| # | 問題 | 選項 | 推薦 |
|---|---|---|---|
| U1 | 單機場景放哪裡（第 2 節） | A `object_connect`／B 新的 console 範例／C 測試層級的場景 | C（想手玩時再加 B） |
| U2 | inkcpp 怎麼進 build（第 3 節） | FetchContent 寫在 `plugins/ink`（不放 `third_party/`）／vendor／FetchContent 放 `third_party/` | FetchContent 放 `plugins/ink`。**同意後 CI 每台機器會 clone inkcpp**（`github.com/JBenda/inkcpp`，commit d38608d，本機 pack 7.59 MiB，實際傳輸量未確認） |
| U3 | 本機下載 inklecate（第 4 節） | 同意／不同意（不同意時 spike 只能用 inkcpp 測試裡現成的 `.json`，自己的台本寫不了） | 同意：`inklecate_mac.zip`，29,173,014 bytes，`https://github.com/inkle/ink/releases/download/v1.1.1/inklecate_mac.zip`；要 .NET 的話先停下回報 |
| U4 | thread 的條件（第 7.4 節） | 照源碼改成「同一個 Story 一家不同時用」＋一般 Resource／照工作指示的「同一個 thread」＋`threadBound` | 前者（text TX-7 的先例；Match 的模擬執行緒能用） |

卡片判、不需要逐項看的（不同意時再說）：Plugin 叫 `ink`、`library_only`（5.5）；只做沒有回傳值的指令（5.4）；沒有 line ID 不算錯誤（6.2）；locale 寫成契約不檢查（7.5）；`Restore` 用快照的亂數狀態（7.1）。

## 11. 使用者的決定（2026-11-27）

出處：HANDOFF `v8-arch-gyo2-ink-spike-u1-u4`（gyo2 3825f51b），由主會話轉交。

- **原話**：「全部照推薦，空殼刪掉」。使用者先說「全部展示出來我過一遍」，主會話逐題列出選項與推薦後，使用者這樣回覆。
- **U1**：C，測試層級的場景（第 2 節）。
- **U2**：FetchContent 寫在 `plugins/ink`（第 3 節）。使用者同意 CI 每台機器 clone `github.com/JBenda/inkcpp` 的 d38608d。
- **U3**：同意在本機下載 `inklecate_mac.zip`（29,173,014 bytes，`https://github.com/inkle/ink/releases/download/v1.1.1/inklecate_mac.zip`）（第 4 節）。**同意只限這一個檔**；它需要 .NET 的話先停下回報；任何其他下載都要先問。
- **U4**：「同一個 Story 不能同時給兩處用」，當一般 Resource 處理，不用 `threadBound`（第 7.4 節）。
- **卡片判、使用者沒有推翻的**：Plugin 名 `ink`、`library_only`（5.5）；只做沒有回傳值的指令（5.4）；沒有 line ID 不算錯誤（6.2）；locale 寫成契約（7.5）；`Restore` 用快照的亂數狀態（7.1）。
- 開始實作（I1）等主會話另外通知；合併等使用者說「合併」。

### 11.1 U5：inkcpp 的存檔缺陷 D2（2026-11-27）

出處：HANDOFF `v8-arch-gyo2-ink-spike-u5`，由主會話轉交。

- **原話**：「照建議，不過讓主會話開一張任務卡來專門處理回報的問題。詳細溝通我會在任務卡裡進行。」
- 選的是主會話列出的 (2)＋(3)：
  - **(2) 回報上游**（12.3 的 (c)）：由主會話另開的回報卡處理，這張卡不發 issue。回報卡要最小重現的台本、或釘住現行為的測試（`plugins/ink/tests/DialogueTests.cpp` 裡名字以「inkcpp defect, not patched」開頭的三個測試，D2 是第 394 行那個；台本在 `plugins/ink/tests/data/`）時，這張卡配合提供。
  - **(3) 存檔時機**：照 inkcpp 作者在 #146 的建議，在「選完選項之後、要下一次 `Advance` 之前」存檔，**選項畫面不存**。**這是遊戲的規則，不是 Engine 的**：adapter 不擋、不改，在選項處存仍然照現狀回位元組，`Restore` 讀不回來時照第 6 節回 `SnapshotUnreadable`／`SnapshotNotMigratable`。
- 這條規則要不要在 adapter 的文件或介面上露出來（例如 `Save` 回報「現在存的檔讀不回來」），和 U7 (a) 有關，**等 U7 定了再做**。
- 不打補丁（12.3 的 (b) 沒有選）。

## 12. 實作時發現的事（I1、I2，2026-11-27）

細節與程式位置在 owner 頁 [ink.zh-Hant.md](ink.zh-Hant.md)。

### 12.1 照宣告做到的

- 7.1～7.5 都有外部可驗的測試（`ink.dialogue`，I2 結束時 23 案，I3 後 24 案）。
- 場景照 U1 的 C 做（`SceneTests.cpp`）：Product＋一個 World，作者的 System 把 `give_item` 變成 `Message`，把 `play_sound` 變成 `Event`。
- 突變 I2 結束時 13 個、13/13 killed（I3 後 15 個，見 12.4）（`docs/architecture/target/v2/scripts/ink_mutations.json`）。
- inklecate：
  - 下載了 `inklecate_mac.zip`，29,173,014 bytes，SHA256 與宣告一致。
  - 它是 self-contained 的 x86_64 程式，**不需要 .NET**。
  - 沒有其他下載：本機用已有的 inkcpp clone，其他依賴用別的工作樹已經抓好、版本相同的來源。

### 12.2 和宣告不同的地方（卡片判）

- **存檔多了 8 bytes 的開頭**：格式版本和台本的 hash。
  - 為什麼要 hash：inkcpp 的 snapshot 不對外露出 hash，`Restored::migrated` 只能自己記。
  - 先寫了 4 bytes 的 magic，突變顯示它多餘（格式版本與 inkcpp 的 header 已經擋掉同樣的位元組），所以拿掉了。
- **短位元組的緩衝**：inkcpp 讀台本或 snapshot 時，先讀固定長度的 header、之後才檢查長度。adapter 先複製進至少 256 bytes、其餘填 0 的緩衝區（owner 頁第 7 節）。
- **inkcpp 的例外**：STL 版的 `ink::ink_exception` 就是 `std::runtime_error`，不是第 6.2 節以為的「自己的類別」。接法不變。
- **空行**：inkcpp 在選項前多給一個沒有文字也沒有 tag 的行，adapter 不交出去。
- **選項的 tag 寫在 `[...]` 裡面**：寫在 `]` 之後，照 ink 的語法是選了之後那段輸出的 tag。fixture 照這樣寫。

### 12.3 要交使用者決定的事

| # | 事 | 選項 | 卡片的建議 |
|---|---|---|---|
| U5 | **inkcpp 的存檔缺陷 D2**：在選項處存的檔，只要台本裡有任何 sequence（`{a\|b}`、`{&…}`、`{!…}`、`{~…}`），連讀回同一個台本都失敗（`globals_impl.cpp:294-297`）。能存的點是「選完之後、下一次 `Advance` 之前」。D1（跳過的選項的 tag 給了下一個選項）、D3（第一次 `Advance` 前存的檔讀回已結束）比較輕 | (a) 現在不處理，把能存的點寫進契約（現狀）；(b) 給 inkcpp 打補丁（宣告第 1 節原本不做，要擴大範圍）；(c) 先回報 upstream，等遊戲真的要在選項處存檔時再決定（RC-INK1） | (c)：spike 沒有遊戲在用；三個缺陷各有釘住現狀的測試，inkcpp 修好時測試會失敗、提醒我們改 <br>**已決定（11.1）**：回報上游（另一張卡）＋存檔時機是遊戲的規則 |
| U6 | **產品選不了 `library_only` 的 Plugin**：`gyo_product_units(... PLUGINS ...)` 只收放進 World 的 Plugin，`ink` 現在只能經 `GYO_PLUGINS` 選。spike 的測試自己連 `GYO::Ink` | (a) 等第一個用 ink 的遊戲開卡時再定；(b) 現在就讓產品能選 `library_only` 的 Plugin（改建置層，擴大範圍）；(c) 給 `ink` 一個放進 World 的單位（推翻 `library_only`） | (a) |

### 12.4 I3：審查與修正

- 設計審查（Opus／high）和純淨 Code Review（Fable／high）都只讀 `origin/gyo2...11ef39c4`。重點與處置：主 checkout `build/evidence/v8-arch-gyo2-ink-spike/reviews-11ef39c4.md`。
- **兩份都指出的 blocker**：讀進新版台本時，inkcpp 在存檔所在的位置不見了時回空的 runner（`story_impl.cpp:262-269`），adapter 沒有檢查。依序處理：
  1. 用第四版台本 `merchant_v4.ink`（hub 留著，拿掉存檔所在的選項）重現：0 個指令時，`Restore` 回成功。
  2. 寫成失敗的測試。
  3. 修正：加空 runner 的檢查。
  4. 第三版 `merchant_v3.ink`（hub 改名）走的是 inkcpp 丟例外的路，原本回 `SnapshotUnreadable`。改成：台本換過時，讀不了一律 `SnapshotNotMigratable`。detail 只寫 inkcpp 給了什麼。
- 其他修正：
  - D1 只釘在一處：主台本的「買劍」改成 `+`，新的 `once.ink` 專門給 D1。
  - 補零改成長度檢查（台本 `sizeof(ink::internal::header)`，snapshot 5 個 `size_t`），加 16 bytes 的截斷台本測試。
  - hash 在 `LoadStory` 時算一次。
  - D2 的測試只比 code。
  - `ScriptFault` 的「之後的狀態」照進入點寫。
- 突變：15 個（加了 `ink-empty-runner-unchecked`、`ink-migration-failure-unreadable`），最後的原始碼上 15/15 killed。

### 12.5 審查帶出的設計問題（交使用者，合併前核准時一起看）

| # | 事 | 卡片的建議 |
|---|---|---|
| U7 | **介面的幾個取捨**（兩份審查共同或各自提出），都是公開介面的形狀：(a) `Save` 要不要告訴作者「這份存檔讀不讀得回來、能不能遷移」（inkcpp 的 Unreal 整合有 `Migratable`；現在作者要等到 `Restore` 才知道，D2、D3 讓這件事變重要）；(b) `seed` 只對 `Start` 有用，要不要拆出 `DialogueOptions`；(c) tag：現在固定拆兩個鍵（`line`、`speaker`），要不要改成「所有 `鍵:值` 的 tag 都拆成鍵值對」；(d) 不支援的參數（list）現在是 `ScriptFault`、整段對話停掉，要不要改成交出一個「不支援」的值 | (a) 現在加：只是多給資訊，不替作者決定存不存；(b)(c)(d) 等第一個遊戲用時再定（spike 沒有使用者，形狀還看不出來） |

另外寫進 owner 頁的偏離與觸發條件（不需要現在決定）：每個 `Dialogue` 有自己的 globals（RC-INK2）、`Advance` 一次跑到選項（RC-INK3）、資料契約綁在 inkcpp 的二進位格式（RC-INK5）、沒有選項的無窮迴圈會卡住 `Advance`（範囲の限界）。

