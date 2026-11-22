# 事前宣告：SDL（第一個 Plugin 批）與 platform 的拆法（`v8-arch-gyo2-sdl`）

更新：2026-11-23（修訂 0）。Owner：架構（GYO2 主會話開的任務卡 `gyo2-sdl-declaration`，`local_2cc7e494`）。**狀態：草稿，等敵對式檢查與使用者的動工前核准。這張卡只寫宣告與分批計畫，不寫實作。**

**性質**：批次表第 6 組「SDL（第一個 Plugin 批）」那一列的事前宣告＋分批計畫（HANDOFF `v8-arch-gyo2-sdl-declaration-start`，使用者 2026-11-23「開 SDL 的宣告卡」）。範圍大，照「先立統合、再分子份」（`split-large-task-integrator-first`）：本頁是**統合**——定邊界、關聯表、要使用者定的事、各子批的先後與「模型／檔位」；各子批開工前各自再寫一份小宣告。動到 Engine 的公開介面、建置圖、Kernel 的 Extract、時間的取樣，照 AGENTS §2 當**基盤**處理（第 4 節）。

**用詞**：「**卡片照 L17 判**」＝這張卡照命名規範、別家原始碼、已定的原則自己判的，**不是使用者的話**。別家的事實標出處；**〔知〕**＝憑一般知識、這次沒用原始碼查證，不當決勝的根據。「Engine 的／作者的」是每一項的前提（`premise-engine-or-author`）：只有 Engine 做得到的（平台差異、跨 Module 的共用機制）是 Engine 的；作者用現有機制能做、而且做法本身是遊戲規則的，是作者的，Engine 只寫契約。

**核心一句**（每一項拿來對照）：**Engine 給視窗、輸入、畫面、文字、聲音輸出各一個公開出口（介面是我們的、用我們的詞），SDL 從外面注入成這些出口的實作；什麼時候開窗、按鍵對應什麼動作、畫什麼、UI 長什麼樣、聲音什麼時候響，是作者的。**

---

## 0. 根據與讀的版本

- 程式：`origin/gyo2` 7cf0861e（PR #284 之後）。`engine/core/time/include/engine/time/MonotonicClock.hpp`（SDL 時間戳要在平台邊界換算的註解）；`engine/ecs/kernel` 的 Extract 宣告那一半（`Params.hpp:1318-1340` 的 `Extract<Read<T>>`、`ExtractRes<R>`，`Registry.hpp:673-676` 的 `SetExtractSource`，`Declarations.hpp:90-91` 的 `extract` 旗標；**執行那一半隨第 4 批拿掉**，參數建構子會斷言 `Params.hpp:1325`）；`build/cmake/GyoUnitRegistry.cmake`（Plugin 登記檔已能讀、`plugins/<名>` 會被 `add_subdirectory`、**Module 依賴 Plugin 是 FATAL_ERROR**，`:164-165`；Plugin 不能 `default=yes`，`:120`）；`config/plugins.csv`（只有表頭）；頂層沒有 `plugins/`。
- 文件（文件分支 `origin/claude/gyo2-time-design` d315368c）：批次表 `rebuild-batch-plan.zh-Hant.md` 第 61 行（SDL 列）、第 66 行（audio 那一側的小批）、「不擋路的小事」；HANDOFF 的 `v8-arch-gyo2-platform-obsolete-split-into-modules`、`-os-in-core-platform-dirs`、`-json-adapters-dir`（含 11-20 的兩次推廣）、`-plan-gaps-2026-11-21`、`-audio-review-approved`、`-world-audio-withdrawn`、`-infrastructure-before-products`、`-module-plugin-owner-definition`、`-module-plugin-registers`、`-module-deps-in-registers`、`-products-pick-modules`、`-keeptool`、`-thread-binding-by-runner`、`-asset-adapter-world-asset`。
- 同詞異義表 `rebuild-vocabulary.zh-Hant.md` 第 0 節末（2026-11-16 改定）；AGENTS §6、§7、§9、§10。
- `audio.zh-Hant.md` 第 4 節（後端要求的介面）、第 5.2 節（「建混音器的是誰……哪一種由 SDL 那一批定」）、DI 2、DI 3、DI 7。
- 編排層 `orchestration.zh-Hant.md` O-3、O-4、O-6、O-8、O-12、O-13、O-15；交換點宣告 `exchange-declaration.zh-Hant.md` 5.4（Extract 的契約與留給 SDL 那批的）、5.5（取樣移給 runner，退出條件「第一個需要的 runner 那批」）、5.6（SDL 的三列）；`world-time.zh-Hant.md` 第 3 節、WT-2、WT-6 (2)；時間設計 `rebuild-time-design.zh-Hant.md` 5.5 的（二）（三）。
- Kernel 頁 DI-KS 13、14、15；`target/runtime-composition.zh-Hant.md` §4（11-13 的 plugins 層、SDL 家族、兩類 plugin；**v2 之前的文件，使用者 11-16「後面統一處理」**，只當系譜與已定的原則來源，和 11-16 定義衝突處以 v2 為準）。
- master（只看理由與坑）：偵察報告 `build/evidence/gyo2-sdl-declaration/master-platform-survey.md`（基準 origin/master 3b411e21；`engine/platform`、`input`、`render`、`text`、`ui`、audio 的 SDL 後端、`docs/architecture/platform-sdl.md`、相關 dev_log 29 個坑）。
- 別家：偵察報告 `build/evidence/gyo2-sdl-declaration/reference-engines-platform.md`（Bevy 本機 `~/Code/GitHome/bevy` 6a911653 的 bevy_window、bevy_winit、bevy_input、bevy_gilrs、bevy_render、bevy_extract、bevy_audio、bevy_text、bevy_ui／bevy_ui_render；Godot 經 WebFetch 查 master 的 DisplayServer、AudioDriver、InputEvent；Unity 以官方文件與〔知〕）。
- 兩份偵察報告都在**主 checkout** `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-sdl-declaration/`（本機 Intel Mac，git 不追蹤）；偵察者 Opus／high，只讀。

## 1. 先用遊戲講

**KeepTool 開起來**（使用者 11-14：「需要保障基礎的ImGUI沒問題，輸入法也能用」）：程式的 `main` 建一個 `Product`。選了 SDL 的 Plugin，它在 `Configure` 裝上「有視窗的 runner」。runner 每一輪在主執行緒上：處理 OS 事件 → 取一次時間 → `RunFrame`（World 依序跑，畫面的 World 最後經 Extract 讀前面的 World，把畫面交給 GPU）→ 等垂直同步。玩家在 ImGui 的文字框用日文輸入法打字：組字中的字要畫出來、按 Enter 確定時不能把 Enter 漏給程式（master 為此對 SDL 打過補丁）。

**FPS 的 Client 開槍**：滑鼠按下的時刻是 OS 記的（SDL 給的是 `SDL_GetTicksNS` 時間軸上的 ns）。這個時刻在 SDL 的 adapter 裡換成 Engine 的時間基準，之後所有人只看到 `Time::TimePoint`。混音器拿它排起播（audio DI 3）；網路的預測拿它判斷「這一下屬於第幾 tick」是作者（網路 Module）的事。

**讀檔換掉模擬的 World**（O-8）：畫面的 World 不跟著重建（GPU 的資源、pipeline 很貴），下一輪改讀新的模擬 World（O-12 Extract 第 4 點）。

**Match（沒畫面的伺服器）**：不選 SDL、也不選視窗、輸入、畫面的 Module；建得起來、跑得起來，和 SDL 完全無關。刪掉整個 `plugins/sdl` 也一樣（AGENTS §7）。

## 2. 前提表

每個前提：誰定的、哪些決定掛在它上面。標「使用者」的是使用者的話；標「L17」的是主會話或卡片自己判的，可以被推翻。

