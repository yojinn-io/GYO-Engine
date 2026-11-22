# 事前宣告：SDL（第一個 Plugin 批）與 platform 的拆法（`v8-arch-gyo2-sdl`）

更新：2026-11-23（修訂 1）。Owner：架構（GYO2 主會話開的任務卡 `gyo2-sdl-declaration`，`local_2cc7e494`）。**狀態：草稿，等使用者的動工前核准。這張卡只寫宣告與分批計畫，不寫實作。**

**經過**：
- 修訂 0（c9eae0a0）送 Fable／high 敵對式檢查：blocker 3、should 12、nit 9（主 checkout `build/evidence/gyo2-sdl-declaration/declaration-r0-adversarial.md`）。全部採用，處理對照在第 17 節。最大的改動：Q1 的規則比較重寫（四個選項各撞 AGENTS §6 的哪一句）；B 的登記形與建置代價寫進統合；「Extract 是唯一同步點」拿掉（和 O-13 (3) 相反）；後端的注入形留在統合（第 9.2 節）；ImGui 的接法改成交使用者（Q3）；R2 拆兩段讓 KeepTool 早一點。
- 同日主會話轉來三件，一併寫進修訂 1：asset 宣告第 15 節第 1 項（image Module 與 stb adapter，第 3.2 節、子批 I、Q2）、第 8 項（Kernel 的 `Summary` 雜湊訊息序號，第 6.3 節）；使用者在主會話定的 2D／3D 形狀（HANDOFF `v8-arch-gyo2-2d-and-3d`，第 6.4 節、子批 X、R2、Q5）。

**性質**：批次表第 6 組「SDL（第一個 Plugin 批）」那一列的事前宣告＋分批計畫（HANDOFF `v8-arch-gyo2-sdl-declaration-start`，使用者 2026-11-23「開 SDL 的宣告卡」）。範圍大，照「先立統合、再分子份」：本頁是**統合**——定邊界、跨子批的介面（注入的形、登記形、條號的分配）、關聯表、要使用者定的事、各子批的先後與「模型／檔位」。各子批開工前各自再寫一份小宣告。動到 Engine 的公開介面、建置圖、Kernel 的 Extract、時間的取樣、Transform，照 AGENTS §2 當**基盤**處理（第 4 節）。

**關於 Q1 的選項 A'**：A' 是使用者 2026-11-18 保留的想法；使用者當時說「不想放到正式文件裡」。本卡照主會話的指示把它和推導的拆法並列（第 14 節）。使用者若不要它留在文件，核准時說一聲，這一欄刪掉、改記在 HANDOFF 的四樣裡。

**用詞**：「**卡片照 L17 判**」＝這張卡照命名規範、別家原始碼、已定的原則自己判的，**不是使用者的話**。別家的事實標出處；**〔知〕**＝憑一般知識、這次沒用原始碼查證（SDL、ImGui 的原始碼本機沒有），不當決勝的根據。「Engine 的／作者的」是每一項的前提：只有 Engine 做得到的（平台差異、跨 Module 的共用機制）是 Engine 的；作者用現有機制能做、而且做法本身是遊戲規則的，是作者的，Engine 只寫契約。

**核心一句**（每一項拿來對照）：**Engine 給視窗、輸入、畫面、文字、聲音輸出各一個公開出口（介面是我們的、用我們的詞），SDL 從外面注入成這些出口的實作；什麼時候開窗、按鍵對應什麼動作、畫什麼、UI 長什麼樣、聲音什麼時候響，是作者的。**

---

## 0. 根據與讀的版本

- 程式：`origin/gyo2` 1d4a8d7a（PR #283 asset 合併之後）。`engine/core/time/include/engine/time/MonotonicClock.hpp`；`engine/ecs/kernel` 的 Extract 宣告那一半（`Params.hpp:1318-1340`、`Registry.hpp:673-676`、`Declarations.hpp:90-91`；`Registry::SetExtractSource` 存的是裸指標，`Registry.cpp:673`；**執行那一半隨第 4 批拿掉**，參數建構子會斷言 `Params.hpp:1325`）；`engine/core/math`（`Vec3`、`Quaternion`、`Matrix4`；沒有 Transform 元件）；Kernel 沒有父子關係（`ChildOf` 一類）；`build/cmake/GyoUnitRegistry.cmake`（Plugin 可以 `library_only`；`library_only` 的依賴不必由產品帶，`:236-242`；Plugin 不能 `default=yes`，`:120`；**Module 依賴 Plugin 是 FATAL_ERROR**，`:164-165`；有登記單位的列必須 `library_only=no`，`:105-110`）；`GyoBuild.cmake`（`third_party/` 在登記檔載入之前加入，`:34` 先於 `:55`；**登記檔裡每一個 Plugin 都 `add_subdirectory(plugins/<名>)`，不看產品選了沒**，`:59-66`）；`engine/orchestration/src/Product.cpp`（Plugin 的 `Configure` 在 Module 之後，`:215-218`；銷毀的先後 World → `OnDestroy` 反序 → runner → 共用處，`:75-104`）；`config/plugins.csv`（只有表頭）；頂層沒有 `plugins/`。
- 文件：批次表（文件分支 `origin/claude/gyo2-time-design`）SDL 列、audio 那一側的小批、「不擋路的小事」；HANDOFF（副本：主 checkout `build/evidence/gyo2-sdl-declaration/HANDOFF-d315368c.md`）的 `-platform-obsolete-split-into-modules`、`-os-in-core-platform-dirs`、`-json-adapters-dir`（含 11-20 兩次推廣）、`-plan-gaps-2026-11-21`、`-audio-review-approved`、`-world-audio-withdrawn`、`-infrastructure-before-products`、`-module-plugin-game-tool`、`-module-plugin-owner-definition`、`-module-plugin-registers`、`-module-deps-in-registers`、`-products-pick-modules`、`-keeptool`、`-thread-binding-by-runner`、`-asset-adapter-world-asset`；主會話 2026-11-23 轉來的 `-2d-and-3d`。
- 同詞異義表 `rebuild-vocabulary.zh-Hant.md` 第 0 節末；AGENTS §2、§3、§4、§6、§14。
- `audio.zh-Hant.md` 第 4 節、第 5.2 節、DI 2、DI 3、DI 7。asset 宣告 `asset-declaration.zh-Hant.md` 5.4、5.7、5.9.1、5.11（stb_image）、第 15 節第 1、8 項。
- 編排層 O-3、O-4、O-6、O-8、O-11～O-15、RC-O13；交換點宣告 5.3～5.6；`world-time.zh-Hant.md` 第 3 節、WT-2、WT-6；時間設計 5.5（二）（三）、第 9 節 (a)；Kernel 頁 DI-KS 13、14、15、26。
- `target/runtime-composition.zh-Hant.md` §2（三個 World 的組法，使用者 11-11）、§3b（Editor 預覽、ImGui 拆兩半，使用者 11-11）、§4（plugins 層、SDL 家族、兩類 plugin，使用者 11-11～11-13）。**v2 之前的文件**（使用者 11-16「後面統一處理」）：當系譜與使用者當時的決定，和 11-16 的定義衝突處以 v2 為準，但**仍是使用者的話**，推翻要交使用者。
- master（只看理由與坑）：偵察報告 `build/evidence/gyo2-sdl-declaration/master-platform-survey.md`（基準 origin/master 3b411e21）。
- 別家：偵察報告 `build/evidence/gyo2-sdl-declaration/reference-engines-platform.md`（Bevy 本機 6a911653；Godot 以 WebFetch 查 master；Unity 以官方文件與〔知〕）。主會話另查：Bevy `bevy_transform/.../transform.rs:87`（z 用來排序）、`bevy_camera/src/components.rs:13`（`Camera2d` 預設正交）、`bevy_core_pipeline/src/core_2d`、`core_3d`；Unity Entities `Unity.Transforms/LocalTransform.cs`、`ParentSystem.cs`（本機 `~/Code/GitHome/com.unity.entities`）。
- 證據都在**主 checkout** `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-sdl-declaration/`（本機 Intel Mac，git 不追蹤）；偵察者 Opus／high、檢查者 Fable／high，都只讀。

## 1. 先用遊戲講

**KeepTool 開起來**（使用者 11-14：「需要保障基礎的ImGUI沒問題，輸入法也能用」）：程式的 `main` 建一個 `Product`，選了視窗、輸入、畫面的 Module 與 SDL 的實作。SDL 視窗的單位在 `Configure` 裝上「有視窗的 runner」。runner 每一輪在主執行緒上：處理 OS 事件 → 取一次時間 → `RunFrame`（World 依序跑，畫面的 World 最後經 Extract 讀前面的 World，把畫面交給 GPU）→ 等垂直同步。玩家在 ImGui 的文字框用日文輸入法打字：組字中的字要畫出來、按 Enter 確定時不能把 Enter 漏給程式（master 為此對 SDL 打過補丁）。

**FPS 的 Client 開槍**：滑鼠按下的時刻是 OS 記的（SDL 給的是 `SDL_GetTicksNS` 時間軸上的 ns）。這個時刻在 SDL 的實作裡換成 Engine 的時間基準，之後所有人只看到 `Time::TimePoint`。混音器拿它排起播（audio DI 3）；網路的預測拿它判斷「這一下屬於第幾 tick」是作者（網路 Module）的事。

**讀檔換掉模擬的 World**（O-8）：畫面的 World 不跟著重建（GPU 的資源、pipeline 很貴），之後改讀新的模擬 World（O-12 Extract 第 4 點，做法在子批 R1 比）。

**一個 2D 遊戲**（使用者 11-23：「我可以移植一個2D遊戲進來」）：角色是帶 `Transform` 的實體，z 決定誰畫在前面；相機是正交投影；畫面走 2D 管線，和 3D 共用裝置、資源、Extract、合成。

**Match（沒畫面的伺服器）**：不選 SDL、也不選視窗、輸入、畫面的 Module；建得起來、跑得起來，SDL3 不被抓、不被建。

## 2. 前提表

標「使用者」的是使用者的話；標「L17」的是主會話或卡片自己判的，可以被推翻。

