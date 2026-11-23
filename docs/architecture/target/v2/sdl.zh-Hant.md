# SDL 的 Plugin（`plugins/sdl*`）

更新：2026-11-23（新頁，子批 P `v8-arch-gyo2-sdl-p`）；2026-11-23（子批 A `v8-arch-gyo2-sdl-a`：`sdl_audio` 一節、SDL-3、Plugin 的命名、`GYO::Sdl` 改 `GYO::SDL`）。由來：[SDL 總宣告](sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准「SDL 照建議核准」，HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 9.1、10.2、12 節。本頁是 `plugins/sdl*` 的 owner 文件：子批 P 開頁、寫目錄的形、取得、選擇與 SDL-1、SDL-2；W（`sdl_window`）、A（`sdl_audio`）、R2a（`sdl_gpu`）各自補自己那一列與 SDL-3～SDL-7（草案在總宣告第 12 節）。

Owner：架構。

## 一個例子

Match（伺服器）不帶任何 `sdl*` 的列：configure 不加 `plugins/sdl`，SDL 不下載、不建，產品裡沒有 SDL。KeepTool 帶 `sdl_window`、`sdl_gpu`：建置層加這兩個目錄，也沿依賴加 `sdl`（函式庫），由 `sdl` 抓 SDL 3.4.0、上 GYO 的補丁。把整組 `sdl*` 從 repo 拿掉，Engine、所有 Module、headless 的測試照樣建得起、測得過（SDL-2）。

## 1. 列與依賴

一列＝一個目錄＝一個 target＝一個單位（`config/plugins.csv`）。依賴只有 `plugins/sdl*` → `engine/modules/*` 與 `sdl` 函式庫；Engine 不 include `plugins/`，Module 不依賴 Plugin（登記檔讀取時的 FATAL_ERROR）。

| 列 | 角色 | 誰建 | 現況 |
|---|---|---|---|
| `sdl` | 函式庫（`library_only`）：抓 SDL3、上補丁；`sdl*` 之間共用的、我們的詞寫的型別（視窗登錄表、時鐘配對） | P | 有（target `GYO::SDL`；P 只有取得，還沒有共用型別） |
| `sdl_window` | 來源側：Runner、Window、InputEvents、Ime、剪貼簿 | W | 還沒有 |
| `sdl_gpu` | 渲染側：Gpu、GpuSubmit | R2a | 還沒有 |
| `sdl_audio` | `Configure`：AudioOutput（第 5 節） | A | 有（target `GYO::SDLAudio`） |

各列的內容、依賴與兩個 World 的接法見總宣告第 9.1、9.2 節。一列的依賴在登記檔寫：例 `sdl_window` 的 `depends` 是 `sdl;input;window`。

## 2. 目錄的形

```text
plugins/<名>/
  CMakeLists.txt          # target gyo_<名>、alias GYO::<名>；SDL3::SDL3-static 以 PRIVATE 連結
  include/…               # 給依賴者的標頭：我們的詞，不 include SDL3/
  src/…                   # System、單位、runner：不 include SDL3/
  src/adapters/sdl/…      # 只有這裡的 .cpp（與這裡的私有標頭）include SDL3/
  tests/CMakeLists.txt    # BUILD_TESTING 時加入；這個 Plugin 沒被選就不存在
  tests/adapters/sdl/…    # 直接呼叫 SDL 的測試
```

- adapter 給 System 用的介面標頭放在 `src/adapters/sdl/` 外（例：`plugins/sdl_gpu/src/GpuDevice.hpp`），由 adapter 的 `.cpp` 實作（SDL-1）。
- **名字**（命名規範 N17 的「`GYO::Plugins::<plugin>` 由第一個 plugin 批定」，A 定）：SDL 是縮寫，照縮寫表寫 `SDL`（一大一小不准；`Json` 是唯一的例外）。目錄 `plugins/<名>` ↔ namespace `GYO::Plugins::<從目錄名推>`（同 Engine 的單位：以 `_` 分段、首字母大寫、縮寫段照表）↔ target `gyo_<名>`／`GYO::<推出的名>`：`sdl_audio` ↔ `GYO::Plugins::SDLAudio` ↔ `GYO::SDLAudio`；單位型別 `<推出的名>Plugin`（N20）；含入路徑 `plugins/<名>/…`。Plugin 的測試也在它的 namespace 裡（測試在 Plugin 的目錄，守衛照目錄判）。守衛：`naming.guard` 把 `plugins/` 底下每個目錄當一個單位查（`ACRONYMS` 加 `sdl`）。
- **SDL 的全域狀態**（`sdl*` 共用；A 定，設計審查 S6）：SDL 是行程全域的。(1) 每個 `sdl*` 只用 `SDL_InitSubSystem`／`SDL_QuitSubSystem` 管自己用的子系統（參照計數），**不呼叫 `SDL_Quit`**：`SDL_Quit` 會把所有子系統一起收掉（例：音訊的 `SDL_QuitAudio` 會 `SDL_DestroyAudioStream` 所有裝置 stream，`SDL_audio.c:1110-1116`），之後 `sdl_audio` 停裝置時就是對已釋放的 stream 操作。(2) 第一次 `SDL_InitSubSystem` 的執行緒被 SDL 記成主執行緒（`SDL.c:289-290`；AUDIO 隱含 EVENTS）：`Configure` 在呼叫 `Product::Build` 的執行緒上跑，所以那條執行緒就是 SDL 的主執行緒，有視窗的 runner 要在同一條執行緒（SDL-4）。(3) hint 是行程全域的：設了要在用完時還原，環境變數優先於程式設的值。
- Plugin 的測試放在自己的目錄、跟著列走；名字以 Plugin 名開頭（例：`sdl.build`）。`plugins/` 是執行時的程式：os 守衛的 R1～R4 都管它，它的測試也受 R4（os.md DI 1、2；`tests/` 底下的豁免不延伸到 `plugins/<名>/tests/`）；Windows 上它的執行檔套 UTF-8 的 code page（os.md DI 11）。

## 3. 選擇：只建被選的 Plugin

- 建置層（`build/cmake/GyoBuild.cmake`）在所有產品、測試、examples 之後，加入被選的 Plugin 目錄：產品在 `gyo_product_units(... PLUGINS …)` 列出的，加上快取變數 `GYO_PLUGINS` 點名的（登記檔的名字，或 `ALL`），連同它們依賴的 Plugin，照登記順序（`gyo_plugin_selection`，`GyoUnitRegistry.cmake`）。產品先 configure、以名字 link Plugin 的 target，CMake 在全部目錄 configure 完之後才解析；link 一個沒被選的 Plugin，generate 時 CMake 自己報 target 不存在。Plugin 自己的測試裡也可以用 `gyo_product_units(... PLUGINS <自己>)`。
- 預設 `GYO_PLUGINS` 空：只建產品帶的。`test` 系列的 preset（含 L1 的 `ci-<os>`）設 `ALL`：三個平台都建 SDL、跑 Plugin 自己的測試。`core` preset 設空。
- `gyo-build.json` 的 `plugins` 記這次加入的 Plugin。

## 4. SDL 的取得

| 項目 | 值 |
|---|---|
| 版本 | SDL 3.4.0（tag `release-3.4.0`，commit `a962f40bbba175e9716557a25d5d7965f134a3d3`，2026-01-01 發布） |
| 來源 | 官方 release 的原始碼 archive `https://github.com/libsdl-org/SDL/releases/download/release-3.4.0/SDL3-3.4.0.tar.gz`（15,567,602 bytes） |
| 驗證 | `URL_HASH SHA256=082cbf5f429e0d80820f68dc2b507a94d4cc1b4e70817b119bbb8ec6a69584b8`（GitHub 的 asset digest） |
| 放哪 | FetchContent，建置輸出的 `_deps/sdl3-*`；不放 `plugins/` 底下、不放 `third_party/`（`third_party/` 在登記檔之前無條件加入，放那裡刪不掉 SDL，總宣告第 9.1 節） |
| 建法 | 靜態（`SDL_STATIC=ON`、`SDL_SHARED=OFF`）；不建 SDL 自己的測試、examples、install |

**靜態的理由**：產品旁邊不必再帶一個 SDL3 的動態函式庫，Windows 的測試不必設 DLL 搜尋路徑。master 用動態（`GyoRuntimeDeployment.cmake` 的 `SDL3-shared` 複製）；要改回動態時，打包（子批 K 的 tools 線）一起處理。

**GYO 的編譯選項**：`plugins/` 在 `add_compile_options`（`-ffp-contract=off`、MSVC 的 `/Zc:preprocessor`，`GyoBuild.cmake`）之後加入，FetchContent 的 SDL 也吃到這些選項。L1 三平台（含 MSVC）建得起是 P 的驗收之一。

### SDL 的補丁

`plugins/sdl/patches/*.patch` 照檔名順序上（`plugins/sdl/cmake/ApplyPatches.cmake`；已上的跳過）。原始碼是 release archive 解開的、在建置輸出裡（可能在 GYO 的 git 工作樹之內），所以 `git apply` 用 `GIT_CEILING_DIRECTORIES` 停在原始碼目錄、當一般的 patch 程式用。用 `FETCHCONTENT_SOURCE_DIR_SDL3` 指定的原始碼不經 patch 步驟：configure 時檢查補丁的標記，沒上就是 FATAL_ERROR。

| 補丁 | 缺陷 | 移除條件 |
|---|---|---|
| `gyo-cocoa-ime-composing-keydown` | macOS：組字中，輸入法處理一個鍵卻沒改組字時，SDL 照樣送出按下（日文輸入法的第一個 Return 只確定候選字），程式中途收到 Return。補丁讓組字中按下的鍵不送給程式，除非輸入法把它當命令轉出（`doCommandBySelector:`），同 winit。由來：master `f775acb2`（`v8-tool-ui-editor-ime-enter`） | SDL 上游修好；或升版後補丁上不去（先在新版重現缺陷，再移植補丁） |

### 升版

改 `plugins/sdl/CMakeLists.txt` 的 URL 與 `URL_HASH`、本節的表、`plugins/sdl/tests/adapters/sdl/SdlBuildTests.cpp` 的版本；每個補丁重新上、上不去時照上表的移除條件。下載新版本要使用者同意（版本、來源、大小、SHA-256）。

## 5. `sdl_audio`：音訊輸出

〔子批 A `v8-arch-gyo2-sdl-a`；總宣告第 7 節、第 10 節的 A。〕

**一個例子**：Object_Connect 帶 `sdl_audio`。產品啟動時，`sdl_audio` 在自己的 `Configure` 建一個混音器放進 Product 的共用處、把音效 loader 登記進 asset、開預設的播放裝置。遊戲的表現層 System 經提交口送「播哪個 clip、什麼時刻」，SDL 的音訊執行緒每個 block 叫一次 `Mixer::Render`。在沒有音效卡的機器上，裝置開不了：產品照樣跑、照樣送聲音（計數、不出聲），遊戲讀共用處的 `AudioOutput` 知道為什麼。關掉產品時，裝置在所有 World 之後、共用處之前停。

**角色**：只做 `Configure` 的單位（總宣告第 3.3 節）：產品帶著、不放進任何 World（放進也沒有東西登記）。`depends`＝`sdl;audio`（都是函式庫）。

**`Configure` 做的事**（照這個順序）：

1. **混音器**：產品給了 `AudioOutputMixer`（`ProductDescription::options`，一個 `shared_ptr<Mixer>`）就播產品的、不建；否則照 `AudioOutputOptions::mixer` 建一個，`Create<Mixer>` 放進 Product 的共用處，其他單位與 World 的 build 用 `Find<Mixer>()` 找（總宣告第 7 節的 (i)，(ii) 當覆寫）。設定超出混音器的限界是啟動錯誤 `SA-E1`（產品寫錯設定，不是機器的狀況）。
2. **音效 loader**：共用處有 asset 的 `AssetLoaderRegistry`（產品帶 `world_asset`）時登記 `WavLoader`（audio 頁第 1 節「SDL 那一批有建混音器的登記單位時，改由它登記」，使用者 2026-11-23 核准）。同一個 loader 再登記不變，所以產品自己也登記無妨；被拒是啟動錯誤 `SA-E2`。沒有 asset 的產品不登記、不報錯。
3. **裝置**：開預設的播放裝置（`AudioOutputOptions::deviceSampleFrames` 是給 SDL 的 block 大小提示，0＝平台的預設），把 `AudioOutput` 放進共用處。**開不了不是啟動錯誤**：`AudioOutput::Failure()` 說為什麼（`AudioOutputFailure`，N25 的 E2：這個輸出沒做成，其他都健康；碼 `SubsystemUnavailable`、`DeviceUnavailable`、`DeviceNotStarted`，detail 是 SDL 的訊息），產品照跑、提交口照收（計數、不出聲）。「告訴外面發生了什麼、不替外面決定」：要不要提示玩家、要不要結束，是作者讀了決定。Bevy 同形：開不了裝置只警告、`AudioOutput.stream` 是 `None`，遊戲照跑（`bevy_audio/src/audio_output.rs:16-31`）。
4. **停**：`OnDestroy` 停裝置。O-15：回呼在所有 World 之後、共用處之前跑，所以裝置先停，`sdl_audio` 建的混音器（共用處）與產品給的混音器（選項，活到共用處之後）都比最後一次回呼活得久（audio DI 7）。

**adapter**（SDL-1、`-sdl-system-calls-via-adapter`）：`src/AudioDevice.hpp` 是我們的詞（開、停、統計），`src/adapters/sdl/AudioDevice.cpp` 是 `sdl_audio` 裡唯一 include SDL 的檔案。開：`SDL_InitSubSystem(AUDIO)` → `SDL_OpenAudioDeviceStream(預設播放裝置, S16／2ch／48000, 回呼)` → `SDL_ResumeAudioStreamDevice`（裝置的格式與預設裝置的切換由 SDL 的 stream 處理）。回呼在 SDL 的音訊執行緒：每個 block 開頭讀一次 `Time::Now()` 當 anchor（SDL-3、audio DI 3），一次最多算 4096 frame、分幾次時下一次的 anchor 加上已算的時間；不丟例外、不配置、不上鎖、不記日誌（audio DI 7；SDL 自己的 `SDL_PutAudioStreamData` 可能配置）。只持混音器的指標、不持所有權。停：`SDL_DestroyAudioStream`（在 SDL 的鎖下解除綁定，返回後不再有回呼）→ `SDL_QuitSubSystem(AUDIO)`（參照計數，多個產品各開各的不互相關掉）。照 master 修過的做法（`engine/audio/backend/sdl/src/SdlAudioOutput.cpp`，origin/master），差別：master 的輸出擁有混音器，這裡只借（audio 頁第 4 節）；拿掉 master 的 anchor 紀錄（只給 master 的起音紀錄用，沒有讀者）。

**和別家比**：

| | 誰擁有混音器 | 停的先後 | 開不了裝置 |
|---|---|---|---|
| GYO `sdl_audio` | 共用處（或產品），輸出只借 | `OnDestroy` 停裝置 → 共用處解構 | 回報在共用處，不是啟動錯誤 |
| Bevy `bevy_audio` | 開裝置的 `AudioOutput` Resource 擁有 rodio 的 sink（`audio_output.rs:16-31`） | Resource 解構時 | `warn!`、`stream: None`，照跑 |
| kira | `AudioManager::new` 把 renderer 交給後端（`crates/kira/src/manager.rs:70-92`），後端擁有它 | `CpalBackend` 的 `Drop` 停 stream（`backend/cpal/desktop.rs:158-166`） | `AudioManager::new` 回 `Err`，作者決定 |
| SoLoud | `Soloud` 物件自己是混音器，`init` 開後端（`include/soloud.h:230`） | `deinit`（`src/core/soloud.cpp:192`） | `init` 回錯誤碼 |
| miniaudio | `ma_device_init` 開裝置、作者的回呼混音（`miniaudio.h:137-149`） | `ma_device_uninit` | 回 `ma_result` |

**`scheduleDelay` 的重新推導**（總宣告第 7 節；判定在量之前寫）：

- **量什麼**：延遲至少要蓋過「事件到下一幀送出」（30 FPS 的消費者最多 33.3 ms）＋「送出到下一次回呼」（回呼的最長間隔）。量的是每個平台、預設播放裝置、SDL 預設 block 大小下的回呼最長間隔：`gyo_sdl_audio_block_record 10`（不是 CTest，要真的裝置），每台跑兩次，取大的。
- **判定**：需要的延遲＝33.3 ms＋最長間隔；預設值＝各平台需要的延遲的最大值加 5 ms 的餘裕，往上取到 5 ms 的倍數。比 60 ms 大就改大；小於 60 ms 時，三個平台都量到才改小（只量到一部分不改小：沒量到的平台可能更大）。
- **量到的**：（見下表；`v8-arch-gyo2-sdl-a` 本機）

| 平台 | 機器、驅動、預設輸出 | 指定 | device_frames | 最長間隔（兩次） | 需要的延遲 |
|---|---|---|---|---|---|
| macOS 26.7.1（Intel） | MacBook Pro、coreaudio、LG HDR 4K（DisplayPort） | 不指定（SDL 預設） | 1024 | 21.463／21.470 ms | 33.3＋21.5＝54.8 ms |
| 同上 | 同上 | 480 | 480 | 10.831 ms（一次） | 44.1 ms |
| 同上 | 同上 | 256 | 256 | 10.863 ms（一次） | 44.2 ms |
| Windows | **未量**（主戰場；這張卡的機器上沒有） | | | | |
| Linux | **未量**（CI 沒有音訊裝置，dummy／disk 驅動的節拍不是真的） | | | | |

- **結論**：量到的只有 macOS：54.8＋5＝59.8 → **60 ms，預設值不變**（判定的「比 60 大才改大」不成立；只量到一個平台，不改小）。master 的量測（macOS 54.8 ms、Windows 44.4 ms）只當比較基準：macOS 這次和 master 同一台、同一個數。Windows、Linux 要在實機上各跑兩次 `gyo_sdl_audio_block_record 10`；任一個的需要的延遲＋5 ms 超過 60 ms 就照判定改大。
- macOS 的 CoreAudio 節拍下限約 10.8 ms：指定 256 也只到 10.9 ms（同 master）。

**範囲の限界**：

- 只有預設的播放裝置；選裝置、裝置清單、錄音不做（沒有點名的 Feature）。預設裝置被拔掉時，SDL 的 stream 自己跟到新的預設裝置；跟不上時（沒有裝置了）不出聲，`AudioOutput` 不改狀態（不輪詢）。
- `AudioOutput` 不隨裝置中途的狀況改變；`Stats()` 只供顯示與量測。
- 回呼統計是 relaxed 原子、一個一個讀，期間的快照不一致；不拿來相減。
- `sound`／`wav` 由 `sdl_audio` 登記 `WavLoader`，在產品的 `configure` 之前；asset 對同一組（型別、格式）拒絕第二個 loader（`FormatTaken`），所以帶 `sdl_audio` 的產品不能換成自己的 `wav` loader（Bevy 允許重複登記、只警告，`bevy_asset/src/server/loaders.rs:65-67`）。沒有點名的 Feature 要換；要換時在 `AudioOutputOptions` 加一個不登記的選項。
- `OnDestroy` 停裝置和共用處的反序解構今天給出同一個先後（`AudioOutput` 在 `Mixer` 之後 Create、先解構），測試分不出拿掉 `OnDestroy` 的差別，沒有守衛；留著是因為停的先後不該依賴 Create 的順序（總宣告第 7 節、audio 頁第 4 節）。
- `scheduleDelay` 的 Windows、Linux 量測未做；合併前要不要先量，由使用者定。

## Design Invariants

- **SDL-1 SDL 只在 `plugins/sdl*` 的 adapter 目錄**：`SDL3/` 的標頭只被 `plugins/sdl*` 的 `src/adapters/sdl/` 與 `tests/adapters/sdl/` 底下的檔案 include（adapter 目錄裡可以有私有標頭，給 adapter 的幾個 `.cpp` 共用 SDL 的型別）；adapter 目錄外的檔案不 include adapter 目錄裡的任何檔案——adapter 給 System 用的介面標頭（我們的詞，不含 SDL）放在 adapter 目錄外，由 adapter 的 `.cpp` 實作；所以 SDL 不會經遞移的 include 漏到 System。System、runner、單位的 `Configure`／`Build`／`Check` 的程式裡沒有 SDL（建視窗、開裝置、claim 視窗也呼叫 adapter）；`engine/` 底下沒有 SDL。Plugin 之間的交接（視窗登錄表）用不透明的把手，型別定在 `sdl` 函式庫、不露 SDL 的型別（收窄使用者 11-13 的 (iii)，總宣告第 14 節核准清單第 6 件）。SDL3 對 `sdl*` 的 target 都是 PRIVATE 連結；`GYO::SDL` 本身不帶 SDL。守衛：原始碼掃描在子批 K 的 scenario 通過後立（總宣告第 9.1 節）；在那之前、以及 adapter 目錄外前置宣告 SDL 型別這種掃描判不了的，由每批的設計審查看。
- **SDL-2 沒有 SDL 也成立**：拿掉 `sdl*` 的 Plugin 與登記檔的列（與帶它們的產品），Engine、所有 Module、headless 的測試仍 configure、build、通過，SDL 不被下載、不被建。沒有產品選 `sdl*`、`GYO_PLUGINS` 也沒點名時，同樣不下載、不建。守衛：CI 的 core 步驟（L1 的 Linux 列、打包工作流程的 toolchain 列）——checkout 自己的建置與測試跑完之後，`build/ci/common/plugin_removal.py remove` 在 checkout 裡拿掉 `sdl*` 的列與目錄與帶它們的產品（暫存區刪除，經 git 列檔的測試照樣能跑），以 `core` preset、`GYO_PLUGINS=ALL` configure、build、ctest，`plugin_removal.py check` 確認 `gyo-build.json` 的 `plugins` 沒有 `sdl*`、`_deps` 沒有抓 SDL、CMakeCache 沒有 SDL 自己的選項（`SDL_*`，抓 vendored 或 ExternalProject 加進來的 SDL），最後 `restore` 把 checkout 還原成 HEAD。在 checkout 本身做、不在副本做：編譯快取的命中若跨兩棵原始碼樹，object 裡的 `std::source_location` 會是另一棵樹的路徑（L1 第二輪在 Windows 的 sccache＋MSVC 上碰到，`authority.authority_sources` 失敗）；同一個路徑編譯，快取的 key 也和以前一樣。本機要不動自己的樹時用 `copy`（HEAD 的 git 工作樹）；`cmake.PluginSelection`（整個 repo 什麼都沒選時不加任何 Plugin 目錄；configure 而已，沿用父建置抓好的來源、不連網）；`cmake.UnitRegistry`（選擇：依賴補齊、`ALL`、拒絕非 Plugin 的名字、產品登記）；`build.ci` 的 `test_plugin_removal.py`。突變：`docs/architecture/target/v2/scripts/sdl_mutations.json`。
- **SDL-3 一條時間線**（A 寫音訊這一半；輸入事件的時刻、runner 的取樣由 W 補）：送進混音器的時刻是 `Time::TimePoint`（audio DI 3）；`sdl_audio` 的回呼在每個裝置 block 開頭讀一次 `Time::Now()` 當 anchor，一個 block 分幾次算時，後一次的 anchor 是前一次的 anchor 加上已算的 frame 數換成的時間；SDL 的時刻（`SDL_GetTicksNS` 等）不出 adapter、不進混音器。守衛：`sdl_audio.dummy` 的「maps a play from the anchor of the block that takes it」（只在開裝置時讀一次 anchor 的寫法，起播會晚上一秒）；「分幾次算時後一次的 anchor」由審查守（dummy、disk 的 block 都小於一次的 4096 frame，測試走不到；命令在第一次 `Render` 就取完，後幾次的 anchor 幾乎不影響結果）。
- SDL-4、SDL-5、SDL-7 的草案在總宣告第 12 節，由 W、R2a 寫進本頁。SDL-6 已撤，號碼不重用。

## Replacement Criteria

總宣告第 12 節 RC-SDL 1～4（整條線）；各子批補自己的。

## 範囲の限界

- `remove` 要 checkout 的追蹤檔沒有改動（否則拒絕、請用 `copy`），`restore` 把所有追蹤檔放回 HEAD；中途失敗時 checkout 停在拿掉的狀態（CI 的那個 job 已經失敗）。`copy` 建的是 HEAD 的工作樹：本機還沒 commit 的改動不在裡面。
- 「帶 SDL 的產品」以產品目錄（與它的 `tests/`、`build/acceptance/`、`assets/` 底下同名目錄）的 CMake 檔裡有沒有 `sdl*` 的名字判（`#` 註解不算；多行的 `#[[ ]]` 註解只去掉第一行，後面的名字會多報、不會漏報）；產品以外的 CMake 檔還提到被拿掉的名字時，`remove`／`copy` 失敗並列出檔案。examples 目前是一個 `examples/CMakeLists.txt`：第一個帶 `sdl*` 的 example（W）要能被拿掉（例：每個 example 一個目錄、照目錄加入），由 W 定。
- SDL-2 的 CI 步驟只在 Linux（L1）與打包的 toolchain 列跑；Windows、macOS 的 L1 列建的是帶 SDL 的組態。
- 本機重用的建置目錄若留著以前抓的 `_deps/sdl3-*`，`check` 會報錯：用新的建置目錄。`copy` 會在本機 repo 登記一個 worktree（`git worktree remove` 拿掉）。
- master 留下的舊「component」機制（`GyoComponents.cmake` 的 `SDL_*`、`GYO_ENABLE_SDL3`，`GyoRuntimeDeployment.cmake` 的 `SDL3-shared`，`core` preset 的 `GYO_BUILD_SDL_*`）和 `plugins/sdl` 沒有接上；`GyoRuntimeDeployment` 在產品 configure 時查 `TARGET SDL3-shared`，早於 Plugin 加入，永遠是 false。留給重審產品登記機制的那批（Object_Connect 移植）處理，這批不動。