| # | 前提 | 誰定的（出處） | 掛在它上面的決定 |
|---|---|---|---|
| P1 | master 的 `engine/platform` 不帶回，功能拆進各 Module | 使用者 2026-11-19（`-platform-obsolete-split-into-modules`） | 第 3 節的拆法全部 |
| P2 | Module＝我們維護、介面是我們的（外部函式庫完全藏在我們的介面後面也算）；Plugin＝第三方官方維護、接進來用的；歸屬：SDL 家族（視窗／輸入／GPU 後端、SDL_image、SDL_ttf）與 ImGui 是 Plugin | 使用者 2026-11-16（`-module-plugin-owner-definition`；同詞異義表第 0 節末）。**未確認**：SDL 的 adapter 歸哪、ufbx 歸哪 | Q1（adapter 放哪）、Q2（ufbx） |
| P3 | Plugin 可以依賴 Module，Module 不能依賴 Plugin；Module 先登記 | 使用者 2026-11-14（`-module-plugin-registers` 第 (5) 點）；建置層已實作成 FATAL_ERROR | Q1：A' 與 B 的依賴形；Plugin 單位的 `Configure` 一定在所有 Module 之後 |
| P4 | 外部組件的型別與呼叫只在它的 adapter 裡；一個外部組件一個 adapter；adapter 的介面由使用方的詞定；「何を一つの外部 Component と数えるか」寫在架構文件 | AGENTS §6；使用者 2026-11-13（`v8-arch-external-components-as-adapters`） | Q1 各選項的規則文字代價 |
| P5 | Module 裡碰外部組件的程式放 `src/adapters/<組件>/`（不用 backend 等名字） | 使用者 2026-11-20（`-json-adapters-dir` 與兩次推廣） | Q1 的選項 A／A' 的目錄；批次表 SDL 列「SDL 放 `src/adapters/sdl/`」是主會話照這條寫的 |
| P6 | `core/os` 用 `src/<平台>/`；`src/adapters/<組件>/` 是 Module 的結構；視窗、輸入、聲音裝置不進 core/os | 使用者 2026-11-21（`-os-in-core-platform-dirs`） | 本線不改 core/os 的目錄；SDL 的東西一律不進 core |
| P7 | 模擬的 World 與渲染的 World 分開；渲染的 World 有自己的 Registry；只經 Extract 以 const 讀來源；不寫回 | 使用者 2026-11-11（K.3 第 6 項、DI-KS 13、runtime-composition 3b） | 第 6 節；render 子批 |
| P8 | 資料進出 World 的四種形（套用點、交換函式、runner 直接讀寫、Extract），同一件事不兩種形；Extract 的契約 1～6 | 使用者 2026-11-22 合併前核准（O-12；交換點宣告 5.4） | 第 6 節；輸入怎麼進 World |
| P9 | Product 一份的共用處、`OnDestroy` 照登記的反序、在所有 World 之後、共用處之前 | 使用者 2026-11-22（O-15） | 裝置與後端放共用處；audio 後端的停法 |
| P10 | audio 對輸出後端的要求（每 block 一次 anchor、一條執行緒呼叫 `Render`、只持參照、`OnDestroy` 先停、後端登記排在建混音器的單位之後、錯誤碼是後端自己的） | 使用者 2026-11-22（audio 頁第 4 節） | 第 7 節 |
| P11 | 輸入事件的到達時刻在平台邊界換成時間基準，歸 SDL 那批；送進混音器的時刻是時間基準 | 使用者 2026-11-22「照建議」（`-world-audio-withdrawn` 的 1'）；audio DI 3；`MonotonicClock.hpp` 註解 | 第 8 節 |
| P12 | World 時間的取樣移給 runner，退出條件是「第一個需要 runner 取樣的 runner 那批」（有視窗的就是本線） | 使用者 2026-11-22 合併前核准（交換點宣告 5.5、world-time 第 3 節、WT-6 (2)） | 子批 W（有視窗的 runner）要一起定 |
| P13 | 先建基礎設施、第一個搬回的產品不定 | 使用者 2026-11-19（`-infrastructure-before-products`） | 本線不搬 PvP；驗收對象用 KeepTool 與 examples |
| P14 | KeepTool 要有實質內容：基礎 ImGui 沒問題、輸入法能用；交給 GYO2 會話排成一批 | 使用者 2026-11-14（`-keeptool`） | 第 5 節的 Feature 錨；子批 K |
| P15 | 產品可以只選部分 Module | 使用者 2026-11-17（`-products-pick-modules`） | Match 不選視窗類 Module |
| P16 | 進 World 的 Module 預設全帶、產品可關（例外：save_version 預設不啟用） | **L17**（主會話，編排層骨架宣告；`-products-pick-modules` 記「預設全帶可關／預設不帶要列」那一層使用者沒有回） | 第 3.3 節：視窗類 Module 本卡照 L17 判成 `default=no` |
| P17 | ui 留 Engine（`gyo.ui` 是 Engine 的能力） | 使用者 2026-11-13（kernel HANDOFF `v8-arch-adapter-plugins-review`，`docs/architecture/plans/kernel/HANDOFF.md:92`） | 子批 U |
| P18 | Engine 不給標準 `main`；產品的 `main` 怎麼接 runner 的結果，例子在 `examples/orchestration/product_main.cpp` | 使用者 2026-11-21（O-3 退出流程） | `SDL_main`／`GyoMain` 不帶（第 3.1 節） |
| P19 | runner 決定哪條執行緒跑哪個 World；需要綁執行緒的 Resource 自己記、碰錯斷言 | 使用者 2026-11-16（`-thread-binding-by-runner`）；DI-KS 14；O-13 | SDL 的主執行緒規則落在 runner 與後端，不落在 Kernel |
| P20 | 圖片解碼用 stb_image、不走 SDL；loader 歸擁有型別的 Module，第三方放該 Module 的 `src/adapters/` | 使用者 2026-11-21～22（asset 宣告，`-asset-three-decisions`、動工前核准） | SDL_image 不帶；Q2（ufbx）的先例 |
| P21 | 使用者保留的想法（**不是決定、不在正式文件**）：SDL 是 Plugin，只留「Module 介面＋`adapters/sdl`」一個概念，adapter 放在各 Module 底下當後端；不要 `native/`，後端之間靠 SDL 自己協調 | 使用者 2026-11-18（主會話記憶；其中「不再有 platform」11-19 已成 P1） | Q1 的選項 A' |

**卡片自加的前提**（不是使用者的話，列出來讓使用者能推翻）：
- C1：SDL 3.4 的視窗、事件泵、swapchain 的取得與送出只能在建立視窗的那條執行緒（主執行緒）——出處 master 偵察 2 節末（`SDL_gpu.h:4066-4094`、`:4202-4218`，master D27）。
- C2：一個視窗只能被 SDL_GPU 或 SDL_Renderer 其中之一使用——master 偵察 4 節（`2026_11_04_tool_v8_render_offscreen.zh-Hant.md:22-30`）。
- C3：ImGui 官方的 SDL3 平台後端吃 `SDL_Window*`／`SDL_Event`、SDL_GPU 繪製後端吃 `SDL_GPUDevice*`——它們只能寫在看得到 SDL 型別的地方〔知，ImGui v1.92 的 `imgui_impl_sdl3`、`imgui_impl_sdlgpu3`；實作批用原始碼確認〕。

## 3. platform 的每一項功能拆到哪裡（要涵蓋的第 1 項）

### 3.1 master 的 `engine/platform/sdl`

master 的 platform 同時扮三個角色（偵察 9 節末）：(a) 視窗與主執行緒事件泵的擁有者（input、GPU 後端都掛在它的視窗上）、(b) process 服務（入口、執行檔目錄、log 轉接、剪貼簿、檔案對話框）、(c) SDL 時間換到 Engine 時間的唯一位置。