| # | 前提 | 誰定的（出處） | 掛在它上面的決定 |
|---|---|---|---|
| P1 | master 的 `engine/platform` 不帶回，功能拆進各 Module | 使用者 2026-11-19（`-platform-obsolete-split-into-modules`） | 第 3 節的拆法全部 |
| P2 | Module＝我們維護、介面是我們的（外部函式庫完全藏在我們的介面後面也算）；Plugin＝第三方官方維護、接進來用的；SDL 家族與 ImGui 是 Plugin | 使用者 2026-11-16（`-module-plugin-owner-definition`；同詞異義表第 0 節末；AGENTS §6 的判斷句）。**未確認**：SDL 的 adapter 歸哪、ufbx 歸哪 | Q1、Q2 |
| P3 | Plugin 可以依賴 Module，Module 不能依賴 Plugin；Module 先登記 | 使用者 2026-11-14（`-module-plugin-registers` 第 (5) 點）；建置層 FATAL_ERROR | Q1；Plugin 的 `Configure` 在所有 Module 之後（第 9.2 節） |
| P4 | 外部組件只在它的 adapter 裡；一個外部組件一個 adapter；adapter 的介面用使用方的詞；「何を一つの外部 Component と数えるか」寫在架構文件；Plugin は Engine から参照されない、Engine の外に置く | AGENTS §6；使用者 2026-11-13（`v8-arch-external-components-as-adapters`） | Q1 各選項撞哪一句 |
| P5 | Module 裡碰外部組件的程式放 `src/adapters/<組件>/`；主會話回報「SDL 那一批也沿用 `adapters/sdl`」，使用者「good，我剛想和你說其他的adapter也這樣的。」 | 使用者 2026-11-20（`-json-adapters-dir` 與兩次推廣） | Q1 的 A、A'；批次表 SDL 列「SDL 放 `src/adapters/sdl/`」 |
| P6 | `core/os` 用 `src/<平台>/`；`src/adapters/<組件>/` 是 Module 的結構；視窗、輸入、聲音裝置不進 core/os | 使用者 2026-11-21（`-os-in-core-platform-dirs`） | 本線不改 core/os 的目錄；SDL 的東西不進 core |
| P7 | 模擬的 World 與渲染的 World 分開；渲染的 World 有自己的 Registry；只經 Extract 以 const 讀來源；不寫回 | 使用者 2026-11-11（K.3 第 6 項、DI-KS 13、runtime-composition §2、§3b） | 第 6 節；子批 R1、R2 |
| P8 | 資料進出 World 的四種形，同一件事不兩種形；Extract 的契約 1～6 | 使用者 2026-11-22 合併前核准（O-12；交換點宣告 5.4） | 第 6 節 |
| P9 | Product 一份的共用處；兩個單位 `Create` 同一型別是 Programmer Error；`OnDestroy` 照登記的反序、在所有 World 之後、共用處之前；產品或測試給單位的物件走 `ProductDescription::options` | 使用者 2026-11-22（O-2、O-15） | 第 9.2 節的注入形；第 7 節 |
| P10 | audio 對輸出後端的要求（每 block 一次 anchor、一條執行緒呼叫 `Render`、只持參照、`OnDestroy` 先停、錯誤碼是後端自己的） | 使用者 2026-11-22 動工前、2026-11-23 合併前核准（audio 頁第 4 節） | 第 7 節 |
| P11 | 輸入事件的到達時刻在平台邊界換成時間基準，歸 SDL 那批；送進混音器的時刻是時間基準 | 批次表 audio 那一側小批那一列（「輸入事件的到達時刻歸 SDL 那批」）；audio DI 3；`MonotonicClock.hpp` 註解（這一條的判定是 `-world-audio-withdrawn` 時 subagent 的前提，使用者的「照建議」回的是主會話的三件） | 第 8 節 |
| P12 | World 時間的取樣移給 runner，到「第一個需要 runner 取樣的 runner 那批」；取樣、等待零件、渲染 World 的時間、WT-2 的範圍那一批一起定 | 使用者 2026-11-22 合併前核准（交換點宣告 5.5、world-time 第 3 節、WT-6 (2)） | 子批 W |
| P13 | 先建基礎設施、第一個搬回的產品不定 | 使用者 2026-11-19（`-infrastructure-before-products`） | 本線不搬 PvP；驗收用 KeepTool、examples、測試 |
| P14 | KeepTool：基礎 ImGui 沒問題、輸入法能用；交給 GYO2 會話排成一批 | 使用者 2026-11-14（`-keeptool`） | Feature 錨；子批 K |
| P15 | 產品可以只選部分 Module | 使用者 2026-11-17（`-products-pick-modules`） | Match 不選視窗類 Module |
| P16 | 進 World 的 Module 預設全帶、產品可關 | **L17**（主會話，編排層骨架宣告；「預設全帶可關／預設不帶要列」那一層使用者沒有回，`-products-pick-modules`） | 第 3.3 節：要後端的 Module 本卡照 L17 判成 `default=no` |
| P17 | ui 留 Engine（原文「目前；各遷移批確認」） | 使用者 2026-11-13（kernel HANDOFF `v8-arch-adapter-plugins-review`，`docs/architecture/plans/kernel/HANDOFF.md:92`） | 子批 U。本線就是那個遷移批：照 11-16 的判斷句確認——`gyo.ui` 我們維護、介面是我們的 → Module |
| P18 | Engine 不給標準 `main` | 使用者 2026-11-21（O-3 退出流程） | `SDL_main`／`GyoMain` 不帶 |
| P19 | runner 決定哪條執行緒跑哪個 World；需要綁執行緒的 Resource 自己記、碰錯斷言 | 使用者 2026-11-16（`-thread-binding-by-runner`）；DI-KS 14 | SDL 的主執行緒規則落在 runner 與後端 |
| P20 | 圖片解碼用 stb_image、不走 SDL，只在一個 adapter；位置 `engine/modules/image/src/adapters/stb_image/`；image Module 隨 SDL 那批的第一個使用者做 | 使用者 2026-11-21（`-asset-three-decisions`）；asset 宣告 5.11、第 3 節（使用者動工前核准 55a14825） | 子批 I；Q2 |
| P21 | 使用者保留的想法（**不是決定**）：SDL 照舊是 Plugin；我們寫的 adapter 放在各 Module 底下當後端；只留「Module 介面＋後端」一個概念；不要 `native/`，後端之間靠 SDL 自己協調（子系統計數、`SDL_PeepEvents` 各取自己那段、`SDL_GetWindowFromID` 換回視窗）；名字用 `adapters` | 使用者 2026-11-18（主會話記憶；「不再有 platform」11-19 已成 P1；名字照 P5） | Q1 的選項 A' |
| P22 | SDL 是一個 `sdl` plugin、內含多個後端 target；ImGui 官方的 SDL3 平台與 SDL_GPU 繪製後端放在 `sdl` plugin 內的獨立 target；整個 repo 的 SDL 呼叫只在 `sdl` plugin；ImGui、SDL 的全域 context 明列例外；產品用到某能力卻沒選它的後端、或選了兩個，是建置 configure 時的明確錯誤 | 使用者 2026-11-11～11-13（runtime-composition §4，`v8-arch-adapter-plugins-review`；v2 之前） | Q1 的選項 B 的系譜；Q3、Q4 |
| P23 | ImGui 拆兩半：輸入與 widget 邏輯是文件 World 的 System，draw list 的 GPU 繪製是渲染 World 的 System；draw list 經 Extract 進渲染 World；一個函式從 NewFrame 到 Present 消失 | 使用者 2026-11-11（runtime-composition §3b） | 子批 K；Q3 |
| P24 | GYO 要支援 2D 遊戲（使用者會移植一個 2D 遊戲）；Transform 只有一套、用 3D 的，2D 也用、z 拿來排序；render 照 Bevy：底層（裝置、資源、Extract、合成）共用，上層分 2D、3D 兩條管線，相機用同一套投影機制；物理 2D、3D 分兩個 Module，不在這批 | 使用者 2026-11-23（主會話，`v8-arch-gyo2-2d-and-3d`）：「「要」並且我可以移植一個2D遊戲進來。我的意思是：Transform 照建議一套 3D，render 照 Bevy 分管線，物理 2D、3D 分成兩個 Module。」 | 第 6.4 節；子批 X、R2；Q5 |

**卡片自加的前提**（不是使用者的話）：
- C1：SDL 3.4 的視窗、事件泵、swapchain 的取得與送出只能在建立視窗的那條執行緒（master 偵察 2 節末，`SDL_gpu.h:4066-4094`、`:4202-4218`，master D27）。
- C2：一個視窗只能被 SDL_GPU 或 SDL_Renderer 其中之一使用（master 偵察 4 節）。
- C3：ImGui 官方的 SDL3 平台後端吃 `SDL_Window*`／`SDL_Event`、SDL_GPU 繪製後端吃 `SDL_GPUDevice*`；SDL 沒有從視窗查回 GPU device 的 API〔知〕。

## 3. platform 的每一項功能拆到哪裡（要涵蓋的第 1 項）

### 3.1 master 的 `engine/platform/sdl`

master 的 platform 同時扮三個角色（偵察 9 節末）：(a) 視窗與主執行緒事件泵的擁有者、(b) process 服務（入口、執行檔目錄、log 轉接、剪貼簿、檔案對話框）、(c) SDL 時間換到 Engine 時間的唯一位置。下表的「SDL 的實作」指 Q1 選定的位置。

| master 的功能 | 去處 | 前提 | 理由 |
|---|---|---|---|
| 視窗建立、查詢（視窗、螢幕）、操作、視窗事件 | **新 Module `window`**（介面）＋SDL 的實作 | Engine：平台差異 | Bevy `bevy_window`／`bevy_winit`；Godot `DisplayServer` |
| 事件泵（主執行緒、一輪處理完才丟例外） | 有視窗的 runner 呼叫、SDL 的實作裡做；事件轉成 `window`、`input` 的資料（Q3 選 (b) 時 ImGui 也從 `input` 拿，不另拿 `SDL_Event`） | Engine：SDL 的主執行緒規則（C1） | Bevy `WinitPlugin::set_runner`；master 坑 4（提早跳出會截斷下一次泵） |
| 像素大小與密度（DPI） | `window` 的查詢一開始就分「邏輯大小」與「像素大小」 | Engine | master 從沒做（坑 22）；macOS Retina、Windows 縮放上 KeepTool 就會碰到（ImGui 的 framebuffer scale）。只做查詢，不做全螢幕模式 |
| live frame（OS 的 modal 迴圈中畫圖） | **不在薄起步**；介面留一個回呼點（R3），PvP Client 移植時做 | Engine，沒有本線的 Feature | master 坑 1～3 |
| 剪貼簿 | **不帶**；`gyo.ui` 的文字框或 ImGui（Q3 (b)）要用時加到 `window` | 有 Feature 時是 Engine 的 | Bevy 是獨立 crate（arboard），Godot 在 DisplayServer |
| 檔案對話框 | **不帶**；工具回來時在工具那一批加 | Engine 的（平台差異），現在沒有 Feature | master 只有三個工具用；SDL 的回呼在任何執行緒、可能回兩次——那時照 master 的 process 級登錄表做 |
| log 轉接 `SdlLogBridge` | **不帶**；有點名需要時在 SDL 的實作裡接 logging，一個地方 | 那時是 Engine 的 | master 只有 PvP 用；兩個坑（解構順序的遞迴卡死、測試握手） |
| 程式入口 `GyoMain`／`SDL_main` | **不帶** | — | P18；桌面三平台不需要；Windows 的 UTF-8 命令列由 os 的 manifest 處理；Windows GUI 子系統用連結選項保留 `main`（子批 K 驗）；行動平台列範圍的限界 |
| 執行檔目錄 `BaseDirectory` | 需要時加到 `core/os`，**不經 SDL** | Engine：每個產品都要、不需第三方（P6） | 和 `UserDataDirectory` 同處；有使用者時才加 |
| `PlatformName` | 不帶 | — | 只供診斷 |
| 時鐘配對與時間戳換算 | SDL 的輸入實作裡（第 8 節） | Engine | P11 |
| `native/`（帶 SDL 型別的 API） | **不帶** | — | P4、P21、P22；SDL 型別的交接方式依 Q1、Q3 |

### 3.2 master 的 `input`、`render`、`text`、`ui`、audio 的 SDL 後端，與 asset 交來的 image

| master | 去處 | 前提 | 說明 |
|---|---|---|---|
| `GYO::Input`（「狀態＋依序事件」雙軌、文字與組字、失焦清鍵） | **新 Module `input`**（不含 SDL） | Engine：中性資料模型 | 雙軌的理由（同幀按下又放開、事件先後）照 master；`text` 與 `events` 不保證順序照 master。**放 Resource、不用 `Message`**（第 6.3 節） |
| 文字輸入的開關與輸入法候選窗的位置 | `input` 的操作（由 SDL 的實作在主執行緒做） | Engine | `gyo.ui` 的文字框與 ImGui（Q3 (b)）都要；master 每次從 SDL 讀「開沒開」（坑：ImGui 也會開關） |
| `InputActionMap`（按鍵→動作） | **不在薄起步** | 作者（遊戲規則）；有共用需求時再看 | 別家分歧：Bevy 本體沒有、Godot 有 `InputMap`、Unity 有 Actions。PvP 移植時用它的需求再判 |
| 手把 | 不帶 | — | master 沒有程式；沒有點名的 Feature |
| `GYO::Render`（中性 device 介面、`Renderer`、固定 pass、shader ABI `gyo.raster.v1`、讀回） | **新 Module `render`**＋SDL_GPU 的實作＋渲染的 World 與 Extract＋2D／3D 分管線（第 6 節） | Engine | master 沒有 extract、產品每幀自己填 `RenderQueue`——這是本線要換的形 |
| `GYO::RenderBackendSDL`（SDL_Renderer 2D，給 ImGui 工具） | **不帶** | — | 使用者 11-11 §3b（預覽是 draw list 引用 render target 的貼圖、draw list 在渲染 World 畫）與 11-13 §4（ImGui 的繪製後端是 SDL_GPU）的後果，**不是卡片的判斷**；C2 讓 master 的工具繞離屏 |
| shader 管線（共用 HLSL → host 工具 DXC＋SDL_shadercross＋SPIRV-Cross，釘版本與 SHA256） | 子批 R2a 帶回最小的一份；Engine 留 HLSL 原始碼與 ABI，編譯工具在 SDL 的 GPU 那一側 | Engine | P22 |
| `GYO::Text`（中性）＋`TextBackendSDLTTF` | **新 Module `text`**（中性，`library_only`）＋SDL_ttf 的實作 | Engine | master 已是「介面＋後端」、不經 platform |
| `GYO::Ui`（`gyo.ui`：codec、驗證、layout、互動、文字框） | **新 Module `ui`**（不含 SDL、不依賴 render） | Engine（P17） | master 的 `GYO::Ui` 不依賴 Render、Text、Input；Bevy `bevy_ui` 不依賴 `bevy_render`。JSON 改走 JSON Module |
| `UiRenderer` | **新 Module `ui_render`**（依賴 `ui`、`render`、`text`） | Engine | 照 Bevy `bevy_ui_render`、master 的分法；只用 `gyo.ui` 資料的工具與測試不必帶畫面 |
| `GYO::AudioBackendSDL` | SDL 的音訊實作（第 7 節） | Engine（P10） | |
| `GYO::AssetSdlImage` | **不帶**；改由 **image Module**（stb_image adapter） | Engine | P20 |
| image Module（asset 宣告 5.11 定好位置、授權、adapter） | **子批 I**；第一個使用者是 R2b 的貼圖（之後 `ui_render` 的圖片） | Engine | asset 宣告第 15 節第 1 項；stb 的下載照 asset 宣告第 13 節，在 I 問使用者 |
| ufbx（model 的匯入器） | model 回來時，model Module 的 `src/adapters/ufbx/`（Q2） | Engine | 不在本線 |

### 3.3 新 Module 的清單（名字照命名規範 N7、N8、N20、N21，卡片照 L17 判）

| Module | 目錄 | 進 World（`library_only=no`） | `default` | 依賴（登記檔） | 子批 |
|---|---|---|---|---|---|
| `transform` | `engine/modules/transform` | 是（`Transform`、`GlobalTransform`、傳播的 System） | yes | — | X |
| `input` | `engine/modules/input` | 是（輸入的 Resource、套用 System） | no | — | W |
| `window` | `engine/modules/window` | 是（視窗的 Resource） | no | input | W |
| `render` | `engine/modules/render` | 是（渲染 World 裡的 System） | no | transform | R2a、R2b |
| `image` | `engine/modules/image` | 照 asset 宣告 5.4（loader 函式庫＋World 端，子批 I 定） | no | asset | I |
| `text` | `engine/modules/text` | 否 | no | — | T |
| `ui` | `engine/modules/ui` | 是 | no | json、input | U |
| `ui_render` | `engine/modules/ui_render` | 是 | no | ui、render、text | U |

- **`default=no`**（卡片照 L17 判，和 P16 不同）：這些 Module 沒有後端就做不了事；預設帶會讓 Match 與 headless 測試在啟動時就報缺後端。Godot 的 headless 是明確選 dummy〔知〕。`transform` 不要後端，照 P16 預設帶。被推翻的話改成「預設帶＋沒有後端時用空實作」，代價是忘了選 SDL 時不報錯。
- **`window` 依賴 `input`**（L17）：照 Bevy（`bevy_window` 依賴 `bevy_input`）；`input` 不認得視窗，測試與網路的輸入注入只用它。
- **`render` 不依賴 `window`**（L17）：畫到哪個視窗由 SDL 的實作接起來（GPU 的實作依賴視窗的實作），離屏與 headless 的測試不必帶 `window`、`input`。`render` 依賴 `transform`：渲染 World 經 Extract 讀 `GlobalTransform`。
- `text` 是 `library_only`，產品不在 `MODULES` 列它；它的後端照第 9.2 節的通則在共用處找。

## 4. 基盤 Architecture 的根據（AGENTS §2）

**為什麼是基盤**：`window`、`input`、`render`、`transform` 的介面上面會積產品（KeepTool、四個工具、PvP Client、2D 遊戲）、ImGui 的接法、`gyo.ui`、網路複製與回滾的登記；Extract 的執行與來源替換是 Kernel 與編排層的契約；Plugin 的目錄與登記是建置圖；有視窗的 runner 決定時間的取樣。

### 4.1 予見的根據

| 根據 | 種類（§2） | 導出的基盤要求 |
|---|---|---|
| KeepTool（P14） | 1 名指的 Feature | 視窗、鍵盤滑鼠、文字輸入與組字、ImGui 畫在 SDL_GPU 上、tools 線能打包；顯示日文要 CJK 字型；刪掉 KeepTool 回到零工具 |
| 2D 遊戲的移植（P24） | 1 名指的 Feature（遊戲還沒點名，Q5） | Transform 一套（z 排序）、正交相機、2D 管線；細節推不出來的列 Q5 |
| PvP Client（master 的完整遊戲，P13 的對照） | 1 計畫上的 Feature | 輸入帶 OS 時刻；3D 場景＋疊層＋`gyo.ui`；模擬在自己的執行緒、主執行緒＝事件＋畫面（O-13、時間設計 5.5（三）、runtime-composition §2 的三個 World）；live resize；讀檔換模擬 World 時畫面不重建；VFX（半透明、billboard、instancing，主會話轉來的「不要堵死」） |
| 四個工具（master） | 1（之後回來） | ImGui、3D 預覽（draw list 引用 render target 的貼圖，§3b）、音效預覽、檔案對話框（工具那批） |
| Match | 1 | 不選 SDL 也建得起、跑得起；SDL3 不被抓、不被建 |
| 一般的單機遊戲 | 1（GYO2 的目標：通用引擎） | 薄起步只有單執行緒、渲染 World 在主執行緒 |
| Bevy、Godot、Unity、master | 2 成熟的參考實作 | 第 4.3 節 |
| 規模與預算 | 3 | 幀時間、讀入時間的預算**這裡沒有出處**，由 R2a、R2b、K 的宣告各自寫、照 §14.1 先宣告並交使用者；音訊 `scheduleDelay` 的 master 量測（macOS 54.8 ms、Windows 44.4 ms，`2026_10_30`、`2026_10_31` 的 dev_log）只當比較基準，子批 A 重新推導 |

### 4.2 予見的要求寫成邊界（實作薄起步，其餘只定形）

| # | 要求 | 本線實作 | 只定形，留給點名的 Feature |
|---|---|---|---|
| R1 | 一個公開出口、實作注入 | 第 9.2 節的注入形，`window`、`input`、`render`、`text`、音訊輸出都照它 | 第二個實作出現前，「替換成另一個組件」只是預留 |
| R2 | Module 的行為測試不靠 SDL | 每個介面有測試用的假實作 | SDL 的實作自己的測試照 master（`SDL_VIDEODRIVER=dummy`、Lavapipe） |
| R3 | live frame | 介面留「OS modal 迴圈中要一幀」的回呼點（宣告形狀，不實作） | PvP Client 移植 |
| R5 | 模擬在別的執行緒 | 薄起步單執行緒 | O-13 的入口在 PvP Client 移植那批 |
| R6 | 渲染 World 的實體與來源對應 | 薄起步每輪重抽（不保留對應） | 量到超過預算時改成保留（Bevy 0.15 起的 `RenderEntity`，偵察 3.6），RC-SDL 3 |
| R7 | VFX 要的繪圖能力 | 疊層與 UI 本來就要的半透明混合（排在不透明之後）；pipeline 的狀態帶混合模式 | 3D 的半透明排序相位、billboard（面向相機的四邊形，在 vertex shader 做）、instancing（每個實例一份資料的緩衝）：R2b 的介面不堵死——送出的單位能帶實例數與實例緩衝、shader ABI 留欄位、相位能排序 |
| R8 | Transform 的父子階層 | `Transform` 的意思從一開始就是「相對父的局部姿態，沒有父時就是全域」；薄起步只有沒有父的同步 | 父子（`Parent`＋傳播）的形在子批 X 定，實作等點名的 Feature（第 6.4 節） |

（修訂 0 的 R4「多個視窗、介面以視窗 id 為鍵」拿掉：沒有點名的 Feature。）

### 4.3 系譜、退掉的選項、需要它的 Feature（AGENTS §4）

| 形 | 系譜 | 退掉的選項與理由 | 需要它的 Feature／Product |
|---|---|---|---|
| 視窗與輸入分兩個 Module、輸入不認得視窗 | Bevy（bevy_window／bevy_input，一個 adapter 產生兩種事件） | (i) 合成一個（Godot `DisplayServer`）：介面數百個方法、只能整組換、輸入測試要帶視窗〔Godot 的理由是推論〕；(ii) 照 master（input 後端掛在 platform 的視窗上、讀原生 `SDL_Event`）：`native/` 的形，為舊形狀找位置 | KeepTool（Q3 (b)）、PvP Client；input 單獨給測試與網路的輸入注入 |
| 事件泵在有視窗的 runner、由 SDL 的實作做 | Bevy `WinitPlugin::set_runner`；runtime-composition §4「runner plugin」（使用者 11-11） | (i) 泵放在 `window` 的 System：System 在哪條執行緒跑是 runner 定的（P19），等於讓 Kernel 的排程替 SDL 守執行緒；(ii) 泵放在產品的 `main`：每個產品抄一份 | 所有有視窗的產品 |
| 輸入經「輸入服務＋套用點」進 World | O-12 套用點；Bevy `bevy_gilrs`（服務＋System） | runner 直接寫進 World（Bevy winit）：runner 要認得輸入的型別、每個讀輸入的 World 都要 runner 寫一次；交換點宣告 5.6 留給本批選，**卡片照 L17 判** | KeepTool、PvP Client（UI 的與模擬的 World 都要輸入） |
| 渲染 World＋Extract | Bevy `SubApp::extract`、`bevy_extract`；使用者 11-11 | master 的「產品每幀填 `RenderQueue`」：沒有唯讀的保證；Bevy 的 `ResMut<MainWorld>` 寫回：使用者 11-11 不採 | PvP Client、工具的預覽、每個 2D／3D 遊戲 |
| render 底層共用、上層分 2D／3D 管線 | 使用者 11-23（P24）；Bevy `core_2d`、`core_3d` | 一條管線用旗標分 2D／3D：使用者已定照 Bevy | 2D 遊戲、PvP Client |
| Transform 一套 3D、放在 transform Module、父子不進 Kernel | 使用者 11-23（一套 3D）；Unity Entities `Unity.Transforms`（`LocalTransform`、`ParentSystem` 在 Entities 的 Transforms 組件，不在核心）；Bevy `bevy_transform` | 父子放進 Kernel（Bevy 的 `ChildOf` 在 `bevy_ecs`）：Kernel 職責清單（DI-KS 15）多一項「關係」，要使用者定；目前沒有 Feature 逼到那一步 | 2D 遊戲、PvP Client、render 的 Extract |

既有的測試、守衛、golden、突變能不能不改，沒有當決勝的理由（§4）。

## 5. Feature → Contract → Architecture（AGENTS §2 Derivation Order）