| master 的功能 | 去處 | 前提 | 理由 |
|---|---|---|---|
| 視窗建立、查詢（視窗、螢幕）、操作、視窗事件 | **新 Module `window`**（介面）＋SDL 的實作（Q1） | Engine：平台差異 | Bevy `bevy_window`（介面）／`bevy_winit`（adapter）；Godot `DisplayServer` |
| 事件泵（主執行緒、一輪處理完才丟例外） | 有視窗的 runner 呼叫、SDL 的實作裡做；事件依類型分給 window、input、ImGui 後端 | Engine：SDL 的主執行緒規則（C1） | Bevy：`WinitPlugin` 用 `set_runner` 接管主執行緒（偵察 1.2）；master 坑 4（提早跳出會截斷下一次 pump） |
| 像素大小與密度（DPI） | `window` 的查詢一開始就分「邏輯大小」與「像素大小」 | Engine | master 從沒做（坑 22），macOS Retina、Windows 縮放上每個有視窗的 Feature 都會碰到；只做查詢，不做全螢幕模式 |
| live frame（OS 的 modal 迴圈中畫圖） | **不在薄起步**；`window` 的介面留一個點（第 4.2 節 R3），PvP Client 移植時做 | Engine，但沒有本線的 Feature | master 坑 1～3：只解了一半，最後靠分執行緒（D27）；KeepTool 縮放時卡一下不違反它的 Feature |
| 剪貼簿 | **不帶**到 Engine 的介面；ImGui 的 SDL 後端自己處理；`gyo.ui` 的文字框要用時再加到 `window` | 有 Feature 時是 Engine 的 | 沒有本線點名的 Feature；Bevy 是獨立 crate（arboard），Godot 在 DisplayServer |
| 檔案對話框 | **不帶**；工具回來時在工具那一批定 | 作者（工具）的需求先出現 | master 只有三個工具用；`engine-not-tool-duties`（使用者：Engine 不承擔工具職能）；SDL 的回呼在任何執行緒、可能回兩次（master 坑，偵察 2 節）——那時要照 master 的 process 級登錄表做 |
| log 轉接 `SdlLogBridge` | **不帶**；有點名需要（SDL 內部的警告要進日誌檔）時，在 SDL 的實作裡接 logging，一個地方 | 那時是 Engine 的 | master 只有 PvP 用；兩個坑（解構順序的遞迴卡死、測試握手），帶回時照修過的順序 |
| 程式入口 `GyoMain`／`SDL_main` | **不帶** | — | P18（Engine 不給標準 main）；桌面三平台不需要 `SDL_main`；Windows 的 UTF-8 命令列由 os 的 manifest（`activeCodePage=UTF-8`）處理；Windows GUI 子系統用連結選項保留 `main`（子批 K 驗）。行動平台的入口列為範圍的限界 |
| 執行檔目錄 `BaseDirectory` | 需要時加到 `core/os`（`OS::Files` 一族），**不經 SDL** | Engine：每個產品都要、不需第三方（P6 的界線） | 和 `UserDataDirectory`（PR #280）同處；asset 找根目錄時才加，不在本線先加 |
| `PlatformName` | 不帶 | — | 只供診斷 |
| 時鐘配對與時間戳換算 | SDL 的輸入實作裡（第 8 節） | Engine | P11 |
| `native/`（帶 SDL 型別的 API） | **不帶** | — | P4、P21；SDL 型別的交接留在 SDL 的實作裡（Q1 各選項的寫法不同，見 4.1） |

### 3.2 master 的 `input`、`render`、`text`、`ui`、audio 的 SDL 後端

| master | 去處 | 前提 | 說明 |
|---|---|---|---|
| `GYO::Input`（「狀態＋依序事件」雙軌的幀、文字與組字、失焦清鍵） | **新 Module `input`**（不含 SDL） | Engine：鍵盤、滑鼠、文字輸入的中性資料模型 | 雙軌的理由（同幀按下又放開、事件先後）照 master；`text` 與 `events` 不保證順序也照 master（RC 3）|
| `InputActionMap`（按鍵→動作） | **不在薄起步** | 作者（按鍵對應什麼動作是遊戲規則）；有共用需求時再看 | 別家分歧：Bevy 本體沒有（第三方 crate），Godot 有 `InputMap`、Unity 有 Actions。延後不等於否決，PvP 移植時用它的需求再判 |
| 手把 | 不帶 | — | master 沒有程式；沒有點名的 Feature |
| `GYO::Render`（中性 device 介面、`Renderer`、固定 pass、shader ABI `gyo.raster.v1`、讀回） | **新 Module `render`**（中性介面）＋SDL_GPU 的實作（Q1）＋渲染的 World 與 Extract（第 6 節） | Engine | master 沒有 extract、產品每幀自己填 `RenderQueue`（偵察 4 節）——這一點是本線要換的形 |
| `GYO::RenderBackendSDL`（SDL_Renderer 2D，給 ImGui 工具） | **不帶**；ImGui 改走 SDL_GPU 的繪製後端 | — | **卡片照 L17 判**：C2 讓 master 的工具把 3D 預覽畫到離屏再當貼圖上傳；統一在 SDL_GPU 就少一個後端、少一條離屏繞路（「能拿掉的先拿掉」）。風險：ImGui 的 SDL_GPU 後端比 SDL_Renderer 後端新〔知〕，子批 K 驗 |
| shader 管線（共用 HLSL → host 工具 DXC＋SDL_shadercross＋SPIRV-Cross，釘版本與 SHA256） | render 子批帶回；host 工具照 11-13 放在 SDL 的 GPU 那一側，Engine 留 HLSL 原始碼與 ABI | Engine | runtime-composition §4「SDL 家族」（11-13） |
| `GYO::Text`（中性）＋`TextBackendSDLTTF` | **新 Module `text`**（中性）＋SDL_ttf 的實作（Q1：B 時是 `sdl_ttf` Plugin，依賴 `sdl`） | Engine | master 已是「介面＋後端」、不經 platform（偵察 9 節） |
| `GYO::Ui`（`gyo.ui`：codec、驗證、layout、互動、文字框）＋`UiRenderer` | **新 Module `ui`**（不含 SDL）；渲染那一半經渲染的 World | Engine（P17） | master 172 KB 裡最大的是 runtime、codec、驗證；JSON 改走 JSON Module（不帶 nlohmann）。照 Bevy `bevy_ui`／`bevy_ui_render` 分兩半 |
| `GYO::AudioBackendSDL` | SDL 的音訊實作（第 7 節）；混音器的建立者是 audio 這一側的登記單位 | Engine（P10） | |
| `GYO::AssetSdlImage` | **不帶** | — | P20（stb_image） |
| ufbx（`GYO::AssetUfbx`，model 的匯入器） | model 回來時，model Module 的 `src/adapters/ufbx/`（Q2） | Engine | 不在本線（render 沒用它，偵察 8 節） |

### 3.3 新 Module 的清單（名字照命名規範 N7、N8、N20、N21，卡片照 L17 判）

| Module | 目錄 | 進 World | `default` | 依賴（登記檔） | 本線的子批 |
|---|---|---|---|---|---|
| `input` | `engine/modules/input` | 是（Resource、`Message`、一個套用 System） | no | — | W |
| `window` | `engine/modules/window` | 是（視窗的 Resource、視窗事件） | no | input | W |
| `render` | `engine/modules/render` | 是（渲染 World 裡的 System） | no | window | R1、R2 |
| `text` | `engine/modules/text` | 否（`library_only`） | no | — | T |
| `ui` | `engine/modules/ui` | 是 | no | text、render、input、json | U |
| audio 的登記單位（名字子批 A 定：audio 改成有登記單位，或另一個小 Module） | — | 否 | no | audio | A |

`default=no` 是**卡片照 L17 判**、和 P16 不同的地方：這些 Module 沒有 SDL（或別的）實作就做不了事；預設帶會讓沒選 SDL 的產品（Match、headless 測試）在建置時就錯（11-13 定「產品用到某能力卻沒選它的後端，是建置 configure 時的明確錯誤」）。Godot 的 headless 是明確選一個 dummy，不是預設帶〔知〕。被推翻的話改成「預設帶＋沒有實作時用空實作」，代價是忘了選 SDL 時不報錯。

`window` 依賴 `input`：照 Bevy（`bevy_window` 依賴 `bevy_input`，總 enum `WindowEvent` 也裝輸入，偵察 8.2）。視窗的實作產生輸入事件，交給 `input` 的服務；`input` 不認得視窗，測試可以只用它。

## 4. 基盤 Architecture 的根據（AGENTS §2）

**為什麼是基盤**：`window`、`input`、`render` 的介面上面會積產品（KeepTool、四個工具、PvP Client）、ImGui 的接法、`gyo.ui`；Extract 的執行與來源替換是 Kernel 與編排層的契約；Plugin 的目錄與登記是建置圖；有視窗的 runner 決定時間的取樣。改它們，上面積的全部要跟著改。

### 4.1 予見的根據