| Feature | Contract | Architecture |
|---|---|---|
| KeepTool：開窗、ImGui 文字框、日文輸入法組字與確定 | Q3 選 (b)：ImGui 從 `input` 拿鍵盤、滑鼠、文字與組字，文字輸入的開關與候選窗位置是 `input` 的操作；draw list 經 Extract 進渲染 World（P23）。Q3 選 (a)：鍵盤、滑鼠、文字經官方 SDL3 後端，不經 `input`（那時 `input` 的雙軌幀只由測試驗） | `window`、`input`、`render`（R2a）、SDL 的視窗、輸入、GPU 實作、有視窗的 runner、ImGui 的 Plugin |
| 任何遊戲：按鍵、滑鼠 | `input` 的幀（雙軌）；只在套用點進 World；放 Resource | `input`、套用 System |
| PvP Client：輸入時刻、開槍的聲音準時 | 事件時刻是 `Time::TimePoint`（第 8 節的三條保證） | SDL 輸入實作裡的時鐘配對 |
| 任何 2D／3D 遊戲：畫面 | 渲染 World 只讀來源（DI-KS 13）；畫面送出是 `render` 的服務；`GlobalTransform` 是渲染讀的姿態 | Kernel 的 `RunExtract`、編排層的 `extractFrom`、`render`、`transform`、SDL_GPU 實作 |
| HUD 與選單 | `text` 的點陣圖形狀；`gyo.ui` 的資料契約（JSON v1／v2） | `text`、`ui`、`ui_render`。**本線沒有使用它們的 Feature**（KeepTool 不用），只能靠測試驗收（第 15 節） |
| 任何遊戲：聲音 | audio 頁第 4 節 | SDL 的音訊實作 |
| Match：沒有 SDL | Module 不依賴 Plugin；要後端的 Module `default=no` | 刪掉 SDL 的 Plugin 的建置測試 |

## 6. 渲染的 World、Extract、輸入進 World、Transform 與 2D／3D（要涵蓋的第 4 項，K.3-6）

### 6.1 Extract 的實作（O-12 契約 1～6；交換點宣告 5.4「留給 SDL 那批的」）

**前提**：Engine 的（Kernel 的執行原語、編排層的來源與時機，作者做不到）。

1. **Kernel 的原語**：`WorldExecutor::RunExtract(schedule, const World& source)`（暫名）；Extract 參數綁到來源的 const 資料；屬 DI-KS 15 第 (3)、(4) 項之內。最外層 `Run` 指名 extract schedule 是 Programmer Error（斷言），不加 `StartupCode`。
2. **編排層的描述欄位**：讀者 World 的描述帶來源 World 名（暫名 `extractFrom`）；來源不存在、排在讀者之後是啟動錯誤（新的 O-E 碼，**條號在本統合分配：O-E8**，子批 R1 寫文字）。編排層在讀者第一次 `BuildPlans` 之前呼叫 `SetExtractSource`。
3. **清單**：extract schedule 照樣寫在讀者的每幀清單，看到 `extract` 旗標就走 `RunExtract`；讀者的交換函式「進」在它之前。**不放在第一張不算錯**（修訂 0 的「否則啟動錯誤」拿掉：O-4 會把 `Main` 自動放最前面、單位會往清單插入，加錯誤等於對誤用加守衛）；說明頁寫後果：在 extract 之前跑的 schedule 讀到的是上一輪抽的資料。`Product::RunSchedule` 指名 extract schedule 時怎麼辦，子批 R1 定（今天會斷言）。
4. **來源被替換**：「讀者一起重建」與「准許換來源」**在子批 R1 的宣告比、交使用者**（要改的條文：DI-KS 13 與守衛 D-E14、DI-KS 26「之後一輪的 `BuildPlans`」、RC-O13 的觸發條件；AGENTS §3）。本統合的傾向是准許換來源（理由：GPU 的資源與 pipeline 不該跟著讀檔重建；Bevy 的 RenderApp 活過 main world 的變化）；要注意的事實：`ReplaceWorld` 給的是新的 Registry（O-8），DI-KS 13 那句「來源再登記後……讀到新的內容」講的是**同一份** Registry，不能當根據；`SetExtractSource` 存裸指標，`BuildNewFirst` 換上時舊來源就銷毀，所以重設必須在**換上的同一步**，不能留到下一輪。
5. **銷毀**：讀者先於來源（O-15，已有）。
6. **來源壞了**：Product 已結束，不跑 Extract（O-11，已有）。
7. **渲染 World 的時間**：只對時間設計 5.5（二）（模擬與畫面同一條執行緒）定：渲染 World 不帶 world_time，內插要的量（`FixedTime` 的餘數、步長）經 `ExtractRes` 從來源讀（避免兩個 World 各取一次樣、出現兩個「現在」）。這一件和取樣一起在**子批 W** 定（P12：那一批要一起定），R1 照用。5.5（三）（PvP Client：模擬在別的執行緒）的時間來源不在本線，列第 15 節。
8. **實體對應**：薄起步每輪重抽到渲染 World 的 Resource（畫什麼的清單），不保留來源實體的鏡像（R6）。

**執行緒**（修訂 0 的「Extract 是唯一的同步點」錯了，拿掉）：Extract 要兩個 World 都不在 Run 裡（O-12），所以**只在同一條執行緒上的兩個 World 之間**。PvP Client 照使用者 11-11 的組法（runtime-composition §2）：模擬 World S 在自己的執行緒；畫面 World F 與渲染 World R 在主執行緒，R 經 Extract 讀 F；S 與 F 之間照 O-13 (3) 經各自有鎖的物件交換、不等另一條執行緒——在 PvP 移植時定。不開 render 執行緒（master D27）。

**讀回**：GPU 讀回只給測試與驗收（截圖、比對確切顏色），回給呼叫的人，不寫進任何 World（DI-KS 13；runtime-composition 3b 的選取用 CPU 射線）。

### 6.2 O-12 的四種形在本線各管什麼

| 資料 | 形 | 理由 |
|---|---|---|
| 模擬（或文件）World → 渲染 World | **Extract**（同一條執行緒上 World 到 World 唯一的路） | P7 |
| ImGui 的 draw list（文件 World → 渲染 World） | **Extract**（P23） | 使用者 11-11 §3b |
| OS 事件 → 讀輸入的 World | **套用點「進」**（輸入服務在共用處，`Main`／`First` 的套用 System 取） | 第 4.3 節 |
| 渲染 World → GPU（送出畫面） | **套用點「出」**（`render` 的服務在共用處，渲染 World 的 `Main`／`Last` 一個 System 送） | 和 audio 5.3 同形：服務單一生產者，集中一個 System |
| runner 的時間取樣 → 各 World | **交換函式「進」**（world_time 登記，把 runner 給的取樣寫進 `TimeSampling` 的 `ManualInstant`） | P12；交換點宣告 5.5 已留的接法 |
| 混音器、裝置、視窗、後端 | 不進 World：共用處（O-15）；World 裡只放指向它的 Resource | P9 |
| **ImGui 的全域 context** | **明列的例外**（P22「全域 context 明列例外」）：只有 ImGui Plugin 的兩個 System（文件 World 的 widget 那半、渲染 World 的繪製那半）碰它；繪製那半只讀經 Extract 拿到的 draw list，不呼叫 `ImGui::GetDrawData()` | 全域 context 讓繪製那半很容易繞過 Extract；守衛：設計審查（AGENTS §3，scenario 通過前不立實作方式的守衛） |

交換函式和 Extract 的關係：交換函式是**登記單位**在一個 World 的一輪前後碰 **World 外的服務**（可以寫 World）；Extract 是**讀者 World 的 System** 在自己的一輪唯讀地讀**另一個 World**，存取由 Kernel 推導與檢查。順序：讀者的交換函式「進」→ extract schedule → 讀者其餘的 schedule → 讀者的交換函式「出」。

### 6.3 輸入怎麼進 World，與 Kernel 的 `Summary`（主會話轉來的 asset 宣告第 15 節第 8 項）

**事實**：Kernel 的 `World::Summary` 雜湊每個訊息緩衝的序號與游標（`engine/ecs/kernel/src/World.cpp:1190-1205`，gyo2 1d4a8d7a；和 payload 的雜湊函式無關）。依 OS 事件發 `Message`，摘要就依時序而變。

**本卡的判斷**（L17）：`input` 把這一幀的狀態與依序事件放在一個 **Resource**（每輪由套用 System 換掉），**不用 `Message`**；這個 Resource 的雜湊函式什麼都不寫。`window` 的視窗事件（大小改變、要求關閉）同樣放 Resource。
- 理由：和 asset 的 `AssetView`（asset 宣告 5.9.1、5.7 末）、audio 5.4（依時序變的值不進摘要）同一個判法；原始的 OS 輸入本來就不是決定性的資料。判定要的輸入由作者換成每 tick 的遊戲輸入，走回滾的輸入歷史（`rollback.Input<T>()`）——那份才進摘要。
- 代價：(1) 和 Bevy 不同（Bevy 用 `Messages<KeyboardInput>`，偵察 2.1）；(2) 判定的 System 若直接讀原始輸入，摘要抓不到——這是誤用，由設計審查抓（不加守衛）。
- **要不要給訊息型別一個「不進摘要」的選項**：Kernel 的事，交主會話定，本線不改。有了那個選項，`input` 可以改回 `Message`（子批 W 之後的事）。

### 6.4 Transform 與 2D／3D 管線（使用者 11-23，P24）

**Transform**（前提：Engine 的——每個 2D／3D 遊戲、render 的 Extract、網路複製、回滾、權威登記都碰它；同一個姿態只能有一份定義）：
- **放在哪**：新 Module `transform`（`engine/modules/transform`），進 World、`default=yes`、不要後端（L17）。不放 core（core 不認得 ECS，`docs/architecture/README.md`〈Directory layers〉）；不放 Kernel（Kernel 的職責是 ECS 機制，元件的語意不是它的，DI-KS 15）。
- **形**：`Transform`（位置 `Vec3`、旋轉 `Quaternion`、縮放）是作者寫的、權威的姿態；`GlobalTransform`（全域矩陣）是推導值，由傳播的 System 在**來源 World 的 `PostUpdate`** 算（Bevy `TransformSystems::Propagate` 在 `PostUpdate`，`bevy_transform/src/plugins.rs:27-46`），渲染 World 經 Extract 讀 `GlobalTransform`。2D 遊戲用同一套，z 拿來排序（Bevy `transform.rs:87`）。
- **和回滾、複製、權威**：`Transform` 由作者決定要不要登記成可回滾、要不要複製（作者的）；`GlobalTransform` 可重建，**不登記回滾、不複製、不進權威的寫者檢查**，雜湊函式在子批 X 定（它由 `Transform` 決定性地算出，可以進摘要，也可以不寫）。判定程式要不要直接用 `Transform`（例：PvP 的模擬是否用自己的位置元件）是作者的，網路那一側定。
- **父子**：`Transform` 的意思從一開始就定成「相對父的局部姿態；沒有父時等於全域」，之後加父子不必改它的意思（R8）。父子照 Unity Entities 放在 transform Module（`Parent`＋傳播、子清單由 hook 或 observer 維護），**不進 Kernel**；Bevy 把 `ChildOf` 放在 `bevy_ecs`，是別家分歧——要放 Kernel 是職責清單多一項，那時交使用者。薄起步只做沒有父的同步，父子等點名的 Feature（2D 遊戲或 PvP 若要，在它那批做）。
- **縮放用 `Vec3`（Bevy）還是 `float`（Unity，非等比另放矩陣）**：別家分歧、真有取捨（非等比縮放配上父子會出現剪切；2D 的左右翻轉常用 x = −1），**在子批 X 的宣告交使用者**。
- **先後**：X 在 R2b 之前（R2b 的 Extract 讀 `GlobalTransform`）。

**render 的分法**：
- **共用的底層**：裝置、資源（緩衝、貼圖、shader、pipeline）、Extract、相機（同一套投影：正交或透視，Bevy `Camera2d` 預設正交）、合成（多個相機與疊層組成一幀）、畫面送出。
- **上層的管線**：3D 場景、2D 世界、疊層（螢幕座標的 sprite 與文字，HUD 用）、UI（`ui_render`）。
- **本線實作**：底層全部；3D 場景（PvP、工具的預覽）；疊層（sprite 原語、半透明混合——UI 與文字本來就要）；UI。
- **2D 世界的管線**：使用者會移植的 2D 遊戲還沒點名，推不出需求（sprite 的數量、合批與 z 排序、tilemap、圖集、像素對齊、2D 的光影）。正交相機、sprite 原語、半透明已經在共用的底層與疊層裡；**2D 世界管線本身列成未決 Q5**，請使用者點名遊戲或類別，並決定它在本線（R2b 之後一個子批）還是跟著移植做。
- **VFX**：R7（預見但不實作）。

## 7. audio 的 SDL 輸出後端（要涵蓋的第 5 項）

**前提**：Engine 的（開裝置是平台差異；停的先後是 DI 7 的即時安全，作者不該自己踩）。

**誰建混音器**（audio 頁 5.2 留給本批定；修訂 0 寫「audio 這一側的登記單位建」——會破 DI 2，或等於使用者 11-22 撤掉的 `world_audio`——改掉）：

| 做法 | 先後是否成立 | 代價 |
|---|---|---|
| **(i) 音訊後端的單位自己建** ← 卡片照 L17 判 | 同一個單位先 `Create<Mixer>`、再開裝置、再登記 `OnDestroy`；回呼在共用處解構之前跑（O-15）→ 裝置先停 | 混音器這個型別在共用處的擁有者是後端的單位；不要裝置的程式（測試、audio_editor 的離線處理）自己建 `Mixer`（三行） |
| (ii) 產品建好、經 `ProductDescription::options` 交給後端 | 選項活到共用處之後（O-15「描述的閉包與選項在共用處之後」）→ 成立 | audio 頁第 4 節「混音器放在 Product 的共用處」要改；產品每個都寫一遍 |
| (iii) 後端給一個函式，產品在自己的 `configure` 接上 | 產品的 `configure` 在所有單位之後，後端要在那之後才開裝置 → 要另一個時機 | 多一個時機 |
| `world_audio` 那種 audio 側的登記單位 | — | 使用者 11-22 撤掉（`-world-audio-withdrawn`）；放進 audio 本體破 DI 2 |

(i) 的理由：Bevy 的 `bevy_audio` 也是開裝置的那一方擁有輸出（`audio_output.rs:16-31`）；作者不必管先後（P10 的「後端的登記排在建混音器的單位之後」自動成立）；DI 2 不動。audio 頁 5.2 的 `GameAudio` 例子跟著改成只持提交口、`Find<Mixer>` 找混音器（子批 A 改文件，不改 DI）。

**實作**（照 master 修過的做法，偵察 6 節）：`SDL_InitSubSystem(AUDIO)` → `SDL_OpenAudioDeviceStream(預設播放裝置, S16／2ch／48000, 回呼)` → `SDL_ResumeAudioStreamDevice`。回呼在 SDL 的音訊執行緒：每 block 開頭讀一次 `Time::Now()` 當 anchor、`Mixer::Render(span, anchor)`；不丟例外、不配置、不上鎖。後端只持不擁有的參照（DI 7）。停：`SDL_DestroyAudioStream`（返回後不再有回呼）→ `SDL_QuitSubSystem(AUDIO)`。混音器的設定從 `ProductDescription::options` 讀。

**錯誤**：裝置開不了是後端自己的錯誤碼（audio 頁第 4 節第 3 點）。**卡片照 L17 判**：不經 `ProductSetup::Report`（那會讓整個 Product 不建 World——沒有音效卡的機器玩不了不出聲的遊戲）；後端把狀態放在共用處，作者讀了決定。「告訴外面發生了什麼、不替外面決定」。

**`scheduleDelay`**：子批 A 用量測重新推導（Windows 主戰場、macOS、Linux），判定照 §14.1 在 A 的宣告先寫好再量。

**Q1 各選項下**：音訊後端的單位不論在哪（B：`sdl_audio` Plugin；A／A'：音訊 adapter 的單位），(i) 都成立——它只要求「建混音器與開裝置是同一個單位」。

**「audio 那一側的小批」（clip 來源、音效 loader）要不要併進本線**：**建議維持另做**（asset 已合併，可以現在開），不併。理由：(1) 它的兩件（`ClipBank` 留不留、`"sound"` 的資料契約與 loader）是資料層，用現有的 `Mixer::Render` 測試與 asset 的 loader 測試就驗得到；批次表的「那時才有真的 `Render` 可驗」——真的裝置驗的是後端，不是 clip 的來源；(2) 它依賴 asset、不依賴 SDL；本線大，併進來拖慢它；(3) 兩者交界只有「混音器由誰建」，本節已定。批次先後是主會話照 L17 排的，這裡只是建議。

## 8. 輸入事件的到達時刻（要涵蓋的第 6 項）

**前提**：Engine 的（SDL 的時鐘域和 Engine 的不同，只有 SDL 的實作知道；P11）。

- **做法**（照 master `SdlPlatform.cpp:645-674` 修過的版本）：每次泵事件時取一組配對——依序讀 `Time::Now()`、`SDL_GetTicksNS()`、`Time::Now()`，Engine 的時刻取兩次讀數的中點、記下兩者差（bracket）。事件的 SDL 時刻 `t` 換成 `min(中點 + (t − SDL 讀數), 送達時的 Now())`；`t == 0`（`SDL_PeepEvents` 加的、沒有時間）用送達時刻。無號差以 2^64 取模轉 int64。三次讀取的順序與中點固定在一個吃讀取函式的純函式，測試用假的讀取函式以精確等式檢查（master D34）。
- **保證**（master `platform-sdl.md:74-91`）：(1) 不晚於送達；(2) 同一次泵裡 SDL 時刻非零且不遞減的事件，換算後不遞減；(3) `t == 0` → 送達時刻。**不保證**：全序、跨兩次泵的先後（誤差約 ±bracket/2）。（master 的編號是 (1)(2)(4) 保證、(3) 不保證。）
- **已知限制**（記錄、不修，第 15 節）：入列與泵之間的睡眠讓 macOS 偏移睡眠長度；Linux 兩鐘速率可差約 500 ppm；Windows 事件時刻是毫秒解析度。master 的判定曾在部分平台誤判正確的實作（坑 8、9），子批 W 的判定照 master 修過的容許式寫，結果出來之後不放寬。
- **SDL 的時間不出 SDL 的實作**：`input` 的事件只帶 `Time::TimePoint`。
- **不做的**：事件時刻對到第幾 tick——作者的（Unity 用時刻把事件分到 FixedUpdate，Bevy、Godot 用「幀」）。

**「送進 World 外服務的時刻都是時間基準」要不要成跨 Component 規則**：本線出現第二個例子（輸入）。**建議**子批 W 合併時由圖書管理員登記成 CCR，本卡不自己寫。

## 9. plugins 目錄的建置與後端的注入（要涵蓋的第 2 項）

### 9.1 建置

**前提**：Engine 的（建置圖）。

- **已有的**：Plugin 登記檔、`plugins/<名>` 的 `add_subdirectory`、Module 依賴 Plugin 是 FATAL_ERROR、產品經 `gyo_product_units(... PLUGINS ...)` 選、執行時的登記單位表由清單產生（O-6）。
- **一列＝一個目錄＝一個 target＝一個單位**（`GyoBuild.cmake:63`、`GyoUnitRegistry.cmake:345-355`）。修訂 0 寫的「一個 `plugins/sdl`、內含多個 target、產品分別選」現在表達不出來。**本統合定 Q1＝B 時的登記形：多列多目錄**（不改登記檔的格式）：

| 列（暫名，子批 P 照命名規範定） | 種類 | 內容 | 依賴 |
|---|---|---|---|
| `sdl` | Plugin、`library_only` | SDL3 的 FetchContent（`release-3.4.0`＋`gyo-cocoa-ime-composing-keydown.patch`，帶移除條件與「補丁有沒有上」的設定時檢查）；只給依賴它的 Plugin 用 | — |
| `sdl_window` | Plugin、有單位 | 視窗與輸入的實作、事件泵、時間戳換算、有視窗的 runner | sdl、window、input |
| `sdl_gpu` | Plugin、有單位 | GPU 的實作、shader 的編譯工具 | sdl、sdl_window、render |
| `sdl_audio` | Plugin、有單位 | 音訊輸出（第 7 節 (i)） | sdl、audio |
| `sdl_ttf` | Plugin、有單位 | SDL_ttf（vendored FreeType）的實作 | sdl、text |
| `imgui` | Plugin | ImGui 本體＋兩半的 System（Q3 定接法） | Q3 (b)：input、window、render；Q3 (a)：再加 sdl_window、sdl_gpu |

  `SDL_Window*` 從 `sdl_window` 交到 `sdl_gpu`：Plugin 之間可以露第三方型別（使用者 11-13 (iii)），交接在 Plugin 這一側，不經 `engine/`。
- **三種產品各帶什麼**（B）：KeepTool＝Module `window`、`input`、`render`（`transform` 預設帶）＋Plugin `sdl_window`、`sdl_gpu`、`imgui`（`sdl` 是 `library_only`，跟著依賴來）；Match＝都不帶；只測音訊＝Plugin `sdl_audio`（`audio` 是 `library_only`）。
- **SDL3 放 Plugin 自己的目錄**（修訂 0 的「放 `third_party/`」改掉）：`third_party/CMakeLists.txt` 是共通的建置檔、在登記檔之前加入，放那裡刪不掉 SDL。
- **只建有產品選的 Plugin**（`GyoBuild.cmake:59-66` 今天是登記檔有列就建）：子批 P 改成「有作用中的產品或測試選了才 `add_subdirectory`」，否則 Match 的 CI 也會抓、建 SDL3。
- **刪掉 SDL 的建置測試**：CI 的組態檢查——在暫存副本拿掉 `sdl*` 的列與目錄，Engine、所有 Module、headless 的測試照樣 configure、build、ctest，而且 **SDL3 沒有被抓、沒有被建**（看建置目錄與 FetchContent 的狀態）。守 SDL-2。
- **AGENTS §3 的守衛時期**：子批 P 只立行為面的（上面的刪除測試）；「`engine/` 不 include `plugins/`、不含 SDL 標頭」的原始碼掃描與「SDL 只被誰 PRIVATE 連結」的設定時檢查（master G3 的同類，名單照登記檔產生、不手寫；master 漏了 text 與 image）屬實作方式的守衛，在子批 K 的 scenario 通過後立。
- 第三方授權告示（SDL、SDL_ttf、FreeType、ImGui、stb_image）：打包時附（批次表「之後的小事」），子批 K 的 tools 線打包一起做。

**Q1＝A 或 A' 時的不同**：見第 14 節表。

### 9.2 後端的注入（跨子批的介面，留在統合）