| 根據 | 種類（§2） | 導出的基盤要求 |
|---|---|---|
| KeepTool（使用者 11-14） | 1 名指的 Feature | 視窗、鍵盤滑鼠、**文字輸入與組字**（IME）、ImGui 能畫在 SDL_GPU 上、tools 線能打包；刪掉 KeepTool 回到零工具（keeptool 列的移除條件） |
| PvP Client（master 的完整遊戲，P13 的對照） | 1（計畫上的 Feature，`-infrastructure-before-products` 的「有一個完整遊戲作為對照」） | 輸入帶 OS 時刻（預測、聲音）；3D 畫面＋HUD 文字＋`gyo.ui` 選單；模擬在自己的執行緒、主執行緒＝事件＋畫面（O-13、時間設計 5.5（三））；live resize；讀檔換模擬 World 時畫面不重建 |
| 四個工具（voxel_map_editor、audio_editor、model_viewer、ui_editor，master） | 1（之後回來） | ImGui、3D 預覽、音效預覽、檔案對話框（工具那批）、`gyo.ui` 的編輯外殼 |
| Match（沒畫面） | 1 | 不選 SDL 也建得起、跑得起；刪掉 `plugins/sdl` 仍成立 |
| 一般的遊戲（單機） | 1（GYO2 的目標：通用引擎） | 單機不因 PvP 的強化變複雜：薄起步只有單執行緒、渲染 World 在主執行緒 |
| Bevy、Godot、Unity、master | 2 成熟的參考實作 | 第 4.3 節的系譜 |
| 規模與預算 | 3 | 畫面 60～144 Hz；音訊 `scheduleDelay` 預設 60 ms（master：33.3 ms＋macOS 回呼最長 21.5 ms＝54.8 ms，Windows 44.4 ms，偵察 6 節）——子批 A 重新推導；幀時間的預算由子批 R2、K 的宣告寫（照 §14.1 先宣告） |

### 4.2 予見的要求寫成邊界（實作薄起步，其餘只定形）

| # | 要求 | 本線實作 | 只定形（Contract／Interface），留給點名的 Feature |
|---|---|---|---|
| R1 | 一個公開出口、實作注入 | `window`、`input`、`render`、`text` 各一個後端介面（注入的形在子批 W 定，給其他子批照用） | 第二個實作（例：另一個視窗函式庫）出現前，「替換」只是預留（runtime-composition §4 的範圍限界） |
| R2 | 測試不靠 SDL 的 dummy 驅動 | 每個介面都有測試用的假實作；Module 的行為測試用假實作、不連 SDL | SDL 的實作自己的測試照 master 用 `SDL_VIDEODRIVER=dummy`、Lavapipe（坑 26、27） |
| R3 | live frame | 介面留「OS modal 迴圈中要一幀」的回呼點（宣告形狀，不實作） | PvP Client 移植 |
| R4 | 多個視窗 | 介面以視窗 id 為鍵，薄起步只開一個 | 工具要多視窗時 |
| R5 | 模擬在別的執行緒 | 薄起步單執行緒；渲染 World 在主執行緒 | O-13 的入口在 PvP Client 移植那批（已核准的排法） |
| R6 | 渲染 World 的實體與來源對應 | 薄起步每輪重抽（不保留對應） | 量到超過預算時改成保留（Bevy 0.15 起的 `RenderEntity`／`SyncToRenderWorld`，偵察 3.6），RC-SDL 3 |

### 4.3 系譜、退掉的選項、需要它的 Feature（AGENTS §4）

| 形 | 系譜 | 退掉的選項與理由 | 需要它的 Feature／Product |
|---|---|---|---|
| 視窗與輸入分兩個 Module、輸入不認得視窗 | Bevy（bevy_window／bevy_input，一個 adapter 產生兩種事件） | (i) 合成一個（Godot `DisplayServer`）：介面會變成數百個方法、只能整組換、輸入測試要帶視窗〔Godot 部分已查 URL、理由是推論〕；(ii) 照 master（input 後端掛在 platform 的視窗上、讀原生 `SDL_Event`）：那是 `native/` 的形，為舊形狀找位置 | KeepTool、PvP Client；input 單獨給測試與網路的輸入注入 |
| 事件泵在有視窗的 runner、由 SDL 的實作做 | Bevy `WinitPlugin::set_runner`；runtime-composition §4「runner plugin」（11-13） | (i) 泵放在 `window` 的 System 裡：SDL 要求主執行緒（C1），System 在哪條執行緒跑是 runner 定的（P19），放 System 裡等於讓 Kernel 的排程替 SDL 守執行緒；(ii) 泵放在產品的 `main`：每個產品抄一份（機制散在實作裡） | 所有有視窗的產品 |
| 輸入經「輸入服務＋套用點」進 World | O-12 套用點；Bevy `bevy_gilrs`（服務＋System，偵察 2.5） | runner 直接寫進 World（Bevy winit 的做法）：runner 要認得輸入的型別、每個讀輸入的 World 都要 runner 寫一次；交換點宣告 5.6 留給本批選，**卡片照 L17 判**：多個 World（UI 的、模擬的）各自在 `Main`／`First` 取，和 System 排先後 | KeepTool（一個 World）、PvP Client（兩個 World 都要輸入） |
| ImGui 畫在 SDL_GPU | ImGui v1.92 的 `imgui_impl_sdlgpu3`〔知〕 | SDL_Renderer＋離屏 3D（master）：一個視窗兩條渲染路（C2），工具的 3D 預覽要畫兩次 | KeepTool、四個工具 |
| 渲染 World＋Extract | Bevy `SubApp::extract`、`bevy_extract`（偵察 3.1～3.3）；使用者 11-11 | master 的「產品每幀填 `RenderQueue`」：渲染讀的是產品自己整理的副本，模擬與渲染之間沒有唯讀的保證；Bevy 的 `ResMut<MainWorld>` 寫回（偵察 3.4）：使用者 11-11 不採 | PvP Client、工具的預覽、每個 3D 遊戲 |

既有的測試、守衛、golden、突變能不能不改，沒有當決勝的理由（§4）。

## 5. Feature → Contract → Architecture（AGENTS §2 Derivation Order）

| Feature | Contract | Architecture |
|---|---|---|
| KeepTool：開窗、ImGui 文字框、日文輸入法組字與確定 | `input` 的文字與組字事件（含「組字中不交出 Enter」，master 的 SDL 補丁）；`window` 的視窗 id 與像素大小 | `window`、`input`、SDL 的視窗與輸入實作、有視窗的 runner、ImGui 的 Plugin、SDL_GPU 的繪製 |
| 任何遊戲：按鍵、滑鼠 | `input` 的幀（雙軌）；輸入只在套用點進 World | `input` Module、套用 System |
| PvP Client：輸入時刻、開槍的聲音準時 | 事件時刻是 `Time::TimePoint`（第 8 節的四條保證） | SDL 輸入實作裡的時鐘配對 |
| 任何 3D／2D 遊戲：畫面 | 渲染 World 只讀來源（DI-KS 13）；來源被替換後讀新的（O-12 第 4 點）；畫面送出是 `render` 的服務 | Kernel 的 `RunExtract`、編排層的 `extractFrom`、`render` Module、SDL_GPU 實作 |
| HUD 與選單 | `text` 的點陣圖形狀；`gyo.ui` 的資料契約（JSON v1／v2） | `text`、`ui` Module |
| 任何遊戲：聲音 | audio 頁第 4 節 | SDL 的音訊實作、audio 這一側的登記單位 |
| Match：沒有 SDL | Module 不依賴 Plugin；視窗類 Module `default=no` | 刪掉 `plugins/sdl` 的建置測試 |

## 6. 渲染的 World 與 Extract（要涵蓋的第 4 項，K.3-6）

**前提**：Engine 的（Kernel 的執行原語、編排層的來源與時機，作者做不到）。

**現況**：Kernel 只剩 Extract 的宣告那一半（第 0 節）；執行隨第 4 批（rollback-extract）拿掉；編排層沒有呼叫 `SetExtractSource` 的地方。