**通則**（只用 O-2、O-15 已有的規則，不加新機制）：
1. Module 定後端的介面（例：`Window::Backend`，抽象類別）與一個持有者型別（例：`Window::BackendSlot`，持有一個指向介面的參照與它的擁有權）。
2. **後端的單位在自己的 `Configure` `Create<…BackendSlot>`**（放共用處，O-15）。Plugin 的 `Configure` 在所有 Module 之後（`Product.cpp:213-218`），所以 Module 不在 `Configure` 找後端。
3. **Module 的單位在 `Build` `Find<…BackendSlot>`**；找不到時在 `Check` 回報 Module 自己的錯誤碼（例：`window` 的 W-E1「沒有後端」）。`library_only` 的 Module（`text`）沒有單位：用它的一方（`ui_render`）在自己的 `Build` 找。
4. **兩個後端**：兩個單位 `Create` 同一型別是 Programmer Error（O-2、O-15，斷言）——不另做檢查。
5. **測試**：測試的產品在自己的 `configure`（所有單位之後、任何 `Build` 之前）`Create` 持有者、放假實作；不經登記檔，所以不會被「缺後端」擋下。
6. **擁有與銷毀**：持有者照 `Create` 的反序解構（共用處）；後端要在 World 都銷毀之後、共用處之前停的工作（裝置、執行緒），在自己的 `OnDestroy` 停。

**和使用者 11-13 不同的一點**：P22 定「沒選後端、或選了兩個，是建置 configure 時的明確錯誤」。通則把「沒選」放在啟動時（`Check`）、「兩個」是斷言，不在 CMake 再查一次——要在 CMake 查，登記檔得多一欄「實作哪個 Module 的後端」，和持有者型別是同一個事實寫兩處（AGENTS §9）。這是推翻使用者的舊決定，**交使用者（Q4）**。

**條號的分配**（各子批寫文字，號碼在這裡先定，避免並行時撞號）：編排層 O-E8（Extract 的來源）；`window` W-E1、`input` I-E1、`render` RD-E1、`text`／`ui_render` 的缺後端碼在各自子批從 1 編；新 DI 的前綴 SDL-（整條線）、`window` 的 WIN-、`input` 的 IN-、`render` 的 RD-、`transform` 的 TF-、`ui` 的 UI-（命名規範照現有頁的慣例，子批可照規範微調，不撞既有前綴）。

## 10. 分批與先後（要涵蓋的第 3 項）

### 10.1 總覽（「代號｜簡述 — 模型／檔位」；代號 `v8-arch-gyo2-sdl-<x>`）

```text
v8-arch-gyo2-sdl（本宣告：統合）── Opus／high；敵對式檢查 Fable／high
├─ P   plugins-base｜plugins/ 的登記形、SDL3 的取得、只建選了的 Plugin、刪 SDL 的建置測試 ── Opus／high
├─ R1  extract｜Kernel 的 RunExtract＋編排層 extractFrom（O-E8）＋來源替換（比較後交使用者） ── Opus／high
├─ X   transform｜transform Module（Transform、GlobalTransform、傳播；縮放的形交使用者） ── Opus／high
├─ I   image｜image Module＋stb_image adapter（asset 宣告 5.11） ── Opus／high
├─ W   window-input｜window、input Module＋SDL 視窗與輸入＋有視窗的 runner＋取樣移給 runner＋渲染 World 的時間＋時間戳 ── Opus／high
├─ A   audio-output｜SDL 音訊輸出（建混音器、開裝置、OnDestroy）＋scheduleDelay 重新推導 ── Opus／high
├─ T   text｜text Module＋SDL_ttf 實作 ── Opus／high
├─ R2a render-core｜render 的裝置、swapchain、一個 2D 的貼圖＋混合＋裁切 pass、最小的 shader 管線 ── Opus／high
├─ K   keep-tool｜ImGui Plugin（兩半、draw list 經 Extract）＋KeepTool＋IME 實機一次 ── Opus／high
├─ R2b render-pipelines｜Renderer、相機（正交／透視）、合成、3D 場景、疊層、讀 GlobalTransform、VFX 預見 ── Opus／high
└─ U   ui｜ui（gyo.ui，JSON Module）＋ui_render ── Opus／high
```

先後：
- **馬上可以開**：R1（只動 Kernel 與編排層，不依賴 P）、X、I（不依賴 SDL）。
- **P 之後**：W、A、T（`sdl_ttf` 依賴 `sdl`；T 的驗收是點陣圖的形狀與 rowPitch，不需要畫）。
- **R2a** 等 P、W（畫到視窗）、R1（渲染 World 經 Extract）。
- **K** 等 R2a、W（Q3 (b) 用 `input`）——K 是本線唯一使用者點名的 Feature，接在 R2a 之後、不等整個 Renderer 與 3D 管線（AGENTS §2：衝突時保 Feature 的方向）。
- **R2b** 等 R2a、X（`GlobalTransform`）、I（貼圖）。
- **U** 等 T、R2b。
- 會碰同一批檔的：P、W、A、T 都加 Plugin 登記列（衝突小，後合併的一方 merge 跟上）；W 改 world-time 頁（取樣、渲染 World 的時間），R1 不碰時間（7 照 W 定的用）；R1 改編排層頁的條文，其他子批不碰編排層頁。
- **批次先後最終由主會話照 L17 排**（K 等於 `-keeptool` 那一批併進本線，主會話確認）。

### 10.2 各子批

| 子批 | 範圍 | 上段／下段（navigator） | 實作者／審查者／驗證者 | 驗收的骨幹 | 突變清單的敵對式檢查（§14.2） |
|---|---|---|---|---|---|
| P | 第 9.1 節 | 上段：建置圖 | Opus／high ×3 | 刪 SDL 的建置測試（SDL3 不被抓、不被建）；只建選了的 Plugin；L1 三平台 | 有（刪除測試是 SDL-2 的守衛） |
| R1 | 第 6.1 節 1～6、8；來源替換的比較 | 上段：Kernel、編排層的契約 | Opus／high ×3；宣告送 Fable／high | 唯讀（`Extract<Write<…>>` 編譯失敗照舊）、來源替換、`DropOldFirst`、O-E8；不需要 GPU | 有（DI-KS 13 的守衛） |
| X | 第 6.4 節 Transform | 上段：基盤（複製、回滾、權威登記會積在上面） | Opus／high ×3；宣告送 Fable／high | 傳播的結果（沒有父＝`Transform` 本身）；`GlobalTransform` 不在回滾的登記裡 | 有（TF- 的守衛） |
| I | asset 宣告 5.11、5.4 | 上段：資料契約（`"image"` 型別） | Opus／high ×3 | 照 asset 宣告（PNG 的 golden、`STBI_*` 的設定、拒絕的輸入） | 有（受理判定） |
| W | `input`、`window`、SDL 視窗與輸入實作、有視窗的 runner（`sdl_window` 的單位在 `Configure` 裝，O-3）、取樣移給 runner（world-time 第 3 節、WT-2、WT-6 (2) 的文字照交換點宣告 5.5 改；等待零件在哪）、渲染 World 的時間（第 6.1 節 7）、時間戳、文字輸入的操作 | 上段：公開介面、時間 | Opus／high ×3；宣告送 Fable／high | 時間戳三條保證的測試（假讀取函式、精確等式）；SDL dummy 驅動下的泵與事件；runner 跑 `examples` 的一個視窗程式 | 有 |
| A | 第 7 節 | 上段：audio DI 7 | Opus／high ×3 | `OnDestroy` 的先後（裝置先停）；SDL `disk`／`dummy` 驅動；`scheduleDelay` 的量測（判定先宣告） | 有（DI 7 的守衛） |
| T | `text`、`sdl_ttf` | 上段：公開介面 | Opus／high ×3 | 點陣圖的形狀與 rowPitch；上下顛倒的坑（master 坑 17：在 UV 轉換的單一邊界修，那半在 R2b） | 有（受理判定） |
| R2a | 裝置、swapchain、一個 2D pass（貼圖、混合、裁切）、最小的 shader 管線（HLSL、host 工具、ABI） | 上段：公開介面 | Opus／high ×3 | Linux Lavapipe 上的像素比對（比確切顏色，master 坑 18）；macOS Metallib 離線編譯；幀時間的預算先宣告 | 有 |
| K | ImGui Plugin（兩半；draw list 經 Extract；全域 context 的例外，第 6.2 節）；KeepTool 的登記（`keep_tool`）；組字的畫法（ImGui 不畫組字）；CJK 字型；tools 線打包與授權告示；第 9.1 節延後的實作方式守衛 | 上段：使用者指定的 Feature | Opus／high ×3 | tools 線打包；IME 實機按一次（CI 只驗文字事件的路徑）——「tools 線不需要實機證據」的現行規則要不要改，交 K 的宣告 | 有 |
| R2b | `Renderer`、相機、合成、3D 場景、疊層（sprite、文字的繪製）、讀 `GlobalTransform`、R7 | 上段：公開介面 | Opus／high ×3 | 像素比對；幀時間的預算先宣告 | 有 |
| U | `ui`、`ui_render` | 上段：資料契約（Editor↔Runtime） | Opus／high ×3 | `gyo.ui` v1／v2 的資料契約測試；文字框的組字 | 有（受理判定） |

- 敵對式檢查（navigator：作者 Opus → 檢查者用別的、不小的模型 → Fable）：每個子批的宣告、以及表裡標「有」的突變清單，都送 **Fable／high**。Fable 用不到時，照 navigator「同じモデルで行い、報告に書く」。
- 檔位：照使用者 11-19「xhigh 只在需要調查時用」，全部 high；W 的時間戳若同一個判定兩次不成立，照 §14 停下回報，不自己升檔。
- ultracode：不用（本統合已經拆開）。

## 11. Architecture Delta（AGENTS §3 的七項，整條線寫一次；各子批只寫差分）

1. **Feature**：KeepTool（P14）、2D 遊戲的移植（P24）；之後的工具與 PvP Client；Match 不受影響。
2. **現在的問題**：gyo2 沒有視窗、輸入、畫面、文字、UI、音訊輸出、Transform；master 的 platform 三種角色混在一起，被 P1 退掉；Extract 的執行在第 4 批拿掉。
3. **變的邊界**：新 Module `transform`、`input`、`window`、`render`、`image`、`text`、`ui`、`ui_render`；頂層 `plugins/` 的第一批內容（Q1）；只建選了的 Plugin（`GyoBuild.cmake`）；Kernel 的 `RunExtract`；編排層的 `extractFrom` 與 O-E8；來源替換（R1 交使用者，會改 DI-KS 13、DI-KS 26、RC-O13）；world_time 的取樣移給 runner（WT-6 (2)）；audio 頁 5.2 的例子。
4. **影響的 Module／Product／Tool**：kernel、orchestration、world_time、audio（只改說明頁的例子，DI 2 不動）、建置層；KeepTool（新 tool）。
5. **依賴方向**：新 Module 只依賴 core、ecs、orchestration 與登記檔列的 Module；SDL 只被 SDL 的實作連（Q1 定在哪）；Engine 不依賴 Plugin。
6. **Ownership**：新 Module 的 owner 是架構（Engine）；SDL 的實作的 owner 依 Q1；KeepTool 的 owner 在 K 定。
7. **為什麼沒有更小的改法**：不帶 platform（P1）就必須把它的功能各自找家；KeepTool 需要視窗、輸入、畫面同時存在；Extract 不做執行，render 就只能回到 master 的「產品自己填佇列」（為舊形狀找位置）；2D 與 3D 都要一份姿態（P24）。

## 12. Design Invariants 的草案（整條線；各 Module 的 DI 在各子批寫）

**放哪**：新頁 `docs/architecture/target/v2/sdl.zh-Hant.md`（SDL 的實作的 owner 文件；Q1＝B 時是 `sdl*` Plugin 的頁），由子批 P 開，各子批補。各 Module 的 DI 在各自的頁（`target/v2/window.zh-Hant.md` 等，跟著子批開）。