**本線要做的**（O-12 Extract 契約 1～6 的實作；交換點宣告 5.4「留給 SDL 那批的」逐項）：
1. **Kernel 的原語**：`WorldExecutor::RunExtract(schedule, const World& source)`（暫名，交換點宣告 5.4 第 3 點）。Extract 參數綁到來源的 const 資料；屬 DI-KS 15 第 (3)、(4) 項之內。今天最外層 `Run` 跑 extract schedule 會撞到參數的斷言——改成走 `RunExtract`；`Run` 指名 extract schedule 是 Programmer Error（斷言，不加 `StartupCode`）。
2. **編排層的描述欄位**：讀者 World 的描述帶來源 World 名（暫名 `extractFrom`）；來源不存在、排在讀者之後、讀者自己是來源的來源（成環）是啟動錯誤（新的 O-E 碼，子批 R1 定號）。編排層在讀者第一次 `BuildPlans` 之前呼叫 `SetExtractSource`。
3. **清單**：extract schedule 照樣寫在讀者的每幀清單（看到 `extract` 旗標就走 `RunExtract`）；讀者的交換函式「進」在它之前。**卡片照 L17 判**：extract schedule 必須是讀者清單的第一張，否則啟動錯誤——理由：在它之前跑的 schedule 讀到的是上一輪抽的資料，同一輪兩種新舊混用（Bevy 的 extract 也在 render 的一輪最前面，偵察 3.3）。
4. **來源被替換**：選「**准許換來源**」（讀者下一輪重新 `SetExtractSource`、重建 Plan），不選「讀者一起重建」。**卡片照 L17 判**：讀檔換模擬 World 時，GPU 的資源與 pipeline 不該跟著重建（第 1 節例子）；Bevy 的 RenderApp 也活過 main world 的變化；DI-KS 13 已寫「來源再登記後，讀它的 Registry 下一次建 Plan 讀到新的內容」。要放寬的是 `SetExtractSource` 的「第一次 `BuildPlans` 之前」（`Registry.hpp:673-676` 的註解），改成「兩次 Run 之間、下一次 `BuildPlans` 之前」。`DropOldFirst` 時來源的位置空了 → Product 已結束（O-11），不抽。
5. **銷毀**：讀者先於來源（O-15「讀者先於 Extract 的來源」，已有）。
6. **來源壞了**：Product 已結束，不跑 Extract（O-11，已有）。
7. **渲染 World 的時間**（交換點宣告 5.5「渲染 World 的時間從哪裡來」、時間設計 5.5（二）「沒寫的」）：**卡片照 L17 判**：渲染 World 的描述不帶 world_time；內插要的量（`FixedTime` 的餘數、步長）從來源經 `ExtractRes` 讀。理由：渲染 World 自己取樣會出現第二個時間來源（WT-2「每個 World 每幀只取一次樣」是對各 World 說的，兩個 World 各取一次就有兩個「現在」）；Bevy 的 render world 也用 main 的 `Time`（偵察 3.4，經 channel 推 `Time<Real>` 是它的寫回，不照）。
8. **實體對應**：薄起步每輪重抽到渲染 World 的 Resource（畫什麼的清單），不在渲染 World 裡保留來源實體的鏡像（R6）。

**O-12 的四種形在本線各管什麼**（「同一件事不兩種形」）：

| 資料 | 形 | 理由 |
|---|---|---|
| 模擬 World → 渲染 World | **Extract**（只有這一條 World 到 World 的路） | P7 |
| OS 事件 → 讀輸入的 World | **套用點**（輸入服務在共用處，`Main`／`First` 的套用 System 取） | 第 4.3 節 |
| 渲染 World → GPU（送出畫面） | **套用點「出」**（`render` 的服務在共用處，渲染 World 的 `Main`／`Last` 一個 System 送） | 和 audio 5.3 同形：服務單一生產者，集中一個 System |
| runner 的時間取樣 → 各 World | **交換函式「進」**（world_time 登記，把 runner 給的取樣寫進 `TimeSampling` 的 `ManualInstant`；交換點宣告 5.5 已留的接法） | P12 |
| 混音器、裝置、視窗 | 不進 World：放共用處（O-15），World 裡只放指向它的 Resource | P9 |

交換函式和 Extract 的關係：交換函式是**登記單位**在一個 World 的一輪前後碰 **World 外的服務**（可以寫 World）；Extract 是**讀者 World 的 System** 在自己的一輪開頭唯讀地讀**另一個 World**，存取由 Kernel 推導與檢查。順序：讀者的交換函式「進」→ extract schedule → 讀者其餘的 schedule → 讀者的交換函式「出」。

**執行緒**：薄起步所有 World 在主執行緒（C1：swapchain 只在建視窗的執行緒）；模擬交給別的執行緒時（O-13），Extract 是唯一的同步點（Bevy pipelined rendering 的形，偵察 3.5），入口在 PvP Client 移植那批。不開 render 執行緒（master D27 的理由照舊）。

**讀回**：GPU 讀回只給測試與驗收（截圖、比對顏色），回給呼叫的人，不寫進任何 World（DI-KS 13；runtime-composition 3b 的選取用 CPU 射線）。

## 7. audio 的 SDL 輸出後端（要涵蓋的第 5 項）

**前提**：Engine 的（開裝置是平台差異；停的先後是 DI 7 的即時安全，作者不該自己踩）。

- **誰建混音器**（audio 頁 5.2 留給本批）：audio 這一側的登記單位在 `Configure` 照 `ProductDescription::options` 建 `Mixer`，放進共用處（O-15）。SDL 的音訊實作是排在它後面的單位（Q1 選 B 時是 Plugin，Plugin 一定在 Module 之後，P3），在 `Configure` `Find` 混音器、開裝置、登記 `OnDestroy`。`OnDestroy` 照反序先跑 → 裝置先停 → 共用處最後才解構混音器（audio 頁第 4 節第 2 點）。產品自己的 `configure` 在所有單位之後，能 `Find` 混音器建提交口（5.2 的 `GameAudio` 寫法照舊）。
- **實作**（照 master 修過的做法，偵察 6 節）：`SDL_InitSubSystem(AUDIO)`（參考計數）→ `SDL_OpenAudioDeviceStream(預設播放裝置, S16／2ch／48000, 回呼)` → `SDL_ResumeAudioStreamDevice`。回呼在 SDL 的音訊執行緒：每 block 開頭讀一次 `Time::Now()` 當 anchor、`Mixer::Render(span, anchor)`；不丟例外、不配置、不上鎖。後端只持不擁有的參照（DI 7）。停：`SDL_DestroyAudioStream`（返回後不再有回呼）→ `SDL_QuitSubSystem(AUDIO)`。
- **錯誤**：裝置開不了是後端自己的錯誤碼（audio 頁第 4 節第 3 點）。**卡片照 L17 判**：不經 `ProductSetup::Report`（那會讓整個 Product 不建 World——沒有音效卡的機器玩不了不出聲的遊戲）；改成後端把狀態放在共用處，作者讀了決定要不要停。這是「告訴外面發生了什麼、不替外面決定」（`errors-inform-not-prescribe`）。
- **`scheduleDelay`**：子批 A 用量測重新推導（Windows 主戰場、macOS、Linux），量測的判定照 §14.1 在子批 A 的宣告先寫好再量。
- **DI 2 不動**：`GYO::Audio` 照舊只連 Base 與 Time；後端是另一個 target。

**「audio 那一側的小批」（clip 來源、音效 loader）要不要併進本線**：**建議維持另做**（asset 合併後），不併。理由：(1) 它的兩件（`ClipBank` 留不留、`"sound"` 的資料契約與 loader）都是資料層，用現有的 `Mixer::Render` 測試與 asset 的 loader 測試就驗得到，不需要真的裝置；批次表寫的「那時才有真的 `Render` 可驗」——真的裝置驗的是後端，不是 clip 的來源；(2) 它依賴 asset（PR #283 快合併），不依賴 SDL；本線大、併進來會拖慢它；(3) 兩者交界只有「混音器由誰建」，本節已定，小批照用。批次先後是主會話照 L17 排的，這裡只是建議。

## 8. 輸入事件的到達時刻（要涵蓋的第 6 項）

**前提**：Engine 的（SDL 的時鐘域和 Engine 的不同，只有 SDL 的實作知道；P11）。

- **做法**（照 master `SdlPlatform.cpp:645-674` 修過的版本，偵察 3.3）：每次泵事件時取一組配對——依序讀 `Time::Now()`、`SDL_GetTicksNS()`、`Time::Now()`，Engine 的時刻取兩次讀數的中點、記下兩者差（bracket）。事件的 SDL 時刻 `t` 換成 `min(中點 + (t − SDL 讀數), 送達時的 Now())`；`t == 0`（`SDL_PeepEvents` 加的、沒有時間）用送達時刻。無號差以 2^64 取模轉 int64（事件早於配對時對到配對之前、沒有 UB）。三次讀取的順序與中點固定在一個吃讀取函式的純函式，測試用假的讀取函式以精確等式檢查（master D34）。
- **保證**（帶回 master `platform-sdl.md:74-91` 的契約）：(1) 不晚於送達；(2) 同一次泵裡 SDL 時刻非零且不遞減的事件，換算後不遞減；(3) `t == 0` → 送達時刻。**不保證**：全序、跨兩次泵的先後（SDL 混用 OS 的事件時刻與 push 的時刻，誤差約 ±bracket/2）。
- **已知限制**（記錄、不修，列入第 12 節）：入列與泵之間的睡眠讓 macOS 的結果偏移睡眠長度；Linux 兩鐘速率可差約 500 ppm；Windows 事件時刻是毫秒解析度（`GetMessageTime`）。
- **SDL 的時間不出 SDL 的實作**：`input` 的事件只帶 `Time::TimePoint`（audio DI 3 的同一條時間線）。
- **不做的**：事件時刻對到第幾 tick——作者的（網路 Module 或產品）；Unity 用時刻把事件分到 FixedUpdate 的時間片（偵察 7.3），Bevy、Godot 用「幀」，這是作者的選擇，Engine 給時刻就夠。

**「送進 World 外服務的時刻都是時間基準」要不要成跨 Component 規則**（audio 重審交圖書管理員的那一條）：本線第二個例子出現（輸入），**建議**本線子批 W 合併時由圖書管理員登記成 CCR（AGENTS §1 的登記頁），本卡不自己寫。

## 9. plugins 目錄的建置（要涵蓋的第 2 項）

**前提**：Engine 的（建置圖）。

- **已有的**：Plugin 登記檔 `config/plugins.csv`、`GyoBuild.cmake` 照登記檔 `add_subdirectory(plugins/<名>)`、Module 依賴 Plugin 是 FATAL_ERROR、產品經 `gyo_product_units(... PLUGINS ...)` 選、執行時的登記單位表由清單產生（O-6）。
- **本線補的**（子批 P）：
  1. 頂層 `plugins/`（AGENTS §10：11-11 已同意的新頂層，`runtime-composition` §4；同詞異義表第 0 節）。
  2. 第三方的取得：SDL3 照 master 用 FetchContent `release-3.4.0`＋補丁（`gyo-cocoa-ime-composing-keydown.patch`，帶它的移除條件；設定時檢查補丁有沒有上，偵察 8 節）。放在 `third_party/`（和 yyjson 同處），只有 SDL 的實作連它。
  3. **刪掉一個 Plugin 時 Engine 仍能建置**：CI 的組態檢查——拿掉 `plugins/sdl` 的列與目錄（測試用暫存副本），Engine、所有 Module、headless 的測試照樣 configure、build、ctest（守 AGENTS §7 的 Removability）。
  4. **Engine 不 include `plugins/`**：掃描（`engine/` 下沒有 `plugins/` 的 include 路徑、沒有 SDL 標頭）。照 AGENTS §3「實作方式的守衛在 scenario test 通過前不立」：子批 P 先用設定時的連結檢查（`gyo_expect_links`）守 Module 的連結，原始碼掃描到子批 K（KeepTool 的 scenario）通過後立。
  5. SDL 必須 PRIVATE 連結的設定時檢查：master G3 的名單漏了 text 與 image 兩個（偵察 9 節最後一列）；新的檢查照登記檔的 Plugin target 產生名單，不手寫。
  6. 「必備的後端」：產品選了 `window` 卻沒有任何 Plugin 提供它的實作 → 建置 configure 時的明確錯誤（11-13 runtime-composition §4「同一個選擇欄」）。宣告的形（Plugin 登記檔多一欄「實作了哪個 Module 的後端」，或 Module 列多一欄「要一個後端」）在子批 P 定；同一個事實只在登記檔一處（AGENTS §9）。

## 10. 分批與先後（要涵蓋的第 3 項）

### 10.1 總覽（「代號｜簡述 — 模型／檔位」；代號 `v8-arch-gyo2-sdl-<x>`）

```text
v8-arch-gyo2-sdl（本宣告：統合）── Opus／high；敵對式檢查 Fable／high
├─ P  plugins-base｜plugins/ 與 SDL3 取得、刪 Plugin 的建置測試、必備後端的錯誤 ── Opus／high
├─ W  window-input｜window、input Module＋SDL 的視窗與輸入實作＋有視窗的 runner＋取樣移給 runner＋時間戳換算 ── Opus／high
├─ A  audio-output｜混音器的建立者＋SDL 的音訊實作＋scheduleDelay 重新推導 ── Opus／high（P 之後即可，和 W 並行）
├─ R1 extract｜Kernel 的 RunExtract＋編排層 extractFrom＋准許換來源（不碰 GPU，用假的渲染 World 測） ── Opus／high（P 之後即可，和 W 並行）
├─ R2 render｜render Module（中性介面、Renderer）＋SDL_GPU 實作＋shader 管線 ── Opus／high
├─ K  keep-tool｜ImGui 的 Plugin＋KeepTool＋IME 實機一次 ── Opus／high
├─ T  text｜text Module＋SDL_ttf 實作 ── Opus／high
└─ U  ui｜ui Module（gyo.ui，JSON Module）＋經渲染 World 畫 ── Opus／high
```

先後：**P →（W ∥ A ∥ R1）→ R2（等 W、R1）→ K（等 W、R2）→ T（等 R2）→ U（等 T）**。input 在 render 之前（視窗要先有；KeepTool 兩個都要）；text 在 render 之後（點陣圖要畫得出來才驗得到）；ui 最後（依賴 text、render、input）。K 排在 T 之前：ImGui 有自己的字型點陣化〔知〕，不需要 `text`；K 是本線第一個用到全部基盤的 Feature，越早驗越早發現介面錯。**批次先後最終由主會話照 L17 排**（K 等於 KeepTool 那一批提前併進本線，主會話確認）。

### 10.2 各子批

| 子批 | 範圍 | 上段／下段（navigator） | 實作者／審查者／驗證者（模型／檔位） | 驗收的骨幹 |
|---|---|---|---|---|
| P | 第 9 節 1～6 | 上段：建置圖（Architecture Delta） | Opus／high、Opus／high、Opus／high | 刪 Plugin 的建置測試；L1 三平台 |
| W | `input`、`window` Module（介面、假實作、套用 System）；SDL 的視窗與輸入實作（泵、事件分派、時間戳、文字與組字、像素大小）；有視窗的 runner（Plugin 的單位在 `Configure` 裝，O-3）；取樣移給 runner（world-time 第 3 節與 WT-6 (2) 的文字照交換點宣告 5.5 改；等待零件在哪、WT-2 的範圍一起定）；注入的形（R1） | 上段：公開介面、時間 | Opus／high、Opus／high、Opus／high（時間戳換算的判定與突變清單另送 Fable／high 敵對式檢查，§14.2） | 時間戳的四條保證的測試（假讀取函式、精確等式）；SDL dummy 驅動下的泵與事件；runner 跑 `examples` 的一個視窗程式 |
| A | 第 7 節 | 上段：Design Invariants（audio DI 7）、即時安全 | Opus／high、Opus／high、Opus／high | `OnDestroy` 的先後（裝置先停）；SDL `disk`／`dummy` 驅動；`scheduleDelay` 的量測（判定先宣告） |
| R1 | 第 6 節 1～8 | 上段：Kernel 與編排層的契約 | Opus／high、Opus／high、Opus／high；宣告送 Fable／high 敵對式檢查 | Extract 的唯讀（`Extract<Write<…>>` 編譯失敗照舊）、來源替換後讀新的、`DropOldFirst`、清單順序的啟動錯誤；不需要 GPU |
| R2 | `render` Module、SDL_GPU 實作、shader 管線（HLSL、host 工具、ABI） | 上段：公開介面 | Opus／high、Opus／high、Opus／high | Linux Lavapipe 上的像素比對（master 坑 18：比確切顏色）；macOS Metallib 離線編譯；幀時間的預算先宣告 |
| K | ImGui 的 Plugin（core＋SDL3 平台後端＋SDL_GPU 繪製後端的接法照 Q1）；KeepTool 的登記（`keep_tool`）；組字的畫法（ImGui 不畫組字，master 綁定自己畫） | 上段：使用者指定的 Feature（P14） | Opus／high、Opus／high、Opus／high | tools 線打包；IME 只能實機按一次（CI 只驗文字事件的路徑）——「tools 線不需要實機證據」的現行規則要不要改，交那一批的宣告 |
| T | `text` Module、SDL_ttf 實作（vendored FreeType、HarfBuzz 關，照 master） | 上段：公開介面 | Opus／high、Opus／high、Opus／high | 點陣圖的形狀與 rowPitch；上下顛倒的坑（master 坑 17：在 UV 轉換的單一邊界修） |
| U | `ui` Module（codec 改用 JSON Module、驗證、layout、互動、文字框）、渲染那一半 | 上段：資料契約（Editor↔Runtime） | Opus／high、Opus／high、Opus／high | `gyo.ui` v1／v2 的資料契約測試；文字框的組字 |