- **SDL-1 SDL 只在 SDL 的實作裡**：SDL 的型別與呼叫只出現在 SDL 的實作（位置依 Q1）；Engine 的公開標頭不露 SDL 型別。守衛：子批 K 的 scenario 通過後立（第 9.1 節）。
- **SDL-2 沒有 SDL 也成立**：拿掉 SDL 的 Plugin（與登記檔的列），Engine、所有 Module、headless 的測試仍 configure、build、通過，SDL3 不被抓、不被建。守衛：子批 P 的刪除建置測試。
- **SDL-3 一條時間線**：輸入事件的時刻、送進混音器的時刻、runner 的取樣都是 `Time::TimePoint`；SDL 的時刻不出 SDL 的實作。守衛：第 8 節的測試；審查。
- **SDL-4 主執行緒**：視窗、事件泵、swapchain 的取得與送出只在建視窗的執行緒；由 runner 安排、SDL 的實作在進入點斷言（`SDL_IsMainThread`），不由 Kernel 檢查（P19）。
- **SDL-5 渲染只讀**：受 DI-KS 13 約束（全文只在 Kernel 頁）。

**Replacement Criteria**（整條線）：
- RC-SDL 1：要第二個非 SDL 的視窗或 GPU 實作時，某個 Module 的後端介面露出 SDL 才有的概念（換不掉）→ 介面切錯了，重切（master `platform-sdl.md` RC 1、2）。
- RC-SDL 2：事件泵必須離開主執行緒，而 SDL 的主執行緒規則滿足不了 → 換掉有視窗的 runner 的形（master RC 6）。
- RC-SDL 3：每輪重抽（R6）的費用超過 R2b 宣告的幀時間預算（計測）→ 改成保留實體對應。
- RC-SDL 4：保持 SDL-3 需要逐事件取樣或睡眠偵測 → 換掉時間戳的換算（master RC 5）。

## 13. 卡片照 L17 判的（不是使用者的話；可以推翻）

| # | 判了什麼 | 理由 |
|---|---|---|
| L-1 | 新 Module 的名字（`transform`、`input`、`window`、`render`、`image`、`text`、`ui`、`ui_render`）與 Plugin 列的暫名 | 命名規範 N7／N8；和 master、Bevy、asset 宣告同詞 |
| L-2 | 要後端的 Module `default=no`；`transform` `default=yes` | 第 3.3 節 |
| L-3 | `window` 依賴 `input`；`render` 不依賴 `window`、依賴 `transform`；`ui` 與 `ui_render` 分兩個 | 第 3.3 節 |
| L-4 | 不帶剪貼簿、檔案對話框、log 轉接、`SDL_main`、`PlatformName`、`native/`、SDL_image、`InputActionMap`、手把 | 第 3 節各列 |
| L-5 | 輸入經「輸入服務＋套用點」進 World；放 Resource 不用 `Message` | 第 4.3 節、第 6.3 節 |
| L-6 | Extract：不放第一張不算錯；渲染 World 不帶 world_time（只對 5.5（二））；每輪重抽 | 第 6.1 節 3、7、8 |
| L-7 | 有視窗的 runner 由 SDL 視窗的實作在 `Configure` 裝；runner 共用的部分（取樣交給 world_time 的交換函式、等待零件）放中性的地方（編排層或 world_time，W 定），沒畫面的 runner 也用 | 使用者 11-11「runner 由 plugin 安裝」；時間設計第 9 節 (a) 標「我的套用」 |
| L-8 | 混音器由音訊後端的單位建（第 7 節 (i)）；裝置開不了不經 `ProductSetup::Report` | 第 7 節 |
| L-9 | 後端的注入通則（第 9.2 節）與條號的分配 | 只用 O-2、O-15 已有的規則 |
| L-10 | Transform 放 transform Module、父子不進 Kernel、薄起步不做父子 | 第 6.4 節 |
| L-11 | audio 那一側的小批維持另做；子批的先後（建議，主會話排） | 第 7 節、第 10.1 節 |

## 14. 要使用者定的

### Q1：SDL 的 adapter 放哪

**前提**：Engine 的（外部組件怎麼接進 Engine 是建置圖與規則文字的事）。**每個選項都要動規則文字**，條文由風紀委員起草、你核准，本卡不改規則。

**時間線**（這一題是在你自己前後幾個說法之間選）：
1. 11-11～11-13：「SDL 本身是一個 `sdl` plugin、內含多個後端 target；整個 repo 的 SDL 呼叫只在 `sdl` plugin」（P22，runtime-composition §4）——這是選項 B 的形，**是你定過的，不是本卡推導的**。
2. 11-14：Module／Plugin 以「少了它遊戲成不成立」分，SDL 歸 Module。
3. 11-16：改成「誰維護、介面是誰的」，SDL 家族歸 Plugin；SDL 的 adapter 歸哪留作未確認（P2）。
4. 11-18：你保留的想法——adapter 放在各 Module 底下當後端（P21，選項 A'）。
5. 11-20：主會話回報「SDL 那一批也沿用 `adapters/sdl`」，你說「good」（P5）。

**例子**：SDL 是一個函式庫，裡面有視窗、事件、GPU、音訊，而且有共用的全域狀態（子系統的計數、整個 process 的 hint、只有一個事件佇列、主執行緒規則）。master：一個 platform 包住視窗與事件，input、GPU 後端經 `native/` 拿 `SDL_Window*`；audio、text 已經是「Module 介面＋SDL 後端」、不經 platform。

| | **A：SDL 當一般的外部組件**（像 yyjson） | **A'：你保留的想法** | **B：一組 `plugins/sdl*`**（11-13 的形） |
|---|---|---|---|
| 目錄與登記 | adapter 在各 Module 的 `src/adapters/sdl/`，是 Module 本體或它的第二個 target；SDL3 放 `third_party/`；登記檔裡沒有 SDL 的 Plugin 列 | adapter 在各 Module 的 `src/adapters/sdl/`，另一個 target；SDL3 是 Plugin `sdl`。adapter 的登記有兩種變體：**A'-m** 登記成 Module 列（依賴 Plugin `sdl`）；**A'-p** 登記成 Plugin 列、目錄在 `engine/` 底下 | `plugins/sdl`（SDL3）、`plugins/sdl_window`、`sdl_gpu`、`sdl_audio`、`sdl_ttf`（第 9.1 節的表）；Module 只定介面、不含 SDL |
| AGENTS §6 撞哪一句 | 判斷句字面成立（我們維護、介面是 Module 的、SDL 完全藏在後面 → Module）；**撞 11-16 的歸屬「SDL 家族是 Plugin」**（同詞異義表第 0 節）與 11-13「SDL 呼叫只在 `sdl` plugin」 | A'-m：撞「Module 不依賴 Plugin」（P3）與「Plugin は Engine から参照されない」；A'-p：撞「Plugin は Engine の外に置く」 | 撞**判斷句**：adapter 是我們維護、介面是 Module 的、SDL 完全藏在後面——照字面是 Module。要在 §6 與同詞異義表第 0 節補一句（例：「第三方組件的 adapter 跟著組件歸 Plugin」） |
| 「一個外部組件一個 adapter」 | 要在架構文件把 SDL 數成多個組件（視訊＋事件、GPU、音訊……），界線切在 SDL 內部的共用狀態上 | 同 A | 也要數：B 的 adapter 分成幾個 Plugin 列。B 的界線切在「SDL 的 Plugin 家族」外面，家族內部可以露 SDL 型別（使用者 11-13 (iii)） |
| 建置的代價 | 最小（同 JSON）；但 SDL3 要改成「有 Module 用到才抓」 | A'-m：登記檔的 `dir` 必須以名字結尾（`GyoUnitRegistry.cmake:95-98`），`modules/window/src/adapters/sdl` 放不進去，要改登記檔；A'-p：`GyoBuild.cmake:63` 改成看 `dir` | 只建選了的 Plugin（第 9.1 節）；多一組 Plugin 列；不改登記檔的格式 |
| 刪掉 SDL | SDL 不是 Plugin，談不上刪；不選視窗類 Module 的產品不連 SDL | A'-m／A'-p：刪 Plugin 列，但 `engine/` 底下的 adapter 目錄還在，要跟著刪或停建 | 刪 `plugins/sdl*` 與列（SDL-2 一個測試守） |
| adapter 之間的協調（子系統計數、事件分派、`SDL_Window*` 交接） | 你的想法：靠 SDL 自己（`SDL_InitSubSystem` 計數、`SDL_PeepEvents` 各取自己那段、`SDL_GetWindowFromID` 換回視窗）——視窗可行；**GPU device 沒有從視窗查回的 API**（C3），交給別人要有露 SDL 型別的標頭 | 同 A | 家族內的標頭交接（`sdl_gpu` 依賴 `sdl_window`） |
| SDL 的程式在不在 `engine/` | 在（五個 Module 目錄） | 在 | 不在 |
| 誰維護 | 各 Module 的 owner | 各 Module 的 owner | SDL Plugin 家族的 owner（架構） |

**另一個退掉的選項**：(C) 一個 `sdl` Module 包住所有 SDL——等於 platform 換名回來（P1 退掉的形）。

**本卡的建議：B**。理由：(1) 你 11-16 選 Plugin 的理由「特別是SDL這樣的，帶著一堆插件，合到Engine里覺得在自己的系統裡又有一坨子系統，不合又動不了」——A、A' 把 SDL 的程式放回 `engine/` 的五個目錄，B 讓 `engine/` 裡沒有 SDL；(2) 你的原則「一個公開出口、各實作注入」：三個選項的 Module 都是出口，B 的實作在 Engine 外面注入（第 9.2 節），換掉或刪掉實作不碰 Engine；(3) B 是你 11-13 定過的形，11-16 改定義之後只差判斷句要補一句；(4) Bevy 同形（`bevy_winit` 依賴 `bevy_window`，偵察 8.4）。**B 的代價**：§6 判斷句與同詞異義表要補一句（Plugin 裡有我們寫的 adapter）；多一組 Plugin 列與「只建選了的」的建置改動；「一個 adapter」也要在架構文件寫怎麼數。
**A 的長處**（若你更重視規則的字面）：判斷句不必改、建置最省；代價是推翻 11-16 的「SDL 家族是 Plugin」。
**你的想法（A'）裡留下的，B 也保留**：不再有 platform（P1）、不要 `native/`（`engine/` 裡沒有帶 SDL 型別的標頭）、「Module 介面＋實作」一個概念、`adapters` 的名字（JSON、image、ufbx 這類 Module 側的 adapter 照用）。

**選定之後要改的**：A／A'——第 9.1 節的登記形、SDL-1 的守衛範圍、第 3.3 節不變；B——同詞異義表第 0 節、AGENTS §6 判斷句的補句（風紀委員起草）、批次表 SDL 列的「SDL 放 `src/adapters/sdl/`」改成 `plugins/sdl*`。

### Q2：ufbx 與 stb_image 歸哪一類

**前提**：Engine 的（同 Q1）。**例子**：model 回來時要讀 FBX；畫面要讀 PNG 貼圖。ufbx、stb_image 都是單檔的 C 函式庫；master 的 ufbx 用 `UFBX_NO_STDIO`（不自己開檔），asset 宣告 5.11 的 stb_image 用 `STBI_NO_STDIO`、不用全域狀態；輸出都轉成我們的型別，沒有主執行緒規則、沒有要拿它原始型別的第三方。

| | **Module 側**（擁有型別的 Module 的 `src/adapters/<組件>/`）← 建議 | **Plugin** |
|---|---|---|
| 和既有的對照 | 同 JSON（yyjson、fast_float）；**stb_image 的位置已寫在你動工前核准的 asset 宣告 5.11**（`engine/modules/image/src/adapters/stb_image/`） | 11-13 runtime-composition §4 把 ufbx 列在第 1 類 plugin（v2 之前） |
| AGENTS §6 判斷句 | 介面是我們的、組件完全藏在後面 → Module（字面成立） | 要讀成「第三方的就是 Plugin」，和 JSON 的判法不一致；asset 宣告 5.11 要改 |

**建議 Module 側**。和 Q1 不同的地方：它們沒有 SDL 那些全域狀態、主執行緒規則，也沒有像 ImGui 這種要拿它原始型別的第三方——是 yyjson 的同類。ufbx 不在本線實作，只定歸類；stb_image 在子批 I。寫進同詞異義表第 0 節的「還沒確認」那一行。