- 敵對式檢查（navigator：作者 Opus → 檢查者用別的、不小的模型）：本宣告與 R1、W 的宣告送 Fable／high；其餘子批的宣告送 Fable／high 或 Opus 以外的不小模型，照當時的使用量決定（`subagent-model-explicit`）。
- 檔位：照使用者 11-19「xhigh 只在需要調查時用」，全部 high；W 的時間戳若連續兩次判定不成立（master 坑 8、9 的同類），照 §14 停下回報，不自己升檔。
- ultracode：不用（範圍已在本統合拆開；`v7-ultracode-gearing` 的四項不包含本線）。

## 11. Architecture Delta（AGENTS §3 的七項，整條線寫一次；各子批只寫差分）

1. **Feature**：KeepTool（P14）；之後的工具與 PvP Client；Match 不受影響。
2. **現在的問題**：gyo2 沒有視窗、輸入、畫面、文字、UI、音訊輸出；master 的 platform 把三種角色混在一起，被 P1 退掉；Extract 的執行在第 4 批拿掉。
3. **變的邊界**：新增五個 Module（`input`、`window`、`render`、`text`、`ui`）與 audio 這一側的登記單位；頂層 `plugins/` 的第一批內容（Q1 的選項定位置）；Kernel 的 `RunExtract`；編排層的 `extractFrom` 與一個新的 O-E 碼；`SetExtractSource` 的時機放寬；world_time 的取樣移給 runner。
4. **影響的 Module／Product／Tool**：kernel、orchestration、world_time、audio（只多一個登記單位，DI 2 不動）、建置層（`GyoUnitRegistry`、`GyoBuild`、third_party）；KeepTool（新 tool）。
5. **依賴方向**：新 Module 只依賴 core、ecs、orchestration 與登記檔列的 Module；SDL 只被 SDL 的實作連（Q1 定在哪）；Engine 不依賴 Plugin。
6. **Ownership**：五個 Module 的 owner 是架構（Engine）；SDL 的實作的 owner 依 Q1（A'：各 Module；B：`sdl` Plugin）；KeepTool 的 owner 在 K 定。
7. **為什麼沒有更小的改法**：不帶 platform（P1）就必須把它的功能各自找家；KeepTool 需要視窗、輸入、畫面同時存在；Extract 不做執行，render 就只能回到 master 的「產品自己填佇列」（為舊形狀找位置）。

## 12. Design Invariants 的草案（整條線；各 Module 的 DI 在各子批寫）

- **SDL-1 SDL 只在 SDL 的實作裡**：SDL 的型別與呼叫只出現在 SDL 的實作（位置依 Q1）；Engine 的公開標頭不露 SDL 型別。守衛：設定時的連結檢查（子批 P）；原始碼掃描在 K 的 scenario 通過後立（AGENTS §3）。
- **SDL-2 沒有 SDL 也成立**：刪掉 SDL 的 Plugin（與它在登記檔的列），Engine、所有 Module、headless 的測試仍 configure、build、通過。守衛：子批 P 的刪除建置測試。
- **SDL-3 一條時間線**：輸入事件的時刻、送進混音器的時刻、runner 的取樣都是 `Time::TimePoint`；SDL 的時刻不出 SDL 的實作。守衛：第 8 節的測試；審查。
- **SDL-4 主執行緒**：視窗、事件泵、swapchain 的取得與送出只在建視窗的執行緒；由 runner 安排、SDL 的實作在進入點斷言（`SDL_IsMainThread`），不由 Kernel 檢查（P19）。
- **SDL-5 渲染只讀**：受 DI-KS 13 約束（不另寫一份）。

**Replacement Criteria**（整條線）：
- RC-SDL 1：要第二個非 SDL 的視窗或 GPU 實作時，若某個 Module 的後端介面露出 SDL 才有的概念（換不掉）→ 介面切錯了，重切（master `platform-sdl.md` RC 1、2 的同類）。
- RC-SDL 2：事件泵必須離開主執行緒，而 SDL 的主執行緒規則滿足不了 → 換掉有視窗的 runner 的形（master RC 6）。
- RC-SDL 3：每輪重抽（R6）的費用超過子批 R2 宣告的幀時間預算（計測）→ 改成保留實體對應。
- RC-SDL 4：保持 SDL-3 需要逐事件取樣或睡眠偵測 → 換掉時間戳的換算（master RC 5）。

## 13. 卡片照 L17 判的（不是使用者的話；可以推翻）

| # | 判了什麼 | 理由 |
|---|---|---|
| L-1 | 新 Module 的名字 `input`、`window`、`render`、`text`、`ui` | 命名規範 N7／N8（名字＝概念）；和 master、Bevy 同詞 |
| L-2 | 視窗類 Module `default=no` | 第 3.3 節 |
| L-3 | `window` 依賴 `input`（不是反過來） | Bevy；輸入可以單獨給測試與網路注入 |
| L-4 | 不帶 SDL_Renderer；ImGui 走 SDL_GPU | 第 3.2 節（C2） |
| L-5 | 不帶剪貼簿、檔案對話框、log 轉接、`SDL_main`、`PlatformName`、`native/`、SDL_image、`InputActionMap`、手把 | 第 3 節各列（沒有本線的 Feature 或已有替代） |
| L-6 | 輸入經「輸入服務＋套用點」進 World | 第 4.3 節（交換點宣告 5.6 留給本批選） |
| L-7 | Extract：清單第一張、准許換來源、渲染 World 不帶 world_time、每輪重抽 | 第 6 節 3、4、7、8 |
| L-8 | 裝置開不了不經 `ProductSetup::Report` | 第 7 節 |
| L-9 | audio 那一側的小批維持另做（建議，主會話排） | 第 7 節 |
| L-10 | 子批的先後與 K 提前（建議，主會話排） | 第 10.1 節 |

## 14. 要使用者定的

### Q1：SDL 的 adapter 放哪——你保留的想法（A'）和推導出的拆法（B）並列

**前提**：Engine 的（外部組件怎麼接進 Engine 是建置圖與規則文字的事，作者碰不到）。這一題牽涉 AGENTS §6 的判準和 11-16 的定義怎麼對齊，**規則文字的改法交風紀委員起草、你核准**，本卡不改規則。

**例子**：SDL 是一個函式庫，但裡面有視窗、事件、GPU、音訊、剪貼簿，而且有共用的全域狀態（`SDL_InitSubSystem` 的計數、整個 process 的 hint、只有一個事件佇列、主執行緒規則）。master 的做法是一個 platform 包住視窗與事件，input、GPU 後端經 `native/` 拿 `SDL_Window*`；audio、text 已經是「Module 介面＋SDL 後端」、不經 platform。