### Q3：ImGui 的接法（推翻 11-13 的一部分）

**前提**：Engine 的（ImGui 是 Plugin，但它碰輸入與畫面的方式決定 O-12 的形守不守得住，作者碰不到）。

**例子**：KeepTool 的文字框。ImGui 每一幀要「這一幀的鍵盤、滑鼠、文字與組字」，畫完交出 draw list（三角形、貼圖、裁切框）。

| | **(a) 官方的 SDL3 平台後端＋SDL_GPU 繪製後端**（11-13，P22） | **(b) 接在我們的 `input`、`window`、`render` 上** ← 建議 |
|---|---|---|
| 系譜 | ImGui 的 `imgui_impl_sdl3`、`imgui_impl_sdlgpu3`〔知〕；master 用 SDL_Renderer 版 | Bevy 的 `bevy_egui`（第三方 UI 函式庫吃 `bevy_input` 的事件、經 `bevy_render` 畫）〔知〕；ImGui 本來就以自己寫後端為前提〔知〕 |
| OS 事件 | 官方平台後端直接吃 `SDL_Event`：OS 事件第二條路進到文件 World 會讀的狀態（O-12「同一件事不要兩種形」） | 只經 `input`（套用點）一條路 |
| 繪製 | 繪製後端要 `SDL_GPUDevice*`：只能在 SDL 的實作那一側（Q1＝A／A' 時要有露 SDL 型別的標頭） | 渲染 World 的 System 經 `render` 的 2D pass 畫 draw list（R2a 本來就要：貼圖、混合、裁切）——就是 §3b 的「draw list 的 GPU 繪製是渲染 World 的 System」 |
| 組字 | 官方後端不處理 `TEXT_EDITING`，master 綁定自己畫 | 一樣要自己畫；候選窗的位置經 `input` 的操作 |
| 和 Q1 | 綁住 Q1：只有 B 不必露 SDL 型別 | 和 Q1 無關（ImGui Plugin 不連 SDL） |
| KeepTool 驗到什麼 | 視窗、GPU；`input` 沒被驗 | 視窗、`input`（含組字）、`render` 的 2D pass、Extract——整條線的基盤 |
| 代價 | 少寫程式 | 要寫 ImGui 的平台與繪製兩段接法（官方後端各約數百行〔知〕）；鍵碼對應、游標形狀、剪貼簿要經 `window`／`input` |

**建議 (b)**：它守住 O-12 的「一條路」與 §3b，讓 KeepTool 真的驗到 `input` 與 `render`，也讓 Q1 不必為 ImGui 選。**推翻的是 11-13 的一句**（「ImGui 官方的 SDL3 平台與 SDL_GPU 繪製後端放在 `sdl` plugin 內的獨立 target」）；11-13 的「全域 context 明列例外」保留（第 6.2 節）。

### Q4：「沒選後端、或選了兩個」的檢查時點（推翻 11-13 的一部分）

**前提**：Engine 的。**例子**：產品選了 `window` 卻沒選任何 SDL 的實作。

- (a) 11-13（P22）：建置 configure 時的明確錯誤——登記檔要多一欄「這個 Plugin 實作哪個 Module 的後端」，CMake 比對。
- **(b) 建議**：啟動時——`window` 的單位在 `Build` 找不到後端、在 `Check` 回報 W-E1；選了兩個是兩個單位 `Create` 同一型別的斷言（O-2、O-15 已有）。理由：「誰是後端」的事實只在持有者型別一處（AGENTS §9）；測試的假實作不經登記檔，(a) 會擋住它（要另開例外）；錯誤仍在任何 World 跑之前出現。代價：要等執行時才發現，不是 configure 時。

### Q5：2D 遊戲是哪一個（或哪一類），2D 世界管線在不在本線

**前提**：作者（產品）的需求決定 Engine 要做到哪裡；管線本身是 Engine 的。

**例子**：橫向捲軸動作（tilemap、大量小 sprite、像素對齊、正交相機跟隨）、俯視角 RPG（tilemap 多層、y 排序）、卡牌或解謎（少量大圖、UI 為主）——需要的管線差很多。

**已經在本線的**：正交相機、sprite 原語、半透明、z 排序的依據（`Transform` 的 z）。**推不出來的**：sprite 的數量級與合批、tilemap、圖集、像素對齊、2D 的光影、z 相同時的排序規則。

**請你定**：(1) 點名要移植的 2D 遊戲，或至少說是哪一類；(2) 2D 世界管線放在本線（R2b 之後一個子批 `R2d`）還是跟著那個遊戲的移植做。本卡沒有傾向：放本線，基盤早驗；跟著移植，需求最準。

## 15. 範囲の限界

- 行動平台（iOS、Android）的入口不支援（不帶 `SDL_main`）。
- IME 只能實機驗（CI 只驗文字事件的路徑）；Windows 的組字（`SDL_HINT_IME_IMPLEMENTED_UI`，整個 process、只讀一次，master 坑 15）在子批 W／K 實機驗。
- GPU 的 CI 證據只有 Linux Lavapipe（software Vulkan）；Windows D3D12 不在 CI；macOS 只做 Metallib 離線編譯。
- macOS `nextDrawable` 的約 1 秒停頓與縮放空窗（master 坑 1、2）在薄起步照舊存在。
- 時間戳的已知限制（第 8 節）。
- master 的 `platform_sdl_concurrency` 在 CI 逾時的原因沒查明（坑 13）；Linux 上 SDL 的 dummy 視訊驅動會開 evdev——子批 W 的 CI 出現時照 §14.1 切分。
- 「替換成另一個實作」在第二個實作出現前只是預留（R1）。
- `text`、`ui`、`ui_render`、R2b 的 3D 場景在本線沒有使用它們的 Feature（KeepTool 不用），只能靠測試驗收；第一個點名的產品回來時才有實際的使用者。
- PvP Client 組法（三）（模擬在別的執行緒）的渲染時間來源、S 與 F 之間的交換，不在本線（第 6.1 節）。
- 2D 世界管線的需求未定（Q5）。

## 16. 和其他卡的協調（經主會話）

- **asset（PR #283，已合併）**：音效 loader 與 clip 來源在 audio 那一側的小批（第 7 節）；圖片 loader 在子批 I（image Module，stb_image）。
- **網路 Module（PR #275）**：輸入的時刻給網路的預測用；事件時刻對到 tick、`Transform` 要不要複製是網路那一側的事。
- **主會話（Kernel）**：「訊息型別不進摘要」的選項（第 6.3 節）。
- **圖書管理員**：「送進 World 外服務的時刻都是時間基準」登記成 CCR（第 8 節末）。
- **風紀委員**：Q1、Q2 的規則文字（同詞異義表第 0 節、AGENTS §6 判斷句）。
- **KeepTool**：子批 K 等於 `-keeptool` 那一批，主會話確認是否併進本線。
- **物理 2D、3D**：兩個 Module，跟著點名的產品回來，不在本線（P24）。

## 17. 修訂 0 的敵對式檢查：處理對照

| 項 | 處理 | 改在哪 |
|---|---|---|
| B-1 Q1 引的規則不成立、偏向 B | 採用。規則列重寫成各選項撞 §6 的哪一句；拿掉以 AGENTS §7、§9 扣 A' 的兩列；拿掉「AGENTS 本體可能不必改」；建議理由改寫 | 第 14 節 Q1 |
| B-2 B 的建置代價、SDL3 放 `third_party/` 的矛盾、一列一 target | 採用。SDL3 放 Plugin 自己的目錄；B 的登記形定成多列多目錄；三種產品各帶什麼；只建選了的 Plugin；刪除測試驗「SDL3 沒被抓、沒被建」 | 第 9.1 節、第 14 節 Q1 表 |
| B-3 「Extract 是唯一同步點」和 O-13 (3) 相反 | 採用。拿掉；寫成「只在同一條執行緒上的兩個 World 之間」與 11-11 的三個 World 組法；第 6.1 節 7 只對 5.5（二） | 第 6.1 節 |
| S-1 A' 欄的不公平 | 採用。測試列刪（分不出高下）；協調列照你的想法寫，點出 GPU device 交接才是難點；「11-18 的讀法」改成選項的兩種變體（A'-m、A'-p），不寫成你的話；補選項 A 與 A'-p | 第 14 節 Q1 |
| S-2 B 的來歷、P5 寫輕 | 採用。Q1 開頭加時間線；B 標明是 11-13 你定過的形；P5 補原文 | 第 2 節 P5、P22；第 14 節 Q1 |
| S-3 ImGui 漏了 §3b、L-4 歸屬寫反、KeepTool 推不出 `input` | 採用。P23、第 6.2 節（draw list 經 Extract、全域 context 的例外）、子批 K 的範圍；不帶 SDL_Renderer 改標使用者的決定；ImGui 的接法改成 Q3；第 5 節第一列照 Q3 分寫 | 第 2、3.2、5、6.2、10.2、14 節 |
| S-4 建混音器的單位 | 採用。三個做法加 `world_audio` 的比較；改成音訊後端的單位自己建（L-8）；第 3.3 節那一列拿掉 | 第 7 節 |
| S-5 准許換來源會動三條條文 | 採用。改成子批 R1 比較後交使用者，列出要改的條文；重設在換上的同一步；DI-KS 13 那句不當根據 | 第 6.1 節 4、第 11 節 3 |
| S-6 清單第一張否則錯誤 | 採用。改成不算錯、說明頁寫後果；`RunSchedule` 交 R1；拿掉成環 | 第 6.1 節 2、3 |
| S-7 必備後端與假實作、`library_only`、注入形推給子批 | 採用。注入的通則與條號留在統合（第 9.2 節）；測試的出口；「選了兩個」；檢查時點和 11-13 不同的部分交使用者（Q4） | 第 9.2 節、第 14 節 Q4 |
| S-8 先後與並行 | 採用。R1 不等 P；R2 拆 R2a／R2b，K 接 R2a；T 只等 P；渲染 World 的時間併進 W；列出會碰同一批檔的子批 | 第 10.1 節 |
| S-9 `ui`、`render` 的依賴 | 採用。`ui`／`ui_render` 分兩個；`render` 不依賴 `window`（理由列 L-3） | 第 3.3 節 |
| S-10 突變清單的檢查只排了 W | 採用。表加一欄，標「有」的都送 Fable；寫明 Fable 用不到時的處理 | 第 10.2 節 |
| S-11 P 的連結檢查的時期 | 採用。P 只立行為面的；連結檢查與原始碼掃描延到 K 之後 | 第 9.1 節 |
| S-12 予見的根據沒出處 | 採用。拿掉「60～144 Hz」，預算交各子批宣告；R4 拿掉；T、U 沒有 Feature 寫進第 15 節；master 的量測只當比較基準 | 第 4.1、4.2、15 節 |
| N-1 「四條保證」 | 改成三條保證、一條不保證，註明 master 的編號 | 第 5、8 節 |
| N-2 基準過時 | 改成 1d4a8d7a；PR #283 已合併 | 第 0、16 節 |
| N-3 P10 的日期、P11 的出處、P17 的原文 | 改 | 第 2 節 |
| N-4 檔案對話框的前提 | 改成「Engine 的，現在沒有 Feature」；拿掉不合的引用 | 第 3.1 節 |
| N-5 A' 不想放正式文件 | 文首註明，核准時可刪 | 文首 |
| N-6 DI 放哪 | 寫明新頁 | 第 12 節 |
| N-7 登記檔有列就建 | 子批 P 改成只建選了的 | 第 9.1 節 |
| N-8 CJK 字型、授權告示 | 加進子批 K | 第 9.1、10.2 節 |
| N-9 runner 放 Plugin 沒列 L17 | 加 L-7（共用的部分放中性的地方） | 第 13 節 |