| | **A'：你保留的想法**（adapter 放各 Module 底下，獨立 target） | **B：推導出的拆法**（一個 `plugins/sdl`，內含各 Module 的實作 target） |
|---|---|---|
| 目錄 | `engine/modules/window/src/adapters/sdl/`、`input/…`（若 input 要讀 SDL）、`render/src/adapters/sdl_gpu/`、`audio/src/adapters/sdl/`、`text/src/adapters/sdl_ttf/`；Module 本體的 target 不連 SDL，`adapters/sdl` 是另一個 target，只有它連 SDL | `plugins/sdl/`（視窗與輸入、GPU、音訊、ImGui 後端各一個 target）、`plugins/sdl_ttf/`（依賴 `sdl`）；Module 只定介面、不含任何 SDL |
| 系譜 | master 的 audio、text（「Module 介面＋SDL 後端」）；JSON Module 的 `src/adapters/yyjson/`（P5） | Bevy `bevy_window`＋`bevy_winit`（adapter 依賴介面，偵察 8.4）；runtime-composition §4（11-13）的「`sdl` plugin 內含多個後端 target、整個 repo 的 SDL 呼叫只在 `sdl` plugin」；Godot 的介面在 `servers/`、實作在 `platform/<系統>/`（介面與實作分開的部分） |
| AGENTS §6「一個外部組件一個 adapter」 | 要在架構文件把 SDL 數成多個組件（例：視訊＋事件、GPU、音訊各算一個）；但 window 與 input 讀同一個事件佇列、render 要 window 的 `SDL_Window*`，界線切在 SDL 內部的共用狀態上 | 字面成立：SDL 一個 adapter（一個 Plugin，內部分 target） |
| P3「Module 不依賴 Plugin」 | 要讀成「Module 本體不連，Module 目錄下的 adapter target 可以連」（你 11-18 的讀法）；建置層要讓 Module 目錄裡的一個 target 依賴 Plugin，登記檔的「依賴」欄要能表達 target 級的依賴 | 字面成立：`sdl` Plugin 依賴 `window`、`input`、`render`、`audio` |
| P2「Plugin＝第三方官方維護」 | SDL 函式庫本身是 Plugin，我們寫的 adapter 在 Module——和 JSON（yyjson 外部、adapter 在 Module）同形 | `plugins/sdl` 裡有**我們寫的** adapter——要把 Plugin 讀成「接進第三方組件的單位：第三方的程式＋我們包它的 adapter」，同詞異義表第 0 節的定義要補一句 |
| AGENTS §7 刪掉 SDL | 要動每個用到 SDL 的 Module 目錄（`adapters/sdl` 不建），Module 的 CMake 要知道 SDL 選了沒——共通層認得某個 Plugin 的名字（AGENTS §9 要避免的形） | 刪 `plugins/sdl` 與登記檔一列就完成（SDL-2 一個測試守） |
| ImGui 的 SDL 後端（C3） | 要 `SDL_Window*`、`SDL_Event`、`SDL_GPUDevice*`：要從 window、render 的 adapter target 露出帶 SDL 型別的標頭——在 `engine/` 底下，等於 master 的 `native/` 回來；或者自己寫 ImGui 的平台與繪製後端（用我們的介面，數百行×2，組字照 master 自己畫） | 放在 `plugins/sdl` 的一個 target（11-13 的做法），SDL 型別的交接全在 Plugin 內 |
| 後端之間的協調（子系統計數、事件分派、`SDL_Window*` 交接） | 你的想法：靠 SDL 自己（`SDL_InitSubSystem` 計數、`SDL_PeepEvents` 各取自己那段事件類型、`SDL_GetWindowFromID` 換回視窗）——可行；但事件泵只能有一個人做（主執行緒），各 adapter 之間有隱性的先後 | 同一個 Plugin 內協調，一個泵、一處分派 |
| 測試 | Module 的測試要嘛連 SDL dummy、要嘛另做假實作 | Module 的測試用假實作（R2），完全不連 SDL；SDL 的測試只在 Plugin |
| 誰維護 | 各 Module 的 owner | `sdl` Plugin 的 owner（架構） |

**另外兩個退掉的選項**：
- **(C) 一個 `sdl` Module 包住所有 SDL**：等於 platform 換名回來（P1 退掉的形），為舊形狀找位置。
- **(A) adapter 直接寫在 Module 本體的 target 裡、SDL 降成像 yyjson 的外部組件**：Match 選 `window` 就得連 SDL（雖然 Match 不選它）；刪不掉 SDL；ImGui 的問題同 A'；同詞異義表「SDL 家族是 Plugin」要改成「SDL 是外部組件」——推翻 11-16 的歸屬。

**本卡的建議：B。** 理由：(1) 你 11-16 選 Plugin 的理由「SDL這樣的，帶著一堆插件，合到Engine里覺得在自己的系統裡又有一坨子系統」——A' 把 SDL 的 adapter 又分散回 Engine 的五個 Module 目錄；B 讓 Engine 裡完全沒有 SDL；(2) 你的原則「一個公開出口、各實作注入」：B 的 Module 就是出口、Plugin 是注入的實作；A' 的實作編在 Module 目錄裡；(3) AGENTS §6、§7、P3 都字面成立，ImGui 的 SDL 型別不必回到 `engine/`；(4) Bevy 同形。**B 的代價**：P2 的 Plugin 定義要補一句（Plugin 裡可以有我們寫的 adapter）；每個 Module 要有一個注入用的後端介面（R1，子批 W 定一次的形）。**你的想法裡留下的**：不再有 platform（P1）、不要 `native/`、Module 介面＋後端的概念、`adapters` 的名字（B 的 Plugin 內部目錄也用 `src/adapters/`？——Plugin 本身就是 adapter，內部照 target 分目錄即可，子批 P 照命名規範定）。

**選 A' 的話，本線要改的**：建置層支援 target 級的 Plugin 依賴（子批 P 擴大）；架構文件寫 SDL 數成幾個組件；ImGui 的接法要你再選一次（露 SDL 型別的標頭，或自己寫 ImGui 後端）；SDL-1 的守衛改成「SDL 只在各 Module 的 `adapters/sdl*`」。

**選 B 的話**：同詞異義表第 0 節的 Plugin 定義補一句（例：「Plugin＝接進第三方官方維護之組件的單位，含我們包它的 adapter；介面是 Engine 的」），批次表 SDL 列的「SDL 放 `src/adapters/sdl/`」改成 `plugins/sdl/`；11-20 的「Module 碰外部組件放 `src/adapters/<組件>/`」照舊適用（JSON、asset 的 loader），只是 Module 不再碰 SDL。規則文字由風紀委員起草、你看過再改（AGENTS 本體可能不必改：§6 已寫「adapter を Module に置くか Plugin に置くかは、上の判断の一文で決める」）。

### Q2：ufbx 歸哪一類

**前提**：Engine 的（同 Q1）。**例子**：model 回來時要讀 FBX。ufbx 是單檔的 C 函式庫，master 用 `UFBX_NO_STDIO`（不自己開檔、一律經資產讀進來），輸出轉成我們的 model 型別，沒有全域狀態、沒有執行緒、不露型別。

| | **Module 側**（model Module 的 `src/adapters/ufbx/`）← 建議 | **Plugin**（`plugins/ufbx/`） |
|---|---|---|
| 和既有的對照 | 同 JSON（yyjson、fast_float）；同 asset 已核准的「第三方 loader 放擁有型別的 Module 的 `src/adapters/`」（P20） | 11-13 runtime-composition §4 把 ufbx 列在第 1 類 plugin（v2 之前的文件） |
| AGENTS §6 判準一句 | 介面是我們的、ufbx 完全藏在後面 → Module | 要讀成「第三方的就是 Plugin」，和 JSON 的判法不一致 |

**建議 Module 側**：和 Q1 不同的地方在於 ufbx 沒有 SDL 的那些全域狀態、主執行緒規則、也沒有像 ImGui 這種要拿它原始型別的第三方——它就是 yyjson 的同類。本線不實作（model 不在本線），只定歸類，寫進同詞異義表第 0 節的「還沒確認」那一行。

## 15. 範囲の限界

- 行動平台（iOS、Android）的入口不支援（不帶 `SDL_main`）。
- IME 只能實機驗（CI 只驗文字事件的路徑）；Windows 的組字（`SDL_HINT_IME_IMPLEMENTED_UI`，整個 process、只讀一次，master 坑 15）在子批 W／K 實機驗。
- GPU 的 CI 證據只有 Linux Lavapipe（software Vulkan）；Windows D3D12 不在 CI；macOS 只做 Metallib 離線編譯（master 坑 27）。
- macOS `nextDrawable` 的約 1 秒停頓與縮放空窗（master 坑 1、2）在薄起步照舊存在（live frame 與分執行緒在 PvP Client 移植）。
- 時間戳的已知限制（第 8 節）。
- master 的 `platform_sdl_concurrency` 在 CI 逾時的原因沒查明（坑 13）；Linux 上 SDL 的 dummy 視訊驅動會開 evdev（容器沒有權限）——子批 W 的 CI 照 master 的設定，出現時照 §14.1 切分。
- 「替換成另一個實作」在第二個實作出現前只是預留（R1）。

## 16. 和其他卡的協調（經主會話）

- **asset（PR #283）**：音效 loader 與 clip 來源在 audio 那一側的小批（第 7 節）；圖片 loader 是 stb_image，SDL 不碰。
- **網路 Module（PR #275）**：輸入的時刻給網路的預測用；事件時刻對到 tick 是網路那一側的事。
- **圖書管理員**：「送進 World 外服務的時刻都是時間基準」登記成 CCR（第 8 節末）。
- **風紀委員**：Q1／Q2 的規則文字（同詞異義表第 0 節、AGENTS §6 是否需要改）。
- **KeepTool**：子批 K 等於 `-keeptool` 那一批，主會話確認是否併進本線。
