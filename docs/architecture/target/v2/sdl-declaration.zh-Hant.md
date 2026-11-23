# 事前宣告：SDL（第一個 Plugin 批）與 platform 的拆法（`v8-arch-gyo2-sdl`）

更新：2026-11-23（修訂 12；核准時一併確認的第 1 件已由使用者確認）。Owner：架構（GYO2 主會話開的任務卡 `gyo2-sdl-declaration`，`local_2cc7e494`）。**狀態：草稿，等使用者的動工前核准。這張卡只寫宣告與分批計畫，不寫實作。**

**經過**：
- **修訂 12 之後**：使用者在 side chat 確認第 14 節末「核准時一併確認的」第 1 件（拆成一組 `sdl*`）；第 2、3 件與 A' 那一句等使用者回覆。
- **修訂 12**：修訂 9～11 送 Fable／high 窄複查：blocker 1、should 9、nit 12（`build/evidence/gyo2-sdl-declaration/declaration-r11-narrow-check.md`），全部採用，對照在第 27 節。最大的改動：Q6 分開「使用者定的」與「side chat／卡片補的」，預設不用 hb-ft、ImGui 不走 imgui_freetype（要走時是 AGENTS §6 的例外、到時交使用者）；`text` 的狀態、圖集上 GPU 的路、`ui` 量字的依賴寫明；相機的目標大小、Object_Connect 的兩件放對位置；`window`、`camera` 都不依賴別的列；第 14 節末加「核准時一併確認的」三件。
- **修訂 11**：使用者在 side chat 定了 Q6——字型的量字、排版、點陣化用 FreeType＋HarfBuzz、放在 `text` Module，不用 SDL_ttf（第 14 節 Q6 記成已定）；`sdl_ttf` 那一列拿掉；子批 T 不再等 P。**交使用者的題目全部定了**。另依頂層目錄的決定補 KeepTool、Object_Connect 的位置（0ccc90c5）。
- **修訂 10**：使用者在 side chat 定了 Q5——要移植的 2D 遊戲是 Object_Connect，需求併進 R2b、不另開 R2d（第 14 節 Q5 記成已定、第 6.4 節改寫）。交使用者的只剩 Q6。
- **修訂 9**：(1) 使用者在 side chat 定了 Q4——(b) 照 Bevy，沒選 SDL Plugin 不算錯（第 14 節 Q4 記成已定）。(2) 修訂 6～8 送 Fable／high 窄複查：blocker 1、should 9、nit 10（`build/evidence/gyo2-sdl-declaration/declaration-r8-narrow-check.md`），全部採用，對照在第 24 節。最大的改動：`text`／`sdl_ttf` 在 B' 下沒有接法（SDL_ttf 又連 SDL、不能進 `engine/`），列成新的 **Q6** 交使用者；函式庫可以依賴有單位的列（只取型別）寫明；`sdl_window` 只放進一個在主執行緒的 World；`camera` 不依賴 `window`（GYO 的視窗不是實體，依賴沒有內容）；第 6.6 節補三項；B' 的殘留九格改掉。
- **修訂 8**：使用者在 side chat 定了 Q3——ImGui 走 (b)，接在 Engine 的 `input`／`window`／`render` 上、不碰 SDL（第 14 節 Q3 記成已定）；`sdl_imgui` 那一列與 (a) 的分支拿掉；剪貼簿、游標形狀的請求資料進 W。交使用者的只剩 Q5。
- **修訂 7**：使用者在 side chat 定了 Q2——ufbx、stb_image 都走 Module 這一側（第 14 節 Q2 記成已定）。交使用者的只剩 Q3、Q5。
- **修訂 6**：使用者在本卡 fork 出的 side chat 定了 Q1＝**B'**（Module 只放資料，SDL 的呼叫全在 Plugin；P27）。照它重寫第 3.3 節（Module 只有資料、`render` 產出 `RenderCommands`）、第 9 節（`plugins/sdl` 拆成角色各一列——留給本卡的選擇，建議理由：一列放不進兩個 World）；第 9.2 節的「後端注入通則」拿掉；Q1 記成已定、Q4 撤回；`input`、`window` 改 `default=yes`、`camera` 照 Bevy 依賴 `window`；第 6.6 節加 `RenderCommands` 這一層的偏離。
- **修訂 5**：主會話知會兩張新卡——audio 那一側的小批另開 `gyo2-audio-clip-loader`（第 7 節改成只做輸出後端）、Kernel 的 `Summary` 與訊息另開 `gyo2-kernel-summary-msg`（第 6.3、6.6 節改成引用它、W 開工前要有結論）；跟上 gyo2 02a22176（HANDOFF 與批次表已在 gyo2 上）。
- **修訂 4**：修訂 3（fae7eb38）送 Fable／high 窄複查：blocker 2、should 10、nit 13（`build/evidence/gyo2-sdl-declaration/declaration-r3-narrow-check.md`）。全部採用，對照在第 20 節。最大的改動：KR 在統合只寫需求與邊界，和 GYO 現行 Kernel 契約的九個衝突點（hook 的種類、不可就地改、一致的時點、連帶刪除的輪數、子的順序、回滾、讀檔、複製、內建元件的登記）列成表，要改條文的在 KR 的宣告交使用者；渲染 World 的保留式照 Bevy 實際的做法改寫、列出四個衝突點；第 6.6 節補九項（最大的是 `gyo.ui` 的節點模型）；X 拆成 X1（平的）、X2（階層，等 KR）；Q4 加照 Bevy 的 (d)。
- **修訂 3**：使用者在主會話更正父子的位置——照 Bevy 放進 Kernel（通用的關係機制、`ChildOf`），`transform` 只放 `Transform`、`GlobalTransform`、傳播；並定「ECS 相關的機制以 Bevy（含生態）為主參考，偏離要寫衝突點與膠水量」（HANDOFF `v8-arch-gyo2-hierarchy-bevy-and-primary-reference`）。改：P25、P26、第 3.3、4.2、4.3、6.4 節；新的第 6.5 節（Kernel 的關係，子批 KR，排在 X 之前）、第 6.6 節（借自別家與偏離 Bevy 的逐項檢查）；渲染 World 的實體改照 Bevy 0.15 起的保留式（第 6.1 節 8、R6）；縮放照 Bevy 用 `Vec3`、不再交使用者。
- **修訂 2**：修訂 1（95f799c0）送 Fable／high 窄複查：blocker 1、should 11、nit 13（`build/evidence/gyo2-sdl-declaration/declaration-r1-narrow-check.md`）。全部採用，處理對照在第 18 節。最大的改動：**來源側與渲染側的單位拆成不同的列**、後端的單位不放進任何 World（第 3.3、9.3 節，修 blocker：依賴逐 World 檢查）；R1 拆成 R1a／R1b，讓來源替換那一題不擋 KeepTool；`GlobalTransform` 定成畫面用的推導值、不進摘要；Q2 只問 ufbx；Q3、Q4 的比較表修正並補選項。同日使用者在主會話定「Transform、相機、物理全部放Module裡」（HANDOFF `v8-arch-gyo2-transform-camera-physics-modules`），寫進 P25、第 6.4 節、子批 X。
- 修訂 0（c9eae0a0）送 Fable／high 敵對式檢查：blocker 3、should 12、nit 9（主 checkout `build/evidence/gyo2-sdl-declaration/declaration-r0-adversarial.md`）。全部採用，處理對照在第 17 節。最大的改動：Q1 的規則比較重寫（四個選項各撞 AGENTS §6 的哪一句）；B 的登記形與建置代價寫進統合；「Extract 是唯一同步點」拿掉（和 O-13 (3) 相反）；後端的注入形留在統合（第 9.2 節）；ImGui 的接法改成交使用者（Q3）；R2 拆兩段讓 KeepTool 早一點。
- 同日主會話轉來三件，一併寫進修訂 1：asset 宣告第 15 節第 1 項（image Module 與 stb adapter，第 3.2 節、子批 I、Q2）、第 8 項（Kernel 的 `Summary` 雜湊訊息序號，第 6.3 節）；使用者在主會話定的 2D／3D 形狀（HANDOFF `v8-arch-gyo2-2d-and-3d`，第 6.4 節、子批 X、R2、Q5）。

**性質**：批次表第 6 組「SDL（第一個 Plugin 批）」那一列的事前宣告＋分批計畫（HANDOFF `v8-arch-gyo2-sdl-declaration-start`，使用者 2026-11-23「開 SDL 的宣告卡」）。範圍大，照「先立統合、再分子份」：本頁是**統合**——定邊界、跨子批的介面（列的角色、登記形、條號的分配）、關聯表、要使用者定的事、各子批的先後與「模型／檔位」。各子批開工前各自再寫一份小宣告。動到 Engine 的公開介面、建置圖、Kernel 的 Extract、時間的取樣、Transform，照 AGENTS §2 當**基盤**處理（第 4 節）。

**關於 Q1 的選項 A'**：A' 是使用者 2026-11-18 保留的想法；使用者當時說「不想放到正式文件裡」。本卡照主會話的指示並列過；Q1 已定 B'，現在只在第 14 節 Q1 的「比過的選項」與 P21 留一句。使用者若不要它留在文件，核准時說一聲就刪。

**用詞**：「**卡片照 L17 判**」＝這張卡照命名規範、別家原始碼、已定的原則自己判的，**不是使用者的話**。別家的事實標出處；**〔知〕**＝憑一般知識、這次沒用原始碼查證（SDL、ImGui 的原始碼本機沒有），不當決勝的根據。「Engine 的／作者的」是每一項的前提：只有 Engine 做得到的（平台差異、跨 Module 的共用機制）是 Engine 的；作者用現有機制能做、而且做法本身是遊戲規則的，是作者的，Engine 只寫契約。

**核心一句**（每一項拿來對照）：**Engine 給視窗、輸入、畫面、聲音輸出各一份資料（型別是我們的、用我們的詞），SDL 在 Engine 外的 Plugin 裡讀寫這些資料；文字（量字、排版、點陣化）整層在 Engine 的 Module 裡算完，SDL 只看到貼圖；什麼時候開窗、按鍵對應什麼動作、畫什麼、UI 長什麼樣、聲音什麼時候響，是作者的。**

---

## 0. 根據與讀的版本

- 程式：`origin/gyo2` 1d4a8d7a（PR #283 asset 合併之後）。`engine/core/time/include/engine/time/MonotonicClock.hpp`；`engine/ecs/kernel` 的 Extract 宣告那一半（`Params.hpp:1318-1340`、`Registry.hpp:673-676`、`Declarations.hpp:90-91`；`Registry::SetExtractSource` 存的是裸指標，`Registry.cpp:673`；**執行那一半隨第 4 批拿掉**，參數建構子會斷言 `Params.hpp:1325`）；`engine/core/math`（`Vec3`、`Quaternion`、`Matrix4`；沒有 Transform 元件）；Kernel 沒有父子關係（`ChildOf` 一類）；`build/cmake/GyoUnitRegistry.cmake`（Plugin 可以 `library_only`；`library_only` 的依賴不必由產品帶，`:236-242`；Plugin 不能 `default=yes`，`:120`；**Module 依賴 Plugin 是 FATAL_ERROR**，`:164-165`；有登記單位的列必須 `library_only=no`，`:105-110`）；`GyoBuild.cmake`（`third_party/` 在登記檔載入之前加入，`:34` 先於 `:55`；**登記檔裡每一個 Plugin 都 `add_subdirectory(plugins/<名>)`，不看產品選了沒**，`:59-66`）；`engine/orchestration/src/Product.cpp`（Plugin 的 `Configure` 在 Module 之後，`:215-218`；銷毀的先後 World → `OnDestroy` 反序 → runner → 共用處，`:75-104`）；`config/plugins.csv`（只有表頭）；頂層沒有 `plugins/`。
- 文件：批次表（修訂 4 以前讀文件分支 `origin/claude/gyo2-time-design`；之後 HANDOFF 與批次表都在 gyo2 上）SDL 列、audio 那一側的小批、「不擋路的小事」；HANDOFF（副本：主 checkout `build/evidence/gyo2-sdl-declaration/HANDOFF-d315368c.md`）的 `-platform-obsolete-split-into-modules`、`-os-in-core-platform-dirs`、`-json-adapters-dir`（含 11-20 兩次推廣）、`-plan-gaps-2026-11-21`、`-audio-review-approved`、`-world-audio-withdrawn`、`-infrastructure-before-products`、`-module-plugin-game-tool`、`-module-plugin-owner-definition`、`-module-plugin-registers`、`-module-deps-in-registers`、`-products-pick-modules`、`-keeptool`、`-thread-binding-by-runner`、`-asset-adapter-world-asset`；主會話 2026-11-23 轉來的 `-2d-and-3d`。
- 同詞異義表 `rebuild-vocabulary.zh-Hant.md` 第 0 節末；AGENTS §2、§3、§4、§6、§14。
- `audio.zh-Hant.md` 第 4 節、第 5.2 節、DI 2、DI 3、DI 7。asset 宣告 `asset-declaration.zh-Hant.md` 5.4、5.7、5.9.1、5.11（stb_image）、第 15 節第 1、8 項。
- 編排層 O-3、O-4、O-6、O-8、O-11～O-15、RC-O13；交換點宣告 5.3～5.6；`world-time.zh-Hant.md` 第 3 節、WT-2、WT-6；時間設計 5.5（二）（三）、第 9 節 (a)；Kernel 頁 DI-KS 13、14、15、26。
- `target/runtime-composition.zh-Hant.md` §2（三個 World 的組法，使用者 11-11）、§3b（Editor 預覽、ImGui 拆兩半，使用者 11-11）、§4（plugins 層、SDL 家族、兩類 plugin，使用者 11-11～11-13）。**v2 之前的文件**（使用者 11-16「後面統一處理」）：當系譜與使用者當時的決定，和 11-16 的定義衝突處以 v2 為準，但**仍是使用者的話**，推翻要交使用者。
- master（只看理由與坑）：偵察報告 `build/evidence/gyo2-sdl-declaration/master-platform-survey.md`（基準 origin/master 3b411e21）。
- 別家：偵察報告 `build/evidence/gyo2-sdl-declaration/reference-engines-platform.md`（Bevy 本機 6a911653；Godot 以 WebFetch 查 master；Unity 以官方文件與〔知〕）。主會話另查：Bevy `bevy_transform/.../transform.rs:87`（z 用來排序）、`bevy_camera/src/components.rs:13`（`Camera2d` 預設正交）、`bevy_core_pipeline/src/core_2d`、`core_3d`；Unity Entities `Unity.Transforms/LocalTransform.cs`、`ParentSystem.cs`（本機 `~/Code/GitHome/com.unity.entities`）。
- 證據都在**主 checkout** `/Users/karasu/Code/Source/GYO-Engine/build/evidence/gyo2-sdl-declaration/`（本機 Intel Mac，git 不追蹤）；偵察者 Opus／high、檢查者 Fable／high，都只讀。

## 1. 先用遊戲講

**KeepTool 開起來**（使用者 11-14：「需要保障基礎的ImGUI沒問題，輸入法也能用」）：程式的 `main` 建一個 `Product`，帶著視窗、輸入、畫面的 Module（只有資料與中性的邏輯）與 `plugins/sdl*`。`sdl_window` 在 `Configure` 裝上「有視窗的 runner」。runner 每一輪在主執行緒上：處理 OS 事件 → 取一次時間 → `RunFrame`（World 依序跑，畫面的 World 最後經 Extract 讀前面的 World、產出 `RenderCommands`，`sdl_gpu` 照它畫、送出）→ 等垂直同步。玩家在 ImGui 的文字框用日文輸入法打字：組字中的字要畫出來、按 Enter 確定時不能把 Enter 漏給程式（master 為此對 SDL 打過補丁）。

**FPS 的 Client 開槍**：滑鼠按下的時刻是 OS 記的（SDL 給的是 `SDL_GetTicksNS` 時間軸上的 ns）。這個時刻在 SDL 的實作裡換成 Engine 的時間基準，之後所有人只看到 `Time::TimePoint`。混音器拿它排起播（audio DI 3）；網路的預測拿它判斷「這一下屬於第幾 tick」是作者（網路 Module）的事。

**讀檔換掉模擬的 World**（O-8）：畫面的 World 不跟著重建（GPU 的資源、pipeline 很貴），之後改讀新的模擬 World（O-12 Extract 第 4 點，做法在子批 R1b 比）。

**一個 2D 遊戲**（使用者 11-23：「我可以移植一個2D遊戲進來」）：角色是帶 `Transform` 的實體，z 決定誰畫在前面；相機是正交投影；畫面走 2D 管線，和 3D 共用裝置、資源、Extract、合成。

**Match（沒畫面的伺服器）**：不選 SDL 的 Plugin；`input`、`window` 預設帶、只是空的資料（使用者 Q4：「master的match就是沒有視窗的」）；建得起來、跑得起來，SDL3 不被抓、不被建。

## 2. 前提表

標「使用者」的是使用者的話；標「L17」的是主會話或卡片自己判的，可以被推翻。

| # | 前提 | 誰定的（出處） | 掛在它上面的決定 |
|---|---|---|---|
| P1 | master 的 `engine/platform` 不帶回，功能拆進各 Module | 使用者 2026-11-19（`-platform-obsolete-split-into-modules`） | 第 3 節的拆法全部 |
| P2 | Module＝我們維護、介面是我們的（外部函式庫完全藏在我們的介面後面也算）；Plugin＝第三方官方維護、接進來用的；SDL 家族與 ImGui 是 Plugin | 使用者 2026-11-16（`-module-plugin-owner-definition`；同詞異義表第 0 節末；AGENTS §6 的判斷句）。**未確認**（當時）：SDL 的 adapter 歸哪、ufbx 歸哪——2026-11-23 都已定（Q1＝B'，P27；Q2＝Module 側）。SDL_image、SDL_ttf 不用了（P20、Q6），不是改了歸類 | Q1、Q2 |
| P3 | Plugin 可以依賴 Module，Module 不能依賴 Plugin；Module 先登記 | 使用者 2026-11-14（`-module-plugin-registers` 第 (5) 點）；建置層 FATAL_ERROR | Q1；Plugin 的 `Configure` 在所有 Module 之後 |
| P4 | 外部組件只在它的 adapter 裡；一個外部組件一個 adapter；adapter 的介面用使用方的詞；「何を一つの外部 Component と数えるか」寫在架構文件；Plugin は Engine から参照されない、Engine の外に置く | AGENTS §6；使用者 2026-11-13（`v8-arch-external-components-as-adapters`） | Q1 各選項撞哪一句 |
| P5 | Module 裡碰外部組件的程式放 `src/adapters/<組件>/`；主會話回報「SDL 那一批也沿用 `adapters/sdl`」，使用者「good，我剛想和你說其他的adapter也這樣的。」 | 使用者 2026-11-20（`-json-adapters-dir` 與兩次推廣） | Q1 的 A、A'；批次表 SDL 列「SDL 放 `src/adapters/sdl/`」 |
| P6 | `core/os` 用 `src/<平台>/`；`src/adapters/<組件>/` 是 Module 的結構；視窗、輸入、聲音裝置不進 core/os | 使用者 2026-11-21（`-os-in-core-platform-dirs`） | 本線不改 core/os 的目錄；SDL 的東西不進 core |
| P7 | 模擬的 World 與渲染的 World 分開；渲染的 World 有自己的 Registry；只經 Extract 以 const 讀來源；不寫回 | 使用者 2026-11-11（K.3 第 6 項、DI-KS 13、runtime-composition §2、§3b） | 第 6 節；子批 R1a、R1b、R2a、R2b |
| P8 | 資料進出 World 的四種形，同一件事不兩種形；Extract 的契約 1～6 | 使用者 2026-11-22 合併前核准（O-12；交換點宣告 5.4） | 第 6 節 |
| P9 | Product 一份的共用處；兩個單位 `Create` 同一型別是 Programmer Error；`OnDestroy` 照登記的反序、在所有 World 之後、共用處之前；產品或測試給單位的物件走 `ProductDescription::options` | 使用者 2026-11-22（O-2、O-15） | 第 9.1 節的視窗登錄表；第 7 節 |
| P10 | audio 對輸出後端的要求（每 block 一次 anchor、一條執行緒呼叫 `Render`、只持參照、`OnDestroy` 先停、錯誤碼是後端自己的） | 使用者 2026-11-22 動工前、2026-11-23 合併前核准（audio 頁第 4 節） | 第 7 節 |
| P11 | 輸入事件的到達時刻在平台邊界換成時間基準，歸 SDL 那批；送進混音器的時刻是時間基準 | 批次表 audio 那一側小批那一列（「輸入事件的到達時刻歸 SDL 那批」）；audio DI 3；`MonotonicClock.hpp` 註解（這一條的判定是 `-world-audio-withdrawn` 時 subagent 的前提，使用者的「照建議」回的是主會話的三件） | 第 8 節 |
| P12 | World 時間的取樣移給 runner，到「第一個需要 runner 取樣的 runner 那批」；取樣、等待零件、渲染 World 的時間、WT-2 的範圍那一批一起定 | 使用者 2026-11-22 合併前核准（交換點宣告 5.5、world-time 第 3 節、WT-6 (2)） | 子批 W |
| P13 | 先建基礎設施、第一個搬回的產品不定 | 使用者 2026-11-19（`-infrastructure-before-products`） | 本線不搬 PvP；驗收用 KeepTool、examples、測試 |
| P14 | KeepTool：基礎 ImGui 沒問題、輸入法能用；交給 GYO2 會話排成一批 | 使用者 2026-11-14（`-keeptool`） | Feature 錨；子批 K |
| P15 | 產品可以只選部分 Module | 使用者 2026-11-17（`-products-pick-modules`） | Match 不選 SDL 的 Plugin |
| P16 | 進 World 的 Module 預設全帶、產品可關 | **L17**（主會話，編排層骨架宣告；「預設全帶可關／預設不帶要列」那一層使用者沒有回，`-products-pick-modules`） | 第 3.3 節：`input`、`window`、`transform`、`camera` 預設帶；渲染側照角色 `default=no` |
| P17 | ui 留 Engine（原文「目前；各遷移批確認」） | 使用者 2026-11-13（kernel HANDOFF `v8-arch-adapter-plugins-review`，`docs/architecture/plans/kernel/HANDOFF.md:92`） | 子批 U。本線就是那個遷移批：照 11-16 的判斷句確認——`gyo.ui` 我們維護、介面是我們的 → Module |
| P18 | Engine 不給標準 `main` | 使用者 2026-11-21（O-3 退出流程） | `SDL_main`／`GyoMain` 不帶 |
| P19 | runner 決定哪條執行緒跑哪個 World；需要綁執行緒的 Resource 自己記、碰錯斷言 | 使用者 2026-11-16（`-thread-binding-by-runner`）；DI-KS 14 | SDL 的主執行緒規則落在 runner 與 `plugins/sdl*` |
| P20 | 圖片解碼用 stb_image、不走 SDL，只在一個 adapter；位置 `engine/modules/image/src/adapters/stb_image/`；image Module 隨 SDL 那批的第一個使用者做 | 使用者 2026-11-21（`-asset-three-decisions`）；asset 宣告 5.11、第 3 節（使用者動工前核准 55a14825） | 子批 I；Q2 |
| P21 | 使用者保留的想法（**不是決定**）：SDL 照舊是 Plugin；我們寫的 adapter 放在各 Module 底下當後端；只留「Module 介面＋後端」一個概念；不要 `native/`，後端之間靠 SDL 自己協調（子系統計數、`SDL_PeepEvents` 各取自己那段、`SDL_GetWindowFromID` 換回視窗）；名字用 `adapters` | 使用者 2026-11-18（主會話記憶；「不再有 platform」11-19 已成 P1；名字照 P5） | Q1 的選項 A' |
| P22 | SDL 是一個 `sdl` plugin、內含多個後端 target；ImGui 官方的 SDL3 平台與 SDL_GPU 繪製後端放在 `sdl` plugin 內的獨立 target；整個 repo 的 SDL 呼叫只在 `sdl` plugin；ImGui、SDL 的全域 context 明列例外；產品用到某能力卻沒選它的後端、或選了兩個，是建置 configure 時的明確錯誤 | 使用者 2026-11-11～11-13（runtime-composition §4，`v8-arch-adapter-plugins-review`；v2 之前） | Q1 的選項 B 的系譜；Q3、Q4 |
| P23 | ImGui 拆兩半：輸入與 widget 邏輯是文件 World 的 System，draw list 的 GPU 繪製是渲染 World 的 System；draw list 經 Extract 進渲染 World；一個函式從 NewFrame 到 Present 消失 | 使用者 2026-11-11（runtime-composition §3b） | 子批 K；Q3 |
| P24 | GYO 要支援 2D 遊戲（使用者會移植一個 2D 遊戲）；Transform 只有一套、用 3D 的，2D 也用、z 拿來排序；render 照 Bevy：底層（裝置、資源、Extract、合成）共用，上層分 2D、3D 兩條管線，相機用同一套投影機制；物理 2D、3D 分兩個 Module，不在這批 | 使用者 2026-11-23（主會話，`v8-arch-gyo2-2d-and-3d`）：「「要」並且我可以移植一個2D遊戲進來。我的意思是：Transform 照建議一套 3D，render 照 Bevy 分管線，物理 2D、3D 分成兩個 Module。」 | 第 6.4 節；子批 X、R2；Q5 |
| P25 | Transform、相機、物理全部放 Module（使用者先問能不能和 math 同級放 core，之後「我明白為何會放Module裡了。因為需要world能碰到」）；相機一個 Module，render 經 Extract 讀它；物理 2D、3D 各一個 Module、不在這批；預設帶不帶由本卡照產品推，主會話的傾向是 Transform、相機預設帶、物理預設不帶 | 使用者 2026-11-23（主會話，`v8-arch-gyo2-transform-camera-physics-modules`）：「Transform、相機、物理全部放Module裡」 | 第 3.3 節、第 6.4 節；子批 X |
| P26 | 父子關係放進 Kernel，照 Bevy 的 `ChildOf` 加通用的關係機制（含連帶刪除、從子查父）；Kernel 的職責清單多一項「關係」；`transform` 只放 `Transform`、`GlobalTransform`、傳播 System，傳播讀 Kernel 的父子。（「加上或拿掉關係時用 hook 立即維護另一邊」是 HANDOFF 經過裡主會話對 Bevy 的描述，不在決定的字裡；Bevy 實際的保證見第 6.5 節。）判法：ECS 相關的機制以 Bevy（含生態）為主參考；偏離 Bevy 或改用別家，要寫明碰到哪些衝突點、要寫多少膠水；Unity、Godot、flecs 只當對照與查證，不當混搭的來源；編排層等 GYO 自己的設計照舊，但寫清楚和 Bevy 的接合點 | 使用者 2026-11-23（主會話，`v8-arch-gyo2-hierarchy-bevy-and-primary-reference`）：「主要我不想當縫合怪…衝突點太多。會導致膠水代碼爆炸」，對主會話的建議回「照建議」；判法寫進批次表〈每批的設計審查〉第 5 項。**取代 P25 裡主會話的讀法**（父子在 `transform` Module、Kernel 清單不變） | 第 6.5、6.6 節；子批 KR、X |
| P27 | **Q1＝B'**：`engine/modules/input`、`window` 只有資料（`InputFrame`、`TextInput`、`TextInputRequest`；邏輯／像素大小、要求關閉）；`render` 不含 SDL，產出 `RenderCommands`（資料），HLSL 原始碼在這裡；`transform`、`camera` 照前；`plugins/sdl` 有 Runner、Window、InputEvents、Ime、Gpu、GpuSubmit、AudioOutput；依賴只有 `plugins/sdl` → `engine/modules/*`；遊戲到 OS 的操作用請求資料、Plugin 每幀讀（晚一幀生效）；Module 沒有後端介面、注入、持有者型別、缺後端檢查；`plugins/sdl` 一列還是拆幾列留給本卡列選項 | 使用者 2026-11-23（本卡 fork 出的 side chat，經 side chat 照四樣轉來）：最後「可以，轉給任務卡」；完整的原話、比過的選項、理由在第 14 節 Q1 | 第 3.3、9 節；全部子批 |

**卡片自加的前提**（不是使用者的話）：
- C1：SDL 3.4 的視窗、事件泵、swapchain 的取得與送出只能在建立視窗的那條執行緒（master 偵察 2 節末，`SDL_gpu.h:4066-4094`、`:4202-4218`，master D27）。
- C2：一個視窗只能被 SDL_GPU 或 SDL_Renderer 其中之一使用（master 偵察 4 節）。
- C3：ImGui 官方的 SDL3 平台後端吃 `SDL_Window*`／`SDL_Event`、SDL_GPU 繪製後端吃 `SDL_GPUDevice*`；SDL 沒有從視窗查回 GPU device 的 API，但 SDL3 的視窗有屬性表可以掛指標〔知，都未查證〕。從外面拿 `SDL_GPUDevice*` 的需求只在 Q3 (a) 出現（Q3 已定 (b)，這個需求不存在）。

## 3. platform 的每一項功能拆到哪裡（要涵蓋的第 1 項）

### 3.1 master 的 `engine/platform/sdl`

master 的 platform 同時扮三個角色（偵察 9 節末）：(a) 視窗與主執行緒事件泵的擁有者、(b) process 服務（入口、執行檔目錄、log 轉接、剪貼簿、檔案對話框）、(c) SDL 時間換到 Engine 時間的唯一位置。下表的「SDL 的實作」指 `plugins/sdl*`（Q1＝B'，P27）。

| master 的功能 | 去處 | 前提 | 理由 |
|---|---|---|---|
| 視窗建立、查詢（視窗、螢幕）、操作、視窗事件 | **新 Module `window`**（只有資料：大小、要求關閉、改視窗的請求）＋`sdl_window`（建視窗、寫資料、讀請求） | Engine：平台差異 | Bevy `bevy_window`／`bevy_winit`；Godot `DisplayServer` |
| 事件泵（主執行緒、一輪處理完才丟例外） | 有視窗的 runner 呼叫、SDL 的實作裡做；事件轉成 `window`、`input` 的資料（ImGui 也從 `input` 拿，不另拿 `SDL_Event`；Q3，使用者已定） | Engine：SDL 的主執行緒規則（C1） | Bevy `WinitPlugin::set_runner`；master 坑 4（提早跳出會截斷下一次泵） |
| 像素大小與密度（DPI） | `window` 的查詢一開始就分「邏輯大小」與「像素大小」 | Engine | master 從沒做（坑 22）；macOS Retina、Windows 縮放上 KeepTool 就會碰到（ImGui 的 framebuffer scale）。只做查詢，不做全螢幕模式 |
| live frame（OS 的 modal 迴圈中畫圖） | **不在薄起步**；介面留一個回呼點（R3），PvP Client 移植時做 | Engine，沒有本線的 Feature | master 坑 1～3 |
| 剪貼簿 | `window` 的資料與請求資料（ImGui 的文字框複製貼上，Q3＝(b)；`sdl_window` 讀寫 SDL 的剪貼簿），子批 W | Engine 的（KeepTool 要） | Bevy 是獨立 crate（arboard），Godot 在 DisplayServer |
| 檔案對話框 | **不帶**；工具回來時在工具那一批加 | Engine 的（平台差異），現在沒有 Feature | master 只有三個工具用；SDL 的回呼在任何執行緒、可能回兩次——那時照 master 的 process 級登錄表做 |
| log 轉接 `SdlLogBridge` | **不帶**；有點名需要時在 SDL 的實作裡接 logging，一個地方 | 那時是 Engine 的 | master 只有 PvP 用；兩個坑（解構順序的遞迴卡死、測試握手） |
| 程式入口 `GyoMain`／`SDL_main` | **不帶** | — | P18；桌面三平台不需要；Windows 的 UTF-8 命令列由 os 的 manifest 處理；Windows GUI 子系統用連結選項保留 `main`（子批 K 驗）；行動平台列範圍的限界 |
| 執行檔目錄 `BaseDirectory` | 需要時加到 `core/os`，**不經 SDL** | Engine：每個產品都要、不需第三方（P6） | 和 `UserDataDirectory` 同處；有使用者時才加 |
| `PlatformName` | 不帶 | — | 只供診斷 |
| 時鐘配對與時間戳換算 | SDL 的輸入實作裡（第 8 節） | Engine | P11 |
| `native/`（帶 SDL 型別的 API） | **不帶** | — | P4、P21、P22；Q1、Q3 已定：SDL 型別只在 `plugins/sdl*` 之間交接 |

### 3.2 master 的 `input`、`render`、`text`、`ui`、audio 的 SDL 後端，與 asset 交來的 image

| master | 去處 | 前提 | 說明 |
|---|---|---|---|
| `GYO::Input`（「狀態＋依序事件」雙軌、文字與組字、失焦清鍵） | **新 Module `input`**（不含 SDL） | Engine：中性資料模型 | 雙軌的理由（同幀按下又放開、事件先後）照 master；`text` 與 `events` 不保證順序照 master。用 `Message` 還是 Resource 照 Kernel 卡 `gyo2-kernel-summary-msg`（第 6.3 節） |
| 文字輸入的開關與輸入法候選窗的位置 | `input` 的請求資料 `TextInputRequest`（ui／ImGui 寫，`sdl_window` 每幀讀、在主執行緒呼叫 `SDL_StartTextInput`／`SDL_SetTextInputArea`，晚一幀生效） | Engine | `gyo.ui` 的文字框與 ImGui（Q3＝(b)）都要；master 每次從 SDL 讀「開沒開」（坑：ImGui 也會開關） |
| `InputActionMap`（按鍵→動作） | **不在薄起步** | 作者（遊戲規則）；有共用需求時再看 | 別家分歧：Bevy 本體沒有、Godot 有 `InputMap`、Unity 有 Actions。PvP 移植時用它的需求再判 |
| 手把 | 不帶 | — | master 沒有程式；沒有點名的 Feature |
| `GYO::Render`（中性 device 介面、`Renderer`、固定 pass、shader ABI `gyo.raster.v1`、讀回） | **新 Module `render`**＋SDL_GPU 的實作＋渲染的 World 與 Extract＋2D／3D 分管線（第 6 節） | Engine | master 沒有 extract、產品每幀自己填 `RenderQueue`——這是本線要換的形 |
| `GYO::RenderBackendSDL`（SDL_Renderer 2D，給 ImGui 工具） | **不帶** | — | 使用者 11-11 §3b（預覽是 draw list 引用 render target 的貼圖、draw list 在渲染 World 畫）與 11-13 §4（ImGui 的繪製後端是 SDL_GPU）的後果，**不是卡片的判斷**；C2 讓 master 的工具繞離屏 |
| shader 管線（共用 HLSL → host 工具 DXC＋SDL_shadercross＋SPIRV-Cross，釘版本與 SHA256） | 子批 R2a 帶回最小的一份；Engine 留 HLSL 原始碼與 ABI，編譯工具在 SDL 的 GPU 那一側 | Engine | P22 |
| `GYO::Text`（中性）＋`TextBackendSDLTTF` | **新 Module `text`**；量字、排版、點陣化用 **FreeType＋HarfBuzz、放在 `text` 的 `src/adapters/`**（Q6，使用者已定）；不用 SDL_ttf | Engine | master 是「介面＋後端」、不經 platform——B' 拿掉了這種形 |
| `GYO::Ui`（`gyo.ui`：codec、驗證、layout、互動、文字框） | **`ui`**（函式庫）＋**`world_ui`**（來源側的單位）；不含 SDL、不依賴 render | Engine（P17） | master 的 `GYO::Ui` 不依賴 Render、Text、Input（master 的 layout 不量字；GYO2 的 layout 要同步量字，所以 `ui` 依賴 `text`，同 Bevy 的 `bevy_ui` → `bevy_text`）；Bevy `bevy_ui` 不依賴 `bevy_render`。JSON 改走 JSON Module |
| `UiRenderer` | **`ui_render`**（渲染側的單位） | Engine | 照 Bevy `bevy_ui_render`、master 的分法；只用 `gyo.ui` 資料的工具與測試不必帶畫面 |
| `GYO::AudioBackendSDL` | SDL 的音訊實作（第 7 節） | Engine（P10） | |
| `GYO::AssetSdlImage` | **不帶**；改由 **image Module**（stb_image adapter） | Engine | P20 |
| image Module（asset 宣告 5.11 定好位置、授權、adapter） | **子批 I**；第一個使用者是 R2b 的貼圖（之後 `ui_render` 的圖片） | Engine | asset 宣告第 15 節第 1 項；stb 的下載照 asset 宣告第 13 節，在 I 問使用者 |
| ufbx（model 的匯入器） | model 回來時，model Module 的 `src/adapters/ufbx/`（Q2，使用者已定） | Engine | 不在本線 |

### 3.3 新 Module 與 Plugin 的清單、「列的角色」（使用者 Q1＝B'，P27；名字照命名規範 N7、N8、N20、N21，卡片照 L17 判；子批可照規範微調）

**B' 的形**（P27）：`engine/` 底下的 Module **只有資料與中性的邏輯**——沒有後端介面、沒有注入、沒有持有者型別、沒有缺後端的檢查；SDL 的呼叫**全部在 `plugins/sdl*`**，Plugin 讀寫 Module 的資料。依賴只有一個方向：`plugins/sdl*` → `engine/modules/*`；`engine/` 底下沒有 SDL。遊戲到 OS 的操作（開關輸入法、候選窗、改視窗）用**請求資料**，Plugin 每幀讀（晚一幀生效），同 Bevy 的 `Window.ime_enabled`／`ime_position` 由 `bevy_winit` 讀。

**為什麼還要分角色**（修訂 1 的 blocker）：編排層逐 World 檢查依賴——World 用了一個單位，它每個不是 `library_only` 的依賴都必須被**同一個 World** 用到、排在前面（`Product.cpp:175-205`，O-E3），依賴檢查只看登記檔、不看 World 的選項（單位雖然可以從 `WorldSetup::WorldName()`、每個 World 的 `Options<T>()` 分辨自己在哪個 World，`Setup.hpp:247-253`，但一列的依賴是它要進的每個 World 的聯集）。所以每一列只扮一種角色：

| 角色 | 登記 | 放在哪個 World | 依賴可以指向 |
|---|---|---|---|
| 函式庫 | `library_only=yes` | 不進 World | 函式庫；**也可以指向有單位的列，只取它的型別**（例：`render` 讀 `camera`、`transform` 的型別；`imgui` 讀 `input`、`window`）。O-E3 不追函式庫的依賴（`Product.cpp:187`）、cmake 也不查——後果：產品沒帶那個單位時沒有檢查；渲染側讀的來源型別沒登記，由 Kernel 的 R-E4 守 |
| 來源側的單位 | 有單位 | 模擬或文件的 World | 函式庫、同一 World 的來源側單位 |
| 渲染側的單位 | 有單位 | 渲染的 World（描述帶 `extractFrom`） | 函式庫、同一 World 的渲染側單位 |
| 只做 `Configure` 的單位 | 有單位 | **不放進任何 World**（產品帶著、每個 World 的 `units` 都不列它） | 函式庫、它讀寫的 Module |

- 只做 `Configure` 的單位不放進 World：`Configure` 對產品帶的每個單位都跑（`Product.cpp:215-219`），逐 World 的依賴檢查只對放進該 World 的單位做。
- 有兩個以上 World 的產品**明列每個 World 的 `units`**（不寫等於全部，`Product.cpp:170`）。
- 渲染側要讀的來源型別，由來源側的單位登記在來源 World；漏了是 Kernel 的 R-E4（`Registry.cpp:309-322`）——不用登記檔的依賴表達。

**Engine 的 Module**：

| 列 | 角色 | 內容 | `default` | 依賴（登記檔） | 子批 |
|---|---|---|---|---|---|
| `transform` | 來源側 | `Transform`、`GlobalTransform`、傳播（父子讀 Kernel 的 `ChildOf`／`Children`） | yes | — | X1、X2 |
| `camera` | 來源側 | 相機元件、投影、畫到哪個目標（中性的目標 id） | yes | transform | X1 |
| `input` | 來源側 | 只有資料與中性的邏輯（不碰 SDL；狀態的算法做成函式，`sdl_window` 的套用 System 呼叫）：`InputFrame`（狀態＋依序事件，帶 `Time::TimePoint`）、`TextInput`（組字字串、游標、確定的文字）、`TextInputRequest`（要不要文字輸入、候選窗的矩形，由 ui／ImGui 寫）。用 `Message` 還是 Resource 照 `gyo2-kernel-summary-msg`（第 6.3 節） | yes | — | W |
| `window` | 來源側 | 只有資料：邏輯／像素大小、焦點、要求關閉、改視窗的請求、剪貼簿的鏡像（有大小上限） | yes | — | W |
| `render` | 函式庫 | 可畫元件與相機的讀法、2D／3D 管線、疊層、排序合批的邏輯（函式；登記 System 的是 `render_world` 與各 `<x>_render`），產出 **`RenderCommands`**（資料：貼圖 id、shader id、頂點、裁切框、混合模式、實例數等）；HLSL 原始碼與 shader ABI；不含 SDL | — | transform、camera（只取型別） | R2a、R2b |
| `render_world` | 渲染側 | 渲染 World 的核心：把 extract schedule 排在最前、跑管線、把這一輪的 `RenderCommands` 放好 | no | render | R2a |
| 可畫元件的來源側單位（例：2D 的 sprite、3D 的網格） | 來源側 | | no | transform | R2b 定名 |
| `image` | 照 asset 宣告 5.4（loader 函式庫＋World 端；依賴多半還要 `world_asset`，子批 I 定） | | no | asset | I |
| `text` | 函式庫 | 介面是我們的：字型、量字、換行、排版結果（字形＋位置＋**每個字形對回原文的範圍**）、點陣圖；FreeType、HarfBuzz 在 `src/adapters/`（Q6）。純函式庫：狀態（字型集、字形快取、圖集）是呼叫者持有的物件——誰持有、放哪個 World、在哪裡點陣化在 T、U 的宣告定，傾向照 Bevy（來源 World 的 Resource，渲染側經 Extract 讀圖集）；字型檔經 asset 的 loader 來的話依賴 `asset`（同 `image`，T 定） | — | — | T |
| `ui` | 函式庫 | `gyo.ui` 的 codec、驗證、layout（同步量字）、runtime | — | json、text | U |
| `world_ui` | 來源側 | | no | ui、input | U |
| `ui_render` | 渲染側 | 產出 UI 的 `RenderCommands` | no | ui、render、text、render_world | U |
| 物理 `physics_2d`、`physics_3d` | 來源側 | | no | transform | **不在本線**（P24、P25） |

**SDL 的 Plugin**（第 9.1 節）：`sdl`（函式庫）、`sdl_window`（來源側）、`sdl_gpu`（渲染側）、`sdl_audio`（只做 `Configure`）。

命名的規則（L17）：`world_<x>`＝x 進來源 World 的那一端（照 `world_asset`、`world_time` 的慣例）；`<x>_render`＝x 在渲染 World 的那一端（照 Bevy `bevy_ui_render`）；`render_world`＝渲染 World 本身的核心（Bevy 的 RenderApp）。

- **`default`**（L17）：只有資料的 `input`、`window` 與 `transform`、`camera` 預設帶——沒有 SDL 時它們只是空的資料，Match 帶著也無害（同 Bevy：`InputPlugin` 無條件、`WindowPlugin` 在 `bevy_window` feature 開著時在 `DefaultPlugins`，`bevy_internal/src/default_plugins.rs:14`、`:19-22`，沒有後端也照跑；使用者 Q4＝(b)）。**明列 `units` 的產品**（多 World）要自己列預設帶的 Module（`world_time`、`entity_ids` 等），漏列不報錯、那個 Module 只跑 `Configure`。`window` 的資料放在每個 World：雜湊函式照 `input` 的判法（第 6.3 節）；World 被替換時 `sdl_window` 每輪重寫（W 定）。渲染側的單位照角色只放渲染 World，`default=no`。修訂 5 以前的「要後端的 Module `default=no`」跟著 B' 拿掉（沒有「缺後端」了）。
- **`window`、`camera` 都不依賴別的列**（L17）：Bevy 的 `bevy_window` 依賴 `bevy_input`，是因為總 enum `WindowEvent` 也裝輸入事件；GYO 的 `window` 資料不含輸入，依賴沒有內容（修訂 9 以前的「`window` 依賴 `input`」拿掉）。`camera` 不依賴 `window` 的理由分兩半：Bevy 的 `RenderTarget::Window(WindowRef)` 那一半沒有內容（GYO 的視窗不是實體、沒有多視窗），目標用 `camera` 定的中性 id；但 **「目標的大小誰給相機」那一半有內容**——Bevy 的 `camera_system` 在 main world 讀視窗大小更新投影（`bevy_render/src/camera.rs:357-363`），`viewport_to_world_2d`、`ScalingMode` 都靠它。這一題列給 **X1 的宣告**定：(a) 照 Bevy 讓 `camera` 依賴 `window`（X1 等 W、列了 `camera` 的 World 也要列 `window`）；(b) `camera` 定「目標大小」的資料、由一個膠水 System 或作者填（X1 不等 W）←傾向；(c) 只在渲染側算投影（來源側做不了射線與滑鼠換算）。
- **`render` 不依賴 `window`**：畫到哪個目標由 `RenderCommands` 裡的目標 id（來自相機）表示，`sdl_gpu` 對到真的視窗。
- **測試**：Module 只有資料與中性的邏輯，測試直接寫資料、呼叫函式（例：在 `InputFrame` 放一筆按鍵、讀 `RenderCommands` 比內容）；不需要假後端。SDL 的 Plugin 自己的測試照 master（`SDL_VIDEODRIVER=dummy`、Lavapipe）。

## 4. 基盤 Architecture 的根據（AGENTS §2）

**為什麼是基盤**：`window`、`input`、`render`、`transform` 的介面上面會積產品（KeepTool、四個工具、PvP Client、2D 遊戲）、ImGui 的接法、`gyo.ui`、網路複製與回滾的登記；Extract 的執行與來源替換是 Kernel 與編排層的契約；Plugin 的目錄與登記是建置圖；有視窗的 runner 決定時間的取樣。

### 4.1 予見的根據

| 根據 | 種類（§2） | 導出的基盤要求 |
|---|---|---|
| KeepTool（P14） | 1 名指的 Feature | 視窗、鍵盤滑鼠、文字輸入與組字、ImGui 畫在 SDL_GPU 上、tools 線能打包；顯示日文要 CJK 字型；刪掉 KeepTool 回到零工具 |
| 2D 遊戲的移植（P24）：**Object_Connect**（Q5，使用者已定；`~/Code/GitHome/Object_Connect` ca4482c） | 1 名指的 Feature | Transform 一套（z 排序）、正交相機、sprite＋image、純色三角形、半透明、畫的先後、日文 HUD 與選單；**本線多出的兩件**：每幀即時產生的動態頂點、固定解析度的縮放（併進 R2b） |
| PvP Client（master 的完整遊戲，P13 的對照） | 1 計畫上的 Feature | 輸入帶 OS 時刻；3D 場景＋疊層＋`gyo.ui`；模擬在自己的執行緒、主執行緒＝事件＋畫面（O-13、時間設計 5.5（三）、runtime-composition §2 的三個 World）；live resize；讀檔換模擬 World 時畫面不重建；VFX（半透明、billboard、instancing，主會話轉來的「不要堵死」） |
| 四個工具（master） | 1（之後回來） | ImGui、3D 預覽（draw list 引用 render target 的貼圖，§3b）、音效預覽、檔案對話框（工具那批） |
| Match | 1 | 不選 SDL 也建得起、跑得起；SDL3 不被抓、不被建 |
| 一般的單機遊戲 | 1（GYO2 的目標：通用引擎） | 薄起步只有單執行緒、渲染 World 在主執行緒 |
| Bevy（含生態，主參考，P26）；Godot、Unity、flecs（只當對照與查證）；master（只看理由與坑） | 2 成熟的參考實作 | 第 4.3、6.6 節 |
| 規模與預算 | 3 | 幀時間、讀入時間的預算**這裡沒有出處**，由 R2a、R2b、K 的宣告各自寫、照 §14.1 先宣告並交使用者；音訊 `scheduleDelay` 的 master 量測（macOS 54.8 ms、Windows 44.4 ms，`2026_10_30`、`2026_10_31` 的 dev_log）只當比較基準，子批 A 重新推導 |

### 4.2 予見的要求寫成邊界（實作薄起步，其餘只定形）

| # | 要求 | 本線實作 | 只定形，留給點名的 Feature |
|---|---|---|---|
| R1 | 一份資料、Plugin 讀寫（P27） | `input`、`window` 的資料；`render` 的 `RenderCommands`；請求資料 | 第二個實作出現前，「替換成另一個組件」只是預留；換 SDL 只重寫 Plugin |
| R2 | Module 的行為測試不靠 SDL | 測試直接寫資料、讀 `RenderCommands` 比內容 | SDL 的實作自己的測試照 master（`SDL_VIDEODRIVER=dummy`、Lavapipe） |
| R3 | live frame | 介面留「OS modal 迴圈中要一幀」的回呼點（宣告形狀，不實作） | PvP Client 移植 |
| R5 | 模擬在別的執行緒 | 薄起步單執行緒 | O-13 的入口在 PvP Client 移植那批 |
| R6 | 渲染 World 的實體與來源對應 | R1a、R2a、K 只抽 Resource 層的資料（draw list），不需要實體對應 | 照 Bevy 0.15 起分三種（同步的實體、以來源實體為鍵的 Resource、暫時的資料），R2b 逐種判；增刪的讀法 R1a 留形（第 6.1 節 8） |
| R7 | VFX 要的繪圖能力 | 疊層與 UI 本來就要的半透明混合（排在不透明之後）；pipeline 的狀態帶混合模式 | 3D 的半透明排序相位、billboard（面向相機的四邊形，在 vertex shader 做）、instancing（每個實例一份資料的緩衝）：R2b 的介面不堵死——送出的單位能帶實例數與實例緩衝、shader ABI 留欄位、相位能排序 |
| R8 | 父子階層 | 子批 KR：Kernel 的關係機制與 `ChildOf`／`Children`（P26）；子批 X2：傳播讀它 | 多層階層的效能（深的樹、大量實體）在有 Feature 量到時再改（RC-TF）；別種關係（例：「瞄準誰」）的使用者由產品帶 |

（修訂 0 的 R4「多個視窗、介面以視窗 id 為鍵」拿掉：沒有點名的 Feature。）

### 4.3 系譜、退掉的選項、需要它的 Feature（AGENTS §4）

| 形 | 系譜 | 退掉的選項與理由 | 需要它的 Feature／Product |
|---|---|---|---|
| 視窗與輸入分兩個 Module、輸入不認得視窗 | Bevy（bevy_window／bevy_input，一個 adapter 產生兩種事件） | (i) 合成一個（Godot `DisplayServer`）：介面數百個方法、只能整組換、輸入測試要帶視窗〔Godot 的理由是推論〕；(ii) 照 master（input 後端掛在 platform 的視窗上、讀原生 `SDL_Event`）：`native/` 的形，為舊形狀找位置 | KeepTool（Q3＝(b)）、PvP Client；input 單獨給測試與網路的輸入注入 |
| 事件泵在有視窗的 runner、由 `sdl_window` 做 | Bevy `WinitPlugin::set_runner`；runtime-composition §4「runner plugin」（使用者 11-11） | (i) 泵放在 `window` 的 System：System 在哪條執行緒跑是 runner 定的（P19），等於讓 Kernel 的排程替 SDL 守執行緒；(ii) 泵放在產品的 `main`：每個產品抄一份 | 所有有視窗的產品 |
| 輸入經「runner 泵進緩衝＋`sdl_window` 的套用 System」進 World | O-12 套用點 | runner 直接寫進 World（Bevy winit）：B' 之後 runner 就是 `sdl_window` 裝的、認得輸入型別，這個理由已不成立；剩下的理由是 O-12（World 只在套用點被寫、和 System 排先後）；交換點宣告 5.6 留給本批選，**卡片照 L17 判** | KeepTool、PvP Client（UI 的與模擬的 World 都要輸入） |
| 渲染 World＋Extract | Bevy `SubApp::extract`、`bevy_extract`；使用者 11-11 | master 的「產品每幀填 `RenderQueue`」：沒有唯讀的保證；Bevy 的 `ResMut<MainWorld>` 寫回：使用者 11-11 不採 | PvP Client、工具的預覽、每個 2D／3D 遊戲 |
| render 底層共用、上層分 2D／3D 管線 | 使用者 11-23（P24）；Bevy `core_2d`、`core_3d` | 一條管線用旗標分 2D／3D：使用者已定照 Bevy | 2D 遊戲、PvP Client |
| Transform 一套 3D、`transform` Module 只放 `Transform`、`GlobalTransform`、傳播 | 使用者 11-23（P24、P25、P26）；Bevy `bevy_transform`（`TransformPlugin`，`DefaultPlugins` 無條件帶） | 父子也放 `transform`（Unity Entities 的 `Parent.cs`、`ParentSystem.cs` 在核心外）：使用者 11-23 改判，理由是混搭的衝突點與膠水（P26）。放 core：core 不認得 ECS（使用者自己問過、改判） | 2D 遊戲、PvP Client、render 的 Extract |
| 父子是 Kernel 的關係（`ChildOf`／`Children`＋通用的關係機制） | 使用者 11-23（P26）；Bevy `bevy_ecs/src/hierarchy.rs:105-152`、`relationship/`；flecs 的 `EcsChildOf` 也在核心（`flecs/include/flecs.h:1844`，對照） | 父子放 Module（Unity）：見上一列 | 2D 遊戲、PvP Client（武器掛在角色上）；UI 若 U 選「節點是實體」也用；ImGui 不用 |
| 相機一個 Module、render 經 Extract 讀 | 使用者 11-23（P25）；Bevy `bevy_camera`（feature 可關；`Camera2d` 預設正交，`components.rs:13`） | 相機放在 render 裡（master 的 `RenderQueue` 帶相機）：模擬與工具要讀相機（射線、可見範圍）時就得帶 render | 2D 遊戲、PvP Client、工具的預覽 |

既有的測試、守衛、golden、突變能不能不改，沒有當決勝的理由（§4）。

## 5. Feature → Contract → Architecture（AGENTS §2 Derivation Order）

| Feature | Contract | Architecture |
|---|---|---|
| KeepTool：開窗、ImGui 文字框（含複製貼上）、日文輸入法組字與確定 | ImGui 從 `input` 拿鍵盤、滑鼠、文字與組字，文字輸入的開關、候選窗位置、游標形狀、剪貼簿是 `input`／`window` 的請求資料（剪貼簿的內容由 `sdl_window` 鏡像到 `window` 的資料，只有「複製」是請求）；draw list 經 Extract 進渲染 World、轉成 `RenderCommands`（P23；Q3＝(b)，使用者已定） | `window`、`input`、`render`／`render_world`（R2a）、`sdl_window`、`sdl_gpu`、ImGui 的 Plugin |
| 任何遊戲：按鍵、滑鼠 | `input` 的幀（雙軌）；只在套用點進 World；`Message` 或 Resource 照 `gyo2-kernel-summary-msg` | `input`、套用 System |
| PvP Client：輸入時刻、開槍的聲音準時 | 事件時刻是 `Time::TimePoint`（第 8 節的三條保證） | SDL 輸入實作裡的時鐘配對 |
| 任何 2D／3D 遊戲：畫面 | 渲染 World 只讀來源（DI-KS 13）；畫面送出是 `sdl_gpu` 的 GpuSubmit 讀 `RenderCommands`（P27）；`GlobalTransform` 與相機是渲染讀的姿態與視點 | Kernel 的 `RunExtract`、編排層的 `extractFrom`、`render`、`render_world`、`transform`、`camera`、SDL_GPU 實作 |
| HUD 與選單 | `text` 的量字、排版結果（字形＋位置＋原文範圍）與圖集；`gyo.ui` 的資料契約（JSON v1／v2） | `text`、`ui`、`world_ui`、`ui_render`。**本線沒有使用它們的 Feature**（KeepTool 不用），只能靠測試驗收（第 15 節） |
| 任何遊戲：聲音 | audio 頁第 4 節 | SDL 的音訊實作 |
| Match：沒有 SDL | Module 不依賴 Plugin；`input`、`window` 只是空的資料 | 刪掉 SDL 的 Plugin 的建置測試 |

## 6. 渲染的 World、Extract、輸入進 World、Transform 與 2D／3D（要涵蓋的第 4 項，K.3-6）

### 6.1 Extract 的實作（O-12 契約 1～6；交換點宣告 5.4「留給 SDL 那批的」）

**前提**：Engine 的（Kernel 的執行原語、編排層的來源與時機，作者做不到）。

1. **Kernel 的原語**：`WorldExecutor::RunExtract(schedule, const World& source)`（暫名）；Extract 參數綁到來源的 const 資料；屬 DI-KS 15 第 (3)、(4) 項之內。最外層 `Run` 指名 extract schedule 是 Programmer Error（斷言），不加 `StartupCode`。
2. **編排層的描述欄位**：讀者 World 的描述帶來源 World 名（暫名 `extractFrom`）；來源不存在、排在讀者之後是啟動錯誤（新的 O-E 碼，**條號在本統合分配：O-E9**——O-E8 曾在異常分級宣告被提出又撤回〔`error-levels-declaration.zh-Hant.md:965`〕，不重用以免查到舊意思；子批 R1a 寫文字，主會話通知並行的卡）。編排層在讀者第一次 `BuildPlans` 之前呼叫 `SetExtractSource`。
3. **清單**：extract schedule 照樣寫在讀者的每幀清單，看到 `extract` 旗標就走 `RunExtract`；讀者的交換函式「進」在它之前。**不放在第一張不算錯**（O-4 會把 `Main` 自動放最前面、單位會往清單插入，加錯誤等於對誤用加守衛）；預設的排法由渲染側的核心單位 `render_world` 在 `Build` 把 extract schedule 插在 `Main` 之前（清單的插入，O-4），作者不必自己排。說明頁寫後果：在 extract 之前跑的 schedule 讀到的是上一輪抽的資料。`Product::RunSchedule` 指名 extract schedule 時怎麼辦，子批 R1a 定（今天會斷言）。
4. **來源被替換**（**子批 R1b**，不在 KeepTool 的路上——KeepTool 不替換 World）：「讀者一起重建」與「准許換來源」**在 R1b 的宣告比、交使用者**（要改的條文：DI-KS 13 與守衛 D-E14、DI-KS 26「之後一輪的 `BuildPlans`」、RC-O13 的觸發條件；AGENTS §3）。本統合的傾向是准許換來源（理由：GPU 的資源與 pipeline 不該跟著讀檔重建；Bevy 的 RenderApp 活過 main world 的變化）；要注意的事實：`ReplaceWorld` 給的是新的 Registry（O-8），DI-KS 13 那句「來源再登記後……讀到新的內容」講的是**同一份** Registry，不能當根據；`SetExtractSource` 存裸指標，`BuildNewFirst` 換上時舊來源就銷毀，所以重設必須在**換上的同一步**，不能留到下一輪。有保留式的對應之後（第 8 點），「准許換來源」還要在同一步把渲染側以舊來源實體為鍵的對照與對應實體**整批作廢**（新 World 的編號可能和舊的相同）——傾向的理由在 R1b 照這個重算。
5. **銷毀**：讀者先於來源（O-15，已有）。
6. **來源壞了**：Product 已結束，不跑 Extract（O-11，已有）。
7. **渲染 World 的時間**：只對時間設計 5.5（二）（模擬與畫面同一條執行緒）定：渲染 World 不帶 world_time，內插要的量（`FixedTime` 的餘數、步長）經 `ExtractRes` 從來源讀（避免兩個 World 各取一次樣、出現兩個「現在」）。內插還要「上一步的姿態」：放在來源 World（作者或 X 給的一個元件，由固定步的最後寫），不放渲染 World（判定與內插的資料留在擁有它的 World；渲染側只讀）——細節在子批 X。這一件和取樣一起在**子批 W** 定（P12：那一批要一起定），R1a 照用。5.5（三）（PvP Client：模擬在別的執行緒）的時間來源不在本線，列第 15 節。
8. **實體對應**：R1a、R2a、K 只抽 Resource 層的資料（ImGui 的 draw list），不需要實體對應。之後照 Bevy 0.15 起的做法分三種、**在 R2b 的宣告逐種判**：標了要同步的種類（Bevy 是相機、光源、sprite、視窗，`SyncToRenderWorld`）在渲染 World 有對應實體；網格一類的每實例狀態放在以來源實體為鍵的 Resource（Bevy 的 `RenderMeshInstances`，`bevy_pbr/src/render/mesh.rs:1214-1230`）；文字一類每幀用暫時的資料。修訂 2 以前的「每輪全部重抽」照 P26 不採。Bevy 的同步把 `RenderEntity` 寫回來源、由讀者清空來源的待同步清單（`bevy_extract/src/sync_world.rs:93-108`、`:221-258`），和 DI-KS 13 不相容：對照表只能放渲染側、增刪的通知誰清、同步這一步要動兩個 World、元件被拿掉的通知——四個衝突點在 R2b 解；R1a 的宣告先留「上次 Extract 之後來源增刪了什麼」的讀法的形（今天的 Extract 參數只有 `Extract<Read<T>>`、`ExtractRes<R>`）。

**執行緒**（修訂 0 的「Extract 是唯一的同步點」錯了，拿掉）：Extract 要兩個 World 都不在 Run 裡（O-12），所以**只在同一條執行緒上的兩個 World 之間**。PvP Client 照使用者 11-11 的組法（runtime-composition §2）：模擬 World S 在自己的執行緒；畫面 World F 與渲染 World R 在主執行緒，R 經 Extract 讀 F；S 與 F 之間照 O-13 (3) 經各自有鎖的物件交換、不等另一條執行緒——在 PvP 移植時定。不開 render 執行緒（master D27）。

**讀回**：GPU 讀回只給測試與驗收（截圖、比對確切顏色），回給呼叫的人，不寫進任何 World（DI-KS 13；runtime-composition 3b 的選取用 CPU 射線）。

### 6.2 O-12 的四種形在本線各管什麼

| 資料 | 形 | 理由 |
|---|---|---|
| 模擬（或文件）World → 渲染 World | **Extract**（同一條執行緒上 World 到 World 唯一的路） | P7 |
| ImGui 的 draw list（文件 World → 渲染 World） | **Extract**（P23） | 使用者 11-11 §3b |
| OS 事件 → 讀輸入的 World | **套用點「進」**：runner 泵進 `sdl_window` 的緩衝（共用處），`sdl_window` 在 `Main`／`First` 的套用 System 寫成 `input` 的資料 | 第 4.3 節、P27 |
| 渲染 World → GPU（送出畫面） | **套用點「出」**：`render_world` 與各 `<x>_render` 的 System 用 `render` 的函式產出 `RenderCommands`（資料），`sdl_gpu` 的 `GpuSubmit` 是渲染 World 的最後一個 System，讀它、畫、送出 | P27；和 audio 5.3 同形：集中一個 System |
| runner 的時間取樣 → 各 World | **交換函式「進」**（world_time 登記，把 runner 給的取樣寫進 `TimeSampling` 的 `ManualInstant`） | P12；交換點宣告 5.5 已留的接法 |
| 混音器、裝置、真的 GPU 物件、SDL 的視窗 | 不進 World：共用處（O-15），由 `plugins/sdl*` 持有；World 裡只有資料（`RenderCommands` 以 id 指 GPU 物件） | P9、P27 |
| 遊戲 → OS 的操作（開關輸入法、候選窗、改視窗） | **請求資料**（`TextInputRequest`、改視窗的請求），`sdl_window` 在 `Main`／`Last` 讀、晚一幀生效 | P27；同 Bevy 的 `Window.ime_enabled`／`ime_position` |
| **ImGui 的全域 context** | **明列的例外**（P22「全域 context 明列例外」）：只有 ImGui Plugin 的兩個 System（文件 World 的 widget 那半、渲染 World 的繪製那半）碰它；繪製那半原則上只讀經 Extract 拿到的 draw list、不呼叫 `ImGui::GetDrawData()`。較新的 ImGui 要繪製後端把貼圖的狀態與 id 寫回 context 裡的貼圖資料〔知〕，字面上守不住——子批 K 查版本後寫清例外的範圍 | 全域 context 讓繪製那半很容易繞過 Extract；守衛：設計審查（AGENTS §3，scenario 通過前不立實作方式的守衛） |

交換函式和 Extract 的關係：交換函式是**登記單位**在一個 World 的一輪前後碰 **World 外的服務**（可以寫 World）；Extract 是**讀者 World 的 System** 在自己的一輪唯讀地讀**另一個 World**，存取由 Kernel 推導與檢查。順序：讀者的交換函式「進」→ extract schedule → 讀者其餘的 schedule → 讀者的交換函式「出」。

### 6.3 輸入怎麼進 World，與 Kernel 的 `Summary`（主會話轉來的 asset 宣告第 15 節第 8 項）

**事實**：Kernel 的 `World::Summary` 雜湊每個訊息緩衝的序號與游標（`engine/ecs/kernel/src/World.cpp:1190-1205`，gyo2 1d4a8d7a；和 payload 的雜湊函式無關）。依 OS 事件發 `Message`，摘要就依時序而變。

**怎麼定**（2026-11-23 起）：這一題由主會話另開的 Kernel 卡 **`gyo2-kernel-summary-msg`** 處理（依外部事件發的 `Message` 讓摘要隨時序變），本線不自己定、不改 Kernel。子批 W 照那張卡的結論做，**W 開工前要有它的結論**：
- 那張卡給了「這類訊息不讓摘要隨時序變」的做法 → `input`、`window` 照 Bevy 用 `Message`（`Messages<KeyboardInput>`、`Messages<WindowResized>`），每個讀者有自己的游標，固定步跑 0 次或多次也不漏不重。
- 沒有 → `input` 把這一幀的狀態與依序事件放在一個 Resource（每輪由套用 System 換掉、雜湊函式什麼都不寫，同 asset 的 `AssetView`、audio 5.4）；代價是偏離 Bevy、作者要在每幀的 System 把輸入累積成自己的每 tick 輸入（Bevy 的 `ButtonInput` 在固定步也要累積，官方範例 `physics_in_fixed_timestep.rs`），寫進 W 的說明頁。
- 兩種都一樣的：原始的 OS 輸入不是決定性的資料；判定要的輸入由作者換成每 tick 的遊戲輸入，走回滾的輸入歷史（`rollback.Input<T>()`）——那份才進摘要。判定的 System 直接讀原始輸入是誤用，由設計審查抓（不加守衛）。
- 和 KR 的關係：那張卡與子批 KR 都改 Kernel，誰後合併誰跟上 gyo2（條號照第 10.1 節的分配）。

### 6.4 Transform、相機、物理與 2D／3D 管線（使用者 11-23，P24、P25、P26）

**Transform**（前提：Engine 的——每個 2D／3D 遊戲、render 的 Extract、網路複製、回滾、權威登記都碰它；同一個姿態只能有一份定義）：
- **放在哪**（使用者 P25、P26）：Module `transform`（`engine/modules/transform`）只放 `Transform`、`GlobalTransform`、傳播 System；來源側的單位、`default=yes`。父子是 Kernel 的關係（第 6.5 節），傳播讀它（照 Bevy：`bevy_transform` 讀 `bevy_ecs` 的 `ChildOf`／`Children`）。
- **形**：`Transform`（位置 `Vec3`、旋轉 `Quaternion`、縮放）是作者寫的、權威的姿態，意思是「相對父的局部姿態；沒有父時等於全域」；父子用 Kernel 的 `ChildOf`（子上）與 `Children`（父上，由 hook 維護）；`GlobalTransform`（全域矩陣）由傳播的 System 在**來源 World 的 `Main`／`PostUpdate`** 算（Bevy `TransformSystems::Propagate` 在 `PostUpdate`，`bevy_transform/src/plugins.rs:27-46`）。2D 遊戲用同一套，z 拿來排序（Bevy `transform.rs:87`）。
- **`GlobalTransform` 是給畫面讀的推導值**（修訂 1 的「可以進摘要」不成立，拿掉）：傳播一幀一次、固定步一幀 0 到多次，某個 tick 的 `GlobalTransform` 落後幾步取決於幀率；回滾放回 `Transform` 後它還是舊時間線的值。所以：**雜湊函式什麼都不寫**（同 `AssetView`；沒有雜湊函式又有值時 `Summary` 回 `HashMissing`，DI-KS 27，所以一定要寫一個空的）、**判定的 System 不讀它**（判定要全域姿態時自己從 `Transform` 與 `ChildOf` 算）、不登記回滾、不複製、不進權威的寫者檢查。Bevy 的 `GlobalTransform` 也是傳播出來的推導值（`PostUpdate`）；對照：Unity Netcode 同樣把畫面用的那一份與判定用的那一份分開（`com.unity.netcode/Documentation~/intro-to-prediction.md:192`）。
- **`Transform` 要不要回滾、複製**：作者的（例：PvP 的模擬是否用自己的位置元件），網路那一側定。`ChildOf` 的回滾、複製、讀檔在第 6.5 節。
- **子批 X 拆兩步**：X1（`Transform`、`GlobalTransform`＝沒有父時等於本身、相機）不需要 KR，R2b 只等 X1；X2（讀 `ChildOf` 的階層傳播）等 KR。
- **子批 X 要處理的洞**：(1) 讀檔或建 World 後第一幀，`PostUpdate` 之前跑的 System 讀到的 `GlobalTransform` 還沒算——Bevy 在 `PostStartup` 也跑一次傳播（`plugins.rs:27-37` 的註解）；GYO 的對應時機（準備函式之後、第一輪之前，或生成世界的最後）在 X 定。(2) 傳播若用變更偵測略過沒變的格，回滾與讀檔的「放回」要被看成變更，否則放回的實體不重算。(3) 骨架關掉的 World 沒有 `PostUpdate`，傳播掛哪裡要能由作者指定（照 world_time 的掛載選項）。(4) 內插要的「上一步的姿態」放在來源 World（第 6.1 節 7）。(5)（X2）`ChildOf` 被拿掉或換掉時子樹要重算（Bevy 靠 `RemovedComponents<ChildOf>` 與 `Changed<ChildOf>` 找孤兒與換父，`bevy_transform/src/systems.rs:54`、`:112-113`、`:339`）。
- **縮放照 Bevy 用 `Vec3`**（修訂 2 列的「Unity 的 `float`＋`PostTransformMatrix`」照 P26 不當選項，不再交使用者）。非等比縮放配上父子會出現剪切，Bevy 的做法是 `GlobalTransform` 存仿射矩陣、照樣相乘；X 的說明頁寫這個後果。
- **先後**：X1 在 R2b 之前（R2b 的 Extract 讀 `GlobalTransform`）；X2 在 KR 之後。

**相機**（使用者 P25）：Module `camera`（`engine/modules/camera`），來源側的單位、`default=yes`、依賴 `transform`。相機元件帶投影（正交或透視，同一套機制）、視區、畫到哪個目標（視窗或貼圖，以 `camera` 定的中性 id 表示，`camera` 不依賴 render、`window`）、排序；視點由同一個實體的 `GlobalTransform` 給。投影的數學用 core/math 現有的（`Matrix4`、`Frustum`）。渲染 World 經 Extract 讀它。2D 遊戲與 3D 遊戲用同一個元件（Bevy `Camera2d` 預設正交，`bevy_camera/src/components.rs:13`）。在子批 X 和 Transform 一起做。

**物理**（使用者 P24、P25）：`physics_2d`、`physics_3d` 兩個 Module，來源側、`default=no`、依賴 `transform`，跟著點名的產品回來，**不在本線**；這裡只記分層與依賴。

**render 的分法**：
- **共用的底層**：Extract、讀相機（`camera`）、合成（多個相機與疊層組成一幀）、`RenderCommands` 的格式；裝置、真的資源、畫面送出在 `sdl_gpu`（P27）。
- **上層的管線**：3D 場景、2D 世界、疊層（螢幕座標的 sprite 與文字，HUD 用）、UI（`ui_render`）。
- **本線實作**：底層全部（`render` 這一側與 `sdl_gpu` 那一側）；3D 場景（PvP、工具的預覽）；疊層（sprite 原語、半透明混合——UI 與文字本來就要）；UI。
- **2D 世界的管線**（Q5，使用者已定：Object_Connect，需求併進 R2b）：Object_Connect（C++20＋KamataEngine／DirectX 12 的 2D 血管接續解謎，固定 1280×720，核心不依賴引擎、沒有 ECS）要的，大多已在本線——正交相機（`camera`）、sprite＋image（節點貼圖，每關 ≤255 種、依格子座標放）、純色三角形 list（矩形、圓、外框，R2a 的 2D pass）、半透明（光暈、脈動、inactive 55% 亮度）、畫的先後（背景 → 血管 → 節點 → HUD 文字最上層：z 排序＋疊層）、日文 HUD 與選單（`text`、`ui`）、滑鼠游標（sprite，畫在 HUD 文字之上；在視窗內隱藏 OS 游標）、失焦暫停（`window` 的焦點）、滾輪。**本線多出的兩件**（使用者定「併進 R2b、不開 R2d」，不變；以下是放對位置的寫法）：(1) **來源側每幀給頂點的 2D 形狀**——作者在自己的 World 每幀給一串頂點（血管是 Verlet 粒子鏈＋三角形帶，矩形、圓用三角形 list；Object_Connect `Docs/Architecture.md` 第 10、11 節、`:265`），是一種新的可畫元件或 Resource，經 Extract 到渲染 World（第 6.1 節 8 的第三種「每幀用暫時的資料」）；`RenderCommands` 帶頂點這一端 R2a 本來就有（ImGui 的 draw list 也是每幀重生的頂點），R2b 補三角形帶與不貼圖的情形。(2) **固定解析度（1280×720）遇到不同視窗大小時的縮放**（加黑邊或拉伸）——對應 Bevy 相機的 `ScalingMode::Fixed`／`AutoMin`（`bevy_camera/src/projection.rs:523-554`）加相機的 `viewport`（`camera.rs:390`）：縮放模式、視區、螢幕↔世界座標的換算是**相機的欄位，X1 留形**，R2b 照視區畫、清黑邊並實作；**縮放後滑鼠位置換回遊戲座標**（Bevy 的 `viewport_to_world_2d`）接在第 3.3 節「目標大小誰給相機」上，HUD／選單的座標也跟著縮（U）。**不做**：tilemap（格子只用來放節點）、大量 sprite 的合批、圖集、2D 光影、物理 Module（Verlet 鏈是遊戲邏輯）——留到有需要的遊戲再做。
- **VFX**：R7（預見但不實作）。

### 6.5 Kernel 的關係（子批 KR，P26）

**例子**：武器掛在角色手上（武器 `ChildOf(角色)`），角色被刪時武器跟著刪；背包裡的道具換到另一個角色身上（換父）；回滾讓預測中被打掉的道具回來。

**前提**：Engine 的（Kernel 的機制；作者在 Kernel 外做不出「刪父連帶刪子、兩邊一致」而不碰 hook 與同步點的契約）。**為什麼拆成先行的 Kernel 子批**：它改 Kernel 的公開介面與職責清單，`transform` 的階層（X2）、可畫元件、網路複製、讀檔都會積在上面；放進 X 一起做，X 的審查會混進 Kernel 的契約。

**統合在這裡定的只有需求與邊界**；Kernel 裡怎麼做，在 KR 的宣告比、要改的條文在那裡交使用者（AGENTS §3）。

**需求**（照 Bevy `bevy_ecs/src/relationship/`、`hierarchy.rs`；P26）：
1. 通用的關係機制：關係元件放在來源實體上、存目標實體（`ChildOf(parent)`）；關係目標元件放在目標上、存來源的集合（`Children`），由機制維護、作者不直接寫（Bevy 的 `collection_mut_risky`，`relationship/mod.rs:296`）。
2. `ChildOf`／`Children`：從子查父；父被刪時連帶刪子（Bevy `hierarchy.rs:148` 的 `linked_spawn`、`relationship/mod.rs:332` 的 `on_despawn`）。
3. DI-KS 15 多一項「關係」（使用者已同意，P26）；邊界統合宣告（`kernel-app-boundary-declaration.zh-Hant.md`）的 Kernel 職責清單同一個變更改。
4. 薄起步（AGENTS §2）：實作一對多的 `ChildOf`／`Children` 與連帶刪除；Bevy 的一對一關係（`source_to_remove_before_add`）、`allow_self_referential`、自訂集合、連帶複製（linked clone）只定形。
5. 寫關係的 Replacement Criteria（AGENTS §3：加大責務時、積功能之前）。例：維護需要當場套用的第三個例外（RC 18 的同類）；連帶刪除超過清空輪上限；`Children` 的順序保不住回滾的等價。

**Bevy 實際保證到哪裡**（修訂 3 寫的「hook 立即維護另一邊」不準，是主會話對 Bevy 的描述、不在使用者決定的字裡）：`on_insert` 把「加進目標的 `Children`」排成 commands，到那一次結構操作結束才看得到（`relationship/mod.rs:194-204`，註解「Deferring is necessary for batch mode」）；`on_discard` 當場改集合（`:238-241`，本機版本的名字是 `on_discard`、不是 `on_replace`）；目標不存在時 `warn!` 並把關係拿掉（`:206-213`）；指向自己同樣拿掉（`:176`）；關係元件是不可就地改的（`bevy_ecs/macro_logic/src/component.rs:310-312`，只能重新插入）。lightyear 也為「Add hook 與 observer 之間沒有 flush」踩過坑（`crates/replication/replication/src/hierarchy.rs:542-544`）。

**和 GYO 現行 Kernel 契約的衝突點**（KR 的宣告逐一比選項、改條文的交使用者）：

| # | Bevy 靠什麼 | GYO 現在 | KR 要比的路 |
|---|---|---|---|
| K1 | `on_insert`、`on_discard`：每次寫入（含覆寫）都跑 | DI-KS 11：hook 只有 OnAdd、OnRemove；覆寫不是 add（`World.cpp:886`）——「換父」沒有 hook | 加 hook 的種類（改 DI-KS 11）／維護內建在 Kernel 的套用步驟（不是 hook） |
| K2 | 關係元件不可就地改 | Kernel 沒有「不可就地改的元件」 | 加這個概念／`Write<ChildOf>` 是 Programmer Error／維護改成在同步點比對 |
| K3 | hook 排的 commands 在那一次結構操作結束前套用 | DI-KS 18：hook 當場改成員是 World 中斷；DI-KS 9：hook 記的變更屬**下一窗**——第一個子加上時父還沒有 `Children` 那一格，要到下一個同步點才有 | 一致性保證到哪一刻（同一筆操作／同一個同步點／下一窗）；可能改 DI-KS 9、18 |
| K4 | `on_despawn` 遞迴到底，沒有層數上限 | hook 記的刪除一層一輪，上限 `hookRoundLimit` 預設 8（`Registry.hpp:386`），超過 World 中斷 | 連帶刪除內建在 despawn 的套用（不走 hook 輪）／放寬上限 |
| K5 | `Children` 的順序是 commands 的先後 | 同步點的 insert、OnAdd 依 (EntityId, ComponentId) 排（DI-KS 9） | 順序定成插入先後（要另記）／定成正規的順序（偏離 Bevy，UI 若要子順序另給排序鍵） |
| K6 | 回滾：Bevy 生態沒有現成的例子（本機 lightyear、replicon 都沒有把 `ChildOf` 登記成回滾元件並驗 `Children` 的；修訂 3 引的 lightyear 測試其實沒回滾 `ChildOf`） | `ExclusiveWorld::Restore` 只對新增、拿掉的格跑 hook；覆寫與相等的格不跑（`Params.hpp:913-921`、`:915-917`；`World.cpp:886`；DI-KS 12「相同的格不寫」）。換父的回滾、順序變過的回滾、預測死亡的復活三種情形 `Children` 都會錯；回滾頁 R-2 前提 1 也要定 `Children` 屬哪一類 | (a) `Children` 自己登記回滾、放回時不跑關係的維護（Bevy 讀檔的 `RelationshipHookMode::Skip`）→ 撞 DI-KS 12「Kernel 不知道一次放回是為了什麼；通知照跑」；(b) `Children` 的順序定成正規的 → 和 K5 同一題；(c) `Restore` 之後整份重建 `Children` → Kernel 多一步 |
| K7 | 讀檔：Bevy 的場景連 `Children` 也存，放回時跳過關係的 hook（`bevy_world_serialization/src/dynamic_world.rs:148-156`、`:192-200`；GYO 的讀檔宣告 5.3 已記過，`restore-entry-declaration.zh-Hant.md:134`） | 讀檔由生成世界的 System 放回；同一個同步點裡目標都已經在（spawn → insert → OnAdd），只有分批放回才會遇到目標還沒在 | (i) 不存 `Children`、放回後的順序照 K5／(ii) 照 Bevy 存、跳過維護 → 碰讀檔宣告第 11 節的 RC（「hook 要分辨讀檔 → 停下來交使用者」，`:231`）與 DI-KS 12 |
| K8 | 複製：bevy_replicon 只複製 `ChildOf`，`Children` 插入時更新（`bevy_replicon/src/lib.rs:255-259`）；父還沒到時，replicon 先建佔位實體、lightyear 另做 `PendingChildOf`（`lightyear/crates/replication/replication/src/hierarchy.rs:44-56`、`:358-405`） | GYO 的複製照外來編號直接生成（批次表「網路複製的外來編號也要先動配發器的範圍」） | 目標不存在時的行為（K 的選擇直接決定網路 Module 要不要自己做等待）；「`ChildOf` 裡的實體編號要映射」這個前提先和網路 Module 對 |
| K9 | Kernel 內建的元件怎麼登記 | — | `ChildOf`／`Children` 的 owner、每個 World 都有還是用到才有；動到 ComponentId 的先後（DI-KS 4）、Registry 指紋與 J-32 的 golden；`StartupCode` 要不要加（關係的登記錯誤） |

**K3、K6、K7（以及 K1、K2 選改條文的路時）在 KR 的宣告交使用者**；KR 的驗收句等選定之後才寫。統合的傾向只有一句：照 P26，能照 Bevy 的照 Bevy，偏離時寫衝突點與膠水量。

**和本線**：X2 的傳播讀 `ChildOf`／`Children`；ImGui、KeepTool 不用；`gyo.ui` 的節點模型（文件樹還是實體）在 U 的宣告判（第 6.6 節）。

### 6.6 借自別家與偏離 Bevy 的逐項檢查（P26 的判法）

ECS 相關的機制以 Bevy（含生態）為主參考。下表把本宣告裡**偏離 Bevy**、或**引用 Unity、Godot、flecs、master** 的地方逐一列出，寫衝突點與膠水量；只當對照的標「對照」。GYO 自己的設計（編排層、共用處、登記檔）照舊，寫清楚和 Bevy 的接合點（HANDOFF `-hierarchy-bevy-and-primary-reference` 決定 (2)）。

| 項 | 和 Bevy 的關係 | 別家的角色 | 衝突點 | 膠水量 | 處置 |
|---|---|---|---|---|---|
| 視窗與輸入兩個 Module、一個 SDL 的實作產生兩種事件 | 同 Bevy（`bevy_window`／`bevy_input`／`bevy_winit`） | Godot `DisplayServer`：退掉的選項（對照） | — | — | 照 Bevy |
| 輸入的雙軌資料（狀態＋依序事件） | 同 Bevy（`ButtonInput`＋訊息）；做法來自 master | — | — | — | 照 Bevy |
| 有視窗的 runner 由 SDL 的實作在 `Configure` 裝 | 同 Bevy（`WinitPlugin::set_runner`） | — | — | — | 照 Bevy |
| 輸入經「runner 泵＋服務＋套用 System」進 World | 接合點：泵像 `bevy_winit`（在 runner），進 World 的那一步是 GYO 的 O-12 套用點（`bevy_gilrs` 的 `Gilrs` 是 World 裡的 Resource、由 System 自己取，`gilrs_system.rs:38-48`，不完全同） | — | Bevy 由 runner 直接寫 World；GYO 只在套用點寫（O-12） | 一個套用 System | GYO 的 O-12，寫明接合點 |
| **輸入的狀態邏輯（雙軌的算法、失焦清鍵）** | **偏離**：Bevy 的 `keyboard_input_system` 在 `bevy_input` 自己（`crates/bevy_input/src/keyboard.rs:170-202`），`bevy_winit` 只送訊息 | — | B'：`input` 只有資料、套用 System 屬 `sdl_window`；測試與網路的輸入注入若只用 `input`，就沒有這段邏輯 | **卡片照 L17 判**：中性的邏輯留在 `input`、做成函式（不是 System），`sdl_window` 的套用 System 呼叫；B' 的「只有資料」讀成「沒有 SDL、沒有後端介面」 | W 照這樣做 |
| **視窗不是實體；輸入法的資料放 `input`** | **偏離**：Bevy 的 `Window` 元件同時是請求與狀態，`ime_enabled`／`ime_position`（`crates/bevy_window/src/window.rs:279-285`）與 `Ime` 事件在 `bevy_window` | — | GYO 的視窗是共用處的服務、World 裡只有資料；請求另立型別 | `TextInput`、`TextInputRequest` 在 `input`（P27 使用者定的形） | 使用者的決定（P27）；不是混搭 |
| 輸入與視窗事件用 `Message` 還是 Resource | 照 Bevy 是 `Message`（`Messages<KeyboardInput>`、`Messages<WindowResized>`） | — | Kernel 的 `Summary` 雜湊訊息的序號與游標（第 6.3 節） | 用 Resource：一個每幀換掉的 Resource＋作者在固定步之前累積；之後再改回 `Message`，每個讀輸入的 System 都要改 | **照 Kernel 卡 `gyo2-kernel-summary-msg` 的結論**，W 開工前要有；那張卡解掉就照 Bevy |
| **事件時刻保留、在平台邊界換成時間基準** | **偏離**（Bevy 不保留 OS 的事件時刻） | Unity Input System 保留時刻（對照）；做法來自 master | 和 ECS 不衝突（只在 SDL 的實作裡） | 一個配對的純函式＋換算 | 使用者的決定（P11）；不是混搭 |
| `input`、`window` 預設帶、沒有 SDL 時只是空的資料 | 同 Bevy（`InputPlugin` 無條件、`WindowPlugin` 在 feature 開著時在 `DefaultPlugins`） | — | — | — | 照 Bevy（B' 之後；修訂 5 以前的 `default=no` 拿掉） |
| 混音器由開裝置的單位建 | 只有「開裝置的一方擁有輸出」同 Bevy；Bevy 的混音器是 rodio 的（`bevy_audio/src/audio_output.rs:9-22`），沒有「Module 介面＋後端單位」的分法 | — | — | — | GYO 的 audio Module＋後端，接合點同 Bevy |
| Extract 只讀、不寫回 | **偏離**（Bevy 的 `ResMut<MainWorld>` 寫回；`extract_render_asset` 把 main 的素材資料搬走，`render_asset.rs:296`；`Time<Real>` 經 channel 回推，`bevy_render/src/lib.rs:522-535`） | — | GPU 讀回、surface 色彩空間、素材上傳後 CPU 那一份 | 讀回只給呼叫的人（測試）；GYO 的素材在 World 外的 `AssetStore`（asset 宣告），渲染側直接讀服務、不必從 World 搬走——上傳後 CPU 那一份留不留在 I、R2b 定 | 使用者 11-11 的決定（P7）；不是混搭 |
| 渲染 World 的實體保留式 | **方向同 Bevy 0.15 起，做法偏離**：Bevy 只有標了 `SyncToRenderWorld` 的種類（相機、光源、sprite、視窗）有對應實體；網格的每實例狀態在以 `MainEntity` 為鍵的 Resource（`bevy_pbr/src/render/mesh.rs:1214-1230`）；文字每幀用暫時實體 | — | Bevy 的同步把 `RenderEntity` 寫回來源實體、由讀者清空來源的待同步清單（`bevy_extract/src/sync_world.rs:93-108`、`:221-258`）——和 DI-KS 13 不相容。至少四個衝突點：對照表放哪（只能放渲染側）、增刪的通知誰清、同步這一步要同時動兩個 World（R1a 的 `RunExtract(schedule, const World&)` 沒有位置）、元件被拿掉的通知 | 每個 extract System 多查一次對照；增刪的讀法（R1a） | R2b 的宣告逐種判（哪些種類要對應實體）；增刪的讀法在 R1a 的宣告留形 |
| 渲染 World 的時間讀來源 | 讀 main 的時間同 Bevy 的方向；channel 回推不照 | — | — | 一個 `ExtractRes` | 照 Bevy 的方向，不照寫回 |
| **不開 render 執行緒**；Extract 只在同一條執行緒的兩個 World 之間 | **偏離**（Bevy 的 `DefaultPlugins` 帶 `PipelinedRenderingPlugin`，`bevy_internal/src/default_plugins.rs:55-57`） | master D27 | C1：SDL_GPU 的 swapchain 只能在建視窗的執行緒；GYO 的 Extract 要兩個 World 都不在 Run（O-12） | — | 平台的限制（C1）＋master 的教訓；不是混搭 |
| `camera`、`render` 都不依賴 `window` | **偏離**（`bevy_camera` 依賴 `bevy_window`，`RenderTarget::Window(WindowRef)`，`camera.rs:896-901`；`bevy_render` 依賴 `bevy_window`） | — | Bevy 的視窗是實體；GYO 的視窗不是實體、沒有多視窗——`WindowRef` 那一半沒有內容；**目標大小那一半有**（Bevy 的 `camera_system` 讀視窗大小，`bevy_render/src/camera.rs:357-363`） | 一個中性的目標 id；目標大小由膠水 System 或作者填（第 3.3 節 (b)） | X1 的宣告照第 3.3 節的三個做法定；多視窗有 Feature 時再看 |
| **`text` 是沒有 World 狀態的純函式庫** | **偏離**：Bevy 的 `Font` 是素材、`FontAtlasSet` 是 main world 的 Resource、圖集是 `Image` 素材、點陣化在 main world 的 `text_system`（`bevy_ui/src/widget/text.rs:389-422`），渲染側只抽字形位置與圖集（`bevy_text/src/lib.rs:120-129`）；`bevy_text` 依賴 `bevy_asset`、`bevy_image`（`Cargo.toml:20-22`） | — | `text` 的狀態（字型集、快取、圖集）放哪、字型檔從哪來 | 狀態由呼叫者持有（第 3.3 節）；圖集上 GPU 走「CPU 像素的貼圖：建立、局部更新」（R2a） | T、U 的宣告定，傾向照 Bevy |
| **`render` 產出 `RenderCommands`、`sdl_gpu` 執行** | **偏離**（`bevy_render` 直接用 wgpu，沒有中間的指令資料） | — | 多一層資料：貼圖、shader、pipeline 以 id 指、`sdl_gpu` 持有真的物件與對照 | `GpuSubmit` 把 `RenderCommands` 轉成 SDL_GPU 的呼叫（主要的膠水）；`render` 定 `RenderCommands` 的格式。R2a 要定的衝突點：GPU 資源的生命週期與 id 誰配；貼圖上傳的路（不是每幀的指令）；swapchain 的大小、格式與裝置能力怎麼回到 `render`（Plugin → Module 的資料）；render target（工具預覽的「draw list 引用 render target 的貼圖」）；讀回 | 使用者的決定（P27）；格式在 R2a 定，R2b、U、K 照用；R2a 的宣告對照一個成熟的指令式繪圖介面（候選：sokol_gfx、bgfx〔知〕，查原始碼後定） |
| 一列只扮一種角色、`world_<x>`／`<x>_render` 分列 | **偏離**（Bevy 一個 Plugin 的 `build` 同時向 main 與 `RenderApp` 登記） | — | GYO 的 O-E3 逐 World 檢查依賴（第 3.3 節） | 每個跨兩邊的 Module 多一兩列 | GYO 已核准的機制的後果；接合點：一個 Bevy Plugin＝GYO 的一組列 |
| extract schedule 寫在讀者的每幀清單、不放第一張不算錯 | **偏離**（Bevy 的 extract 固定在 sub app 的 update 之前） | — | GYO 的每幀清單（O-4） | `render_world` 預設把它插在 `Main` 之前 | GYO 的 O-4；預設排法和 Bevy 同效果 |
| `transform`、`camera` Module；父子在 Kernel；縮放 `Vec3` | 同 Bevy | Unity `Unity.Transforms`、`float` 縮放：不當選項（P26）；flecs `EcsChildOf`（對照） | — | — | 照 Bevy |
| Kernel 的關係的做法（K1～K9） | 同 Bevy 的形；保證的時點、順序、回滾、讀檔有衝突點 | — | 第 6.5 節的表 | KR 定 | KR 的宣告比、交使用者 |
| `GlobalTransform` 不進摘要、判定不讀 | Bevy 的 `GlobalTransform` 也是推導值；Bevy 本身沒有摘要 | Unity Netcode（對照） | GYO 有逐型別摘要（DI-KS 27） | 一個空的雜湊函式 | GYO 特有的機制上的補充 |
| **`gyo.ui` 的節點是自己的文件樹、不是實體** | **偏離**（`bevy_ui` 的節點是實體，階層就是 `ChildOf`）——本線最大的一個非 Bevy 的形 | master；使用者 P17 定的是「ui 留 Engine」，不是節點模型 | `gyo.ui` 是 Editor↔Runtime 的資料契約（ui_editor 讀寫）；改成實體要另定文件與實體的對應 | 照舊：`ui_render` 抽文件樹的繪製資料；改照 Bevy：codec 讀出後生成實體、互動與 layout 改走 System | **U 的宣告比、交使用者**（節點模型是基盤） |
| `Renderer` 的核心、固定 pass、shader ABI | master 的形；Bevy 是 render graph／phase | — | R2b 的 3D 場景與 2D 管線會積在上面 | R2b 定 | R2b 的宣告對照 Bevy 的 render graph／phase，偏離寫衝突點 |
| ImGui 接在我們的 `input`／`window`／`render` 上 | 同 Bevy 生態（`bevy_egui`，使用者 11-11 §3b 寫的參考） | ImGui 官方後端：退掉（Q3） | — | 兩段接法 | 使用者的決定（Q3＝(b)） |
| 不帶 `InputActionMap` | 同 Bevy 本體（動作對應在生態的 leafwing） | Godot `InputMap`、Unity Actions（對照） | — | — | 照 Bevy |
| Plugin 之間的共用（視窗登錄表）放共用處 | Bevy 的 `bevy_winit` 與 `bevy_render` 經 raw-window-handle 元件交接；GYO 的共用處（O-15）是已核准的 GYO 機制 | — | GYO 的視窗不是實體；`sdl_window` 與 `sdl_gpu` 在不同的 World | 一個登錄表型別（在 `sdl` 函式庫） | GYO 已有機制，寫明接合點 |

## 7. audio 的 SDL 輸出後端（要涵蓋的第 5 項）

**前提**：Engine 的（開裝置是平台差異；停的先後是 DI 7 的即時安全，作者不該自己踩）。

**誰建混音器**（audio 頁 5.2 留給本批定；修訂 0 寫「audio 這一側的登記單位建」——會破 DI 2，或等於使用者 11-22 撤掉的 `world_audio`——改掉）：

| 做法 | 先後是否成立 | 代價 |
|---|---|---|
| **(i) 音訊後端的單位自己建** ← 卡片照 L17 判 | 同一個單位先 `Create<Mixer>`、再開裝置、再登記 `OnDestroy`；回呼在共用處解構之前跑（O-15）→ 裝置先停 | 混音器這個型別在共用處的擁有者是後端的單位；不要裝置的程式（測試、audio_editor 的離線處理）自己建 `Mixer`（三行） |
| (ii) 產品建好、經 `ProductDescription::options` 交給後端 | 選項活到共用處之後（O-15「描述的閉包與選項在共用處之後」）→ 成立 | audio 頁第 4 節「混音器放在 Product 的共用處」要改；產品每個都寫一遍 |
| (iii) 後端給一個函式，產品在自己的 `configure` 接上 | 產品的 `configure` 在所有單位之後，後端要在那之後才開裝置 → 要另一個時機 | 多一個時機 |
| `world_audio` 那種 audio 側的登記單位 | — | 使用者 11-22 撤掉（`-world-audio-withdrawn`）；放進 audio 本體破 DI 2 |

(i) 的理由：Bevy 的 `bevy_audio` 也是開裝置的那一方擁有輸出（`audio_output.rs:16-31`）；作者不必管先後（P10 的「後端的登記排在建混音器的單位之後」自動成立）；DI 2 不動。audio 頁 5.2 的原文是「建混音器的是誰……由產品定」：所以**預設 (i)，產品經 `ProductDescription::options` 給了混音器就用產品的**（(ii) 當覆寫；後端不 `Create`，只持產品那一份的參照——選項活到共用處之後，先後照樣成立），不必二選一。audio 頁 5.2 的 `GameAudio` 例子跟著改成只持提交口、`Find<Mixer>` 找混音器（子批 A 改文件，不改 DI）。

**實作**（照 master 修過的做法，偵察 6 節）：`SDL_InitSubSystem(AUDIO)` → `SDL_OpenAudioDeviceStream(預設播放裝置, S16／2ch／48000, 回呼)` → `SDL_ResumeAudioStreamDevice`。回呼在 SDL 的音訊執行緒：每 block 開頭讀一次 `Time::Now()` 當 anchor、`Mixer::Render(span, anchor)`；不丟例外、不配置、不上鎖。後端只持不擁有的參照（DI 7）。停：`SDL_DestroyAudioStream`（返回後不再有回呼）→ `SDL_QuitSubSystem(AUDIO)`。混音器的設定從 `ProductDescription::options` 讀。

**錯誤**：裝置開不了是後端自己的錯誤碼（audio 頁第 4 節第 3 點）。**卡片照 L17 判**：不經 `ProductSetup::Report`（那會讓整個 Product 不建 World——沒有音效卡的機器玩不了不出聲的遊戲）；後端把狀態放在共用處，作者讀了決定。「告訴外面發生了什麼、不替外面決定」。

**`scheduleDelay`**：子批 A 用量測重新推導（Windows 主戰場、macOS、Linux），判定照 §14.1 在 A 的宣告先寫好再量。

**Q1＝B' 之下**：音訊輸出是 `plugins/sdl_audio` 的 AudioOutput（只做 `Configure` 的單位），(i) 照樣成立——它只要求「建混音器與開裝置是同一個單位」。

**「audio 那一側的小批」（clip 來源、音效 loader）**：主會話已另開卡 `gyo2-audio-clip-loader`（使用者在主會話「開 audio 那一側的小批和 Kernel 兩件」），不併進本線。本線只做輸出後端（audio 頁第 4 節）；兩者交界只有「混音器由誰建」，本節已定，那張卡照用。

## 8. 輸入事件的到達時刻（要涵蓋的第 6 項）

**前提**：Engine 的（SDL 的時鐘域和 Engine 的不同，只有 SDL 的實作知道；P11）。

- **做法**（照 master `SdlPlatform.cpp:645-674` 修過的版本）：每次泵事件時取一組配對——依序讀 `Time::Now()`、`SDL_GetTicksNS()`、`Time::Now()`，Engine 的時刻取兩次讀數的中點、記下兩者差（bracket）。事件的 SDL 時刻 `t` 換成 `min(中點 + (t − SDL 讀數), 送達時的 Now())`；`t == 0`（`SDL_PeepEvents` 加的、沒有時間）用送達時刻。無號差以 2^64 取模轉 int64。三次讀取的順序與中點固定在一個吃讀取函式的純函式，測試用假的讀取函式以精確等式檢查（master D34）。
- **保證**（master `platform-sdl.md:74-91`）：(1) 不晚於送達；(2) 同一次泵裡 SDL 時刻非零且不遞減的事件，換算後不遞減；(3) `t == 0` → 送達時刻。**不保證**：全序、跨兩次泵的先後（誤差約 ±bracket/2）。（master 的編號是 (1)(2)(4) 保證、(3) 不保證。）
- **已知限制**（記錄、不修，第 15 節）：入列與泵之間的睡眠讓 macOS 偏移睡眠長度；Linux 兩鐘速率可差約 500 ppm；Windows 事件時刻是毫秒解析度。master 的判定曾在部分平台誤判正確的實作（坑 8、9），子批 W 的判定照 master 修過的容許式寫，結果出來之後不放寬。
- **SDL 的時間不出 SDL 的實作**：`input` 的事件只帶 `Time::TimePoint`。
- **不做的**：事件時刻對到第幾 tick——作者的（Unity 用時刻把事件分到 FixedUpdate，Bevy、Godot 用「幀」）。

**「送進 World 外服務的時刻都是時間基準」要不要成跨 Component 規則**：本線出現第二個例子（輸入）。**建議**子批 W 合併時由圖書管理員登記成 CCR，本卡不自己寫。

## 9. plugins 目錄的建置（要涵蓋的第 2 項）

### 9.1 建置

**前提**：Engine 的（建置圖）。

- **已有的**：Plugin 登記檔、`plugins/<名>` 的 `add_subdirectory`、Module 依賴 Plugin 是 FATAL_ERROR、產品經 `gyo_product_units(... PLUGINS ...)` 選、執行時的登記單位表由清單產生（O-6）。
- **一列＝一個目錄＝一個 target＝一個單位**（`GyoBuild.cmake:63`、`GyoUnitRegistry.cmake:345-355`）。

**`plugins/sdl` 一列還是拆幾列**（P27 留給本卡列選項）：

| 做法 | 內容 | 代價 |
|---|---|---|
| (一) 一列 `sdl` | 一個 Plugin、一個單位，做 Runner、Window、InputEvents、Ime、Gpu、GpuSubmit、AudioOutput | **兩個 World 的產品放不進去**：這一列的依賴是 `input`、`window`（來源側）與 `render_world`（渲染側）的聯集，O-E3 逐 World 檢查（`Product.cpp:175-205`）——放進文件 World 就要求 `render_world` 也在那裡，放進渲染 World 就要求 `input`、`window` 也在那裡。變體「一列＋每個 World 的選項指明角色」：若不寫 `render_world` 的依賴、渲染 World 也列上 `input`、`window`（只是空的資料），O-E3 過得了——但少了 `render_world` 的依賴就少了 O-E3 的保護、同一個單位靠選項分四種角色、選了就連音訊裝置一起開（KeepTool 不出聲也開） |
| **(二) 拆成角色各一列** ← 建議 | 下表；不改登記檔的格式、不改編排層 | 列多；`SDL_Window*` 從 `sdl_window` 交到 `sdl_gpu` 要經共用處（下面） |
| (三) 一列多 target（登記檔改格式） | 照 11-13 原句「一個 `sdl` plugin 內含多個 target」 | 登記檔、`gyo_product_units`、產生的登記單位表都要改；屬 RC-O6（登記檔的欄位不夠用） |

**建議 (二)**：不改編排層或登記檔、每列的依賴都受 O-E3 保護、產品只帶要的那幾件；Plugin 自己內部照角色分，和第 3.3 節 Engine 側的分法一致。**(二) 改了使用者 11-13（P22）的「SDL 是一個 `sdl` plugin、內含多個後端 target；整個 repo 的 SDL 呼叫只在 `sdl` plugin」**——變成「一組 `sdl*` Plugin」、「SDL 呼叫只在 `sdl*` 這一組」；列在第 14 節末「核准時一併確認的」，不同意就選 (三)。系譜：(二)＝修訂 5 的「B-多列」，對照 Bevy 的 `bevy_winit`、`bevy_render`、`bevy_audio` 各一個 crate；(三)＝11-13 原句；需要它的 Feature：KeepTool（兩個 World）、Match（一個都不帶）、只測音訊（只帶 `sdl_audio`）。

| 列（暫名，子批 P 照命名規範定） | 角色 | 內容（P27 的 Plugin 各件） | 依賴 |
|---|---|---|---|
| `sdl` | 函式庫（Plugin、`library_only`） | SDL3 的 FetchContent（`release-3.4.0`＋`gyo-cocoa-ime-composing-keydown.patch`，帶移除條件與「補丁有沒有上」的設定時檢查）；Plugin 之間共用的型別（例：視窗的登錄表，SDL 的視窗 id ↔ `SDL_Window*`）；時鐘配對的純函式 | — |
| `sdl_window` | 來源側（**只放進一個 World，而且那個 World 在主執行緒跑**） | **Runner**（在 `Configure` 裝：泵 → 取樣 → `RunFrame`；等垂直同步發生在 `sdl_gpu` 送出畫面時；選項可以不裝）、**Window**（`SDL_CreateWindow`、`SDL_PollEvent`，主執行緒；寫 `window` 的資料、讀改視窗的請求）、**InputEvents**（`SDL_Event` → `input` 的資料，`SDL_GetTicksNS` 換時間基準，第 8 節；`Main`／`First` 的套用 System）、**Ime**（讀 `TextInputRequest` → `SDL_StartTextInput`／`SDL_SetTextInputArea`；macOS 的輸入法補丁照帶；Windows 的組字 hint 在開窗前設）、剪貼簿（SDL 的剪貼簿文字鏡像到 `window` 的資料、更新事件時刷新；只有「複製」是請求）。事件緩衝有上限、滿了丟棄並計數（帶了 `sdl_window` 卻沒放進任何 World 時沒人讀） | sdl、input、window |
| `sdl_gpu` | 渲染側 | **Gpu**（持有真的 GPU 物件、以 id 對照、上傳、pipeline；shader 的編譯工具）、**GpuSubmit**（渲染 World 最後一個 System：讀 `RenderCommands` → SDL_GPU 畫、送出） | sdl、render、render_world |
| `sdl_audio` | 只做 `Configure` | **AudioOutput**（第 7 節：建混音器或用產品給的、開裝置、回呼 `Mixer::Render`、`OnDestroy` 停） | sdl、audio |
| `imgui` | 函式庫（Plugin、`library_only`） | ImGui 本體＋我們寫的接法（讀 `input`、寫 `TextInputRequest` 與游標、剪貼簿的請求、產出 `RenderCommands`；Q3＝(b)） | input、window、render |
| `world_imgui` | 來源側（文件 World） | widget 那半（P23） | imgui、input、window |
| `imgui_render` | 渲染側 | 繪製那半（P23）：把經 Extract 拿到的 draw list 轉成 `RenderCommands` | imgui、render、render_world |

- **`SDL_Window*` 的交接**：`sdl_window` 在 `Configure` 建視窗、把「視窗 id ↔ `SDL_Window*`」放進共用處（O-15；型別定在 `sdl` 函式庫）；`sdl_gpu` 找它、claim 視窗——在哪一步做（`Configure` 還是 `Build`；`Build` 在替換 World 時會重跑，`Product.cpp:781`）由 R2a 定、重跑要安全；沒帶 `sdl_window` 在 `sdl_gpu` 的 `Configure` 就查得到（`sdl_window` 的 `Configure` 先跑）。兩者都在 Plugin 這一側，`engine/` 看不到 SDL 型別（使用者 11-13 (iii)：Plugin 之間可以露第三方型別）。`sdl_gpu` 不寫 `sdl_window` 的登記檔依賴（它們在不同的 World，寫了就是 O-E3）；產品沒帶 `sdl_window` 時 `sdl_gpu` 在 `Check` 回報自己的錯誤碼。
- **KeepTool 的兩個 World 各帶什麼**：

| World | `units` | 說明 |
|---|---|---|
| `ui`（文件 World） | `entity_ids`、`world_time`、`input`、`window`、`sdl_window`、`world_imgui`（R2b 之後若渲染側的核心要讀相機，加 `transform`、`camera`——R2b 定） | `sdl_window` 的套用 System 把 SDL 事件寫成 `input`；widget 那半寫 draw list 與 `TextInputRequest` |
| `render`（`extractFrom = "ui"`） | `render_world`、`imgui_render`、`sdl_gpu` | extract schedule 讀 draw list；`imgui_render` 產 `RenderCommands`；`sdl_gpu` 畫、送出 |
| 函式庫（跟著依賴來） | `render`、`imgui`、`sdl` | |

  `transform`、`camera` 預設帶、KeepTool 用不到：兩個 World 的 `units` 都不列它們。
- **Match**：不帶任何 SDL 的列（`input`、`window` 預設帶，只是空的資料）。**只測音訊**：`sdl_audio`。
- **SDL3 放 Plugin 自己的目錄**（不放 `third_party/`）：`third_party/CMakeLists.txt` 是共通的建置檔、在登記檔之前加入，放那裡刪不掉 SDL。代價：`plugins/` 底下 FetchContent 的東西會吃到 GYO 的編譯選項（`-ffp-contract=off`、MSVC 的 `/Zc:preprocessor`；`GyoBuild.cmake:35-49`），SDL3 在 MSVC 上會不會出問題，子批 P 在 L1 驗。（FreeType、HarfBuzz 在 `third_party/`，在這些選項之前加入、不吃它們；但 `third_party/` 無條件加入、Module 全部都建，**每個組態（含 Match、三平台 L1）都會抓、都會建這兩個函式庫**——T 的宣告量一次建置時間，FreeType 選用的依賴（壓縮、PNG、反過來依賴 HarfBuzz〔知〕）要關。）
- **只建有產品選的 Plugin**（`GyoBuild.cmake:59-66` 今天是登記檔有列就建）：子批 P 要把 Plugin 的 `add_subdirectory` 移到產品之後或分兩段；`library_only` 的 `sdl` 照依賴遞移帶入；Plugin 自己的測試放在它的目錄、它的列不在時不納入；刪除測試要一併拿掉依賴 SDL 的產品與 examples（KeepTool）。
- **刪掉 SDL 的建置測試**：CI 的組態檢查——在暫存副本拿掉 `sdl*` 的列與目錄（與依賴它們的產品），Engine、所有 Module、headless 的測試照樣 configure、build、ctest，而且 **SDL3 沒有被抓、沒有被建**。守 SDL-2。
- **shader 的編譯**（使用者 11-13，P22：「shader 的編譯步驟留在共通的內容管線，共通管線從已選 plugin 的登記資料找到編譯器」）：HLSL 原始碼在 `render`（P27），編譯工具（DXC、SDL_shadercross、SPIRV-Cross）在 `sdl_gpu`；共通管線怎麼找到它在子批 R2a 照這一句做，要改這一句就在 R2a 的宣告交使用者。
- **AGENTS §3 的守衛時期**：子批 P 只立行為面的（上面的刪除測試）；「`engine/` 不 include `plugins/`、不含 SDL 標頭」的原始碼掃描與「SDL 只被 `plugins/sdl*` PRIVATE 連結」的設定時檢查（名單照登記檔產生、不手寫），在子批 K 的 scenario 通過後立。
- 第三方授權告示（SDL、FreeType〔選 FTL 要標 credit〕、HarfBuzz、ImGui、stb_image，以及字型檔本身——K 的 CJK 字型、Object_Connect 用的 BIZ UDPGothic）：照產品實際連結的產生（`text`、`image` 是函式庫，沒用到的產品不會連進去）；子批 K 的 tools 線打包一起做。

### 9.2 Plugin 與 Module 的接法（B' 之後）

修訂 5 以前的「後端的注入通則」（Module 定後端介面、持有者型別、`Check` 報缺後端）**跟著 B' 拿掉**（P27：Module 沒有後端介面、注入、持有者型別、缺後端檢查）。剩下的規則：
1. **資料由 Plugin 讀寫**：輸入由 `sdl_window` 的套用 System 寫進來源 World（O-12 套用點「進」）；請求資料（`TextInputRequest`、改視窗）由它在 `Main`／`Last` 讀（晚一幀生效）；`RenderCommands` 由 `sdl_gpu` 的 `GpuSubmit` 在渲染 World 的最後讀（O-12 套用點「出」）。
2. **沒有 SDL 時**：Module 照跑，只是沒有輸入、沒有視窗、沒有人執行 `RenderCommands`——同 Bevy 沒有 winit 時。不是錯誤（Q4，使用者定 (b)）。
3. **兩個 SDL 的實作同時選**：兩個 runner 是 O-E4；兩個單位 `Create` 同一型別是 O-2、O-15 的斷言。
4. **Plugin 之間的共用**：經共用處（O-15），型別定在 `sdl` 函式庫（上面的視窗登錄表）。

**條號的分配**（各子批寫文字，號碼在這裡先定，避免並行時撞號；主會話通知並行的卡）：編排層 **O-E9**（Extract 的來源，第 6.1 節 2）；Module 的 DI 前綴——`window` WN-、`input` IN-、`render`／`render_world` RD-、`transform` TF-、`camera` CM-、`text` TX-、`ui`／`world_ui`／`ui_render` UI-、`image` 照 asset 宣告（RC-IM1 已用 IM-）；SDL 的 Plugin 用 SDL-（整條線的 DI 也是 SDL-），錯誤碼 SDL-E1 起；Kernel 的條號見第 10.1 節。

## 10. 分批與先後（要涵蓋的第 3 項）

### 10.1 總覽（「代號｜簡述 — 模型／檔位」；代號 `v8-arch-gyo2-sdl-<x>`）

```text
v8-arch-gyo2-sdl（本宣告：統合）── Opus／high；敵對式檢查 Fable／high
├─ R1a extract｜Kernel 的 RunExtract＋編排層 extractFrom（O-E9）＋RunSchedule 的處理 ── Opus／high
├─ R1b extract-replace｜來源被替換時（讀者一起重建／准許換來源，比較後交使用者） ── Opus／high
├─ KR  kernel-relationship｜Kernel 的關係機制＋ChildOf／Children＋連帶刪除＋K1～K9 的選擇（要改條文的交使用者）＋職責清單多一項 ── Opus／high
├─ X1  transform-camera｜transform（Transform、GlobalTransform：沒有父時等於本身）＋camera ── Opus／high
├─ X2  transform-hierarchy｜讀 ChildOf 的階層傳播（等 KR） ── Opus／high
├─ I   image｜image Module＋stb_image adapter（asset 宣告 5.11） ── Opus／high
├─ P   plugins-base｜plugins/ 的登記形、SDL3 的取得、只建選了的 Plugin、刪 SDL 的建置測試 ── Opus／high
├─ W   window-input｜window、input（只有資料）＋sdl_window（Runner、Window、InputEvents、Ime）＋取樣移給 runner＋渲染 World 的時間＋時間戳 ── Opus／high
├─ A   audio-output｜sdl_audio（AudioOutput：建混音器、開裝置、OnDestroy）＋scheduleDelay 重新推導 ── Opus／high
├─ T   text｜text Module（FreeType＋HarfBuzz 在 src/adapters/，Q6） ── Opus／high
├─ R2a render-core｜render 的 RenderCommands 格式與一個 2D pass（貼圖、混合、裁切）、render_world＋sdl_gpu（Gpu、GpuSubmit：裝置、swapchain）、最小的 shader 管線 ── Opus／high
├─ K   keep-tool｜ImGui（兩半、draw list 經 Extract）＋KeepTool＋IME 實機一次 ── Opus／high
├─ R2b render-pipelines｜Renderer、讀相機、合成、渲染 World 的實體對應（逐種判）、3D 場景、疊層、讀 GlobalTransform、Object_Connect 的兩件（動態頂點、固定解析度的縮放）、VFX 預見 ── Opus／high
└─ U   ui｜ui、world_ui、ui_render ── Opus／high
```

先後：
- **馬上可以開**：KR（Kernel）、R1a、R1b（只動 Kernel 與編排層，不依賴 P）、X1（照第 3.3 節 (b) 不等 W；選 (a) 就等 W）、T（Q6 之後 `text` 不碰 SDL；驗收見第 10.2 節，不需要畫）、I（不依賴 SDL；I 定的像素格式、原點、列距要和 R2b 對過，master 坑 17）。
- **KR 之後**：X2（傳播讀 `ChildOf`）。KR 帶著要交使用者的題（第 6.5 節），不擋 X1、R2b、K。
- **改 Kernel 的三個子批（KR、R1a、R1b）**：會改同一段同步點的套用（`World.cpp`）與執行（`Executor.cpp`），不只是文件的條號。建議 **R1a 先合**（改動淺：加 `RunExtract`），KR 跟上再合（套用步驟的改動深），R1b 在 R1a 之後。條號先分配：DI-KS 29～34 給 KR、35～37 給 R1a、38～40 給 R1b；Kernel 的 RC（目前到 RC 24）25～27 給 KR、28～29 給 R1a、R1b；`KernelErrorCode` 的新值照合併的先後接著編。主會話那一側的 Kernel 卡（`gyo2-kernel-summary-msg`）若要新條號，從 DI-KS 41、RC 30 起；誰後合併誰跟上 gyo2。
- **P 之後**：W、A。
- **R2a** 等 P、W（畫到視窗）。R2a 自己用一個 World 加「出」的 System 驗，不需要 Extract。
- **K** 等 R2a、R1a（draw list 經 Extract）、W。K 接在 R2a 的 2D 那一段（貼圖、混合、裁切）之後（Q3＝(b) 的連帶）。K 是本線唯一使用者點名的 Feature，不等整個 Renderer 與 3D 管線（AGENTS §2：衝突時保 Feature 的方向）。
- **R1b** 不在 K 的路上；R2b 之前完成即可（讀檔換模擬 World 時畫面不重建，是 PvP 與 2D 遊戲的需求）。
- **R2b** 等 R2a、R1a、X1（`GlobalTransform`、相機）、I（貼圖）、T（疊層的文字）；渲染 World 的實體對應在 R2b 逐種判（第 6.1 節 8）。`render` 對 `transform`、`camera` 的依賴（只取型別）R2b 才加，R2a 的 `render` 不等 X1。讀相機的 extract System 由誰登記 R2b 定：放 `render_world`，接渲染 World 的來源 World 就要列 `transform`、`camera`（否則 R-E4）；另立一列，不用相機的產品（KeepTool）不帶——**定的時候回頭驗 KeepTool**。
- **U** 等 T、R2b。
- 會碰同一批檔的：P、W、A、K 都加 Plugin 登記列；T、I 都改 `config/modules.csv` 與 `third_party/CMakeLists.txt`（衝突小，後合併的一方 merge 跟上）；W 改 world-time 頁（取樣、渲染 World 的時間），R1a 不碰時間；KR、R1a、R1b 改 Kernel 頁與 Kernel 的程式（先後見上），R1a、R1b 另改編排層頁。
- **批次先後最終由主會話照 L17 排**（K 等於 `-keeptool` 那一批併進本線，主會話確認）。

### 10.2 各子批

| 子批 | 範圍 | 上段／下段（navigator） | 實作者／審查者／驗證者 | 驗收的骨幹 | 突變清單的敵對式檢查（§14.2） |
|---|---|---|---|---|---|
| R1a | 第 6.1 節 1～3、5、6；第 8 點「增刪的讀法」留形 | 上段：Kernel、編排層的契約 | Opus／high ×3；宣告送 Fable／high | 唯讀（`Extract<Write<…>>` 編譯失敗照舊）、O-E9、清單與 `RunSchedule`；不需要 GPU | 有（DI-KS 13 的守衛） |
| R1b | 第 6.1 節 4 | 上段：DI-KS 13、26、RC-O13 | Opus／high ×3；宣告送 Fable／high | 來源替換後讀新的、`BuildNewFirst` 的重設時點、換來源時對照整批作廢、`DropOldFirst` | 有 |
| KR | 第 6.5 節（需求 1～5、K1～K9 的選擇、邊界統合宣告的職責清單） | 上段：Kernel 的公開介面與職責清單 | Opus／high ×3；宣告送 Fable／high | 選定之後寫；至少含第 6.5 節的例子（刪父連帶刪子、換父、回滾讓預測刪掉的子回來、深的樹）與 K6 的三種回滾情形 | 有（DI-KS 的守衛） |
| X1、X2 | 第 6.4 節 Transform、相機（X1：含目標大小的做法、縮放模式、視區、螢幕↔世界的換算的欄位；預設帶的 Module 增加後 Registry 指紋與 golden 的影響）、階層（X2） | 上段：基盤（複製、回滾、權威登記會積在上面） | Opus／high ×3；宣告送 Fable／high | 傳播（沒有 `ChildOf`＝`Transform` 本身、有＝相乘）、第一幀與放回後的重算、`GlobalTransform` 不在回滾的登記裡、相機的投影 | 有（TF-、CM- 的守衛） |
| I | asset 宣告 5.11、5.4 | 上段：資料契約（`"image"` 型別） | Opus／high ×3 | 照 asset 宣告（PNG 的 golden、`STBI_*` 的設定、拒絕的輸入） | 有（受理判定） |
| P | 第 9.1 節 | 上段：建置圖 | Opus／high ×3 | 刪 SDL 的建置測試（SDL3 不被抓、不被建）；只建選了的 Plugin；MSVC 上的編譯選項；L1 三平台 | 有（刪除測試是 SDL-2 的守衛） |
| W | `input`、`window`（只有資料，P27）、`sdl_window`（Runner 在 `Configure` 裝，O-3、可以不裝；Window；InputEvents 的套用 System；Ime 讀 `TextInputRequest`）、取樣移給 runner（world-time 第 3 節、WT-2、WT-6 (2) 的文字照交換點宣告 5.5 改；等待零件在哪）、渲染 World 的時間（第 6.1 節 7）、時間戳、請求資料（文字輸入、候選窗、改視窗、游標形狀、剪貼簿；後兩者是 Q3＝(b) 的連帶） | 上段：公開介面、時間 | Opus／high ×3；宣告送 Fable／high | 時間戳三條保證的測試（假讀取函式、精確等式）；SDL dummy 驅動下的泵與事件；runner 跑 `examples` 的一個視窗程式 | 有 |
| A | 第 7 節（`sdl_audio`） | 上段：audio DI 7 | Opus／high ×3 | `OnDestroy` 的先後（裝置先停）；產品給的混音器（覆寫）；SDL `disk`／`dummy` 驅動；`scheduleDelay` 的量測（判定先宣告） | 有（DI 7 的守衛） |
| T | `text`（介面：字型、量字、換行、排版結果＝字形＋位置＋原文範圍、點陣圖；換行、fallback 字體、字形快取／圖集在 `text` 的 `src/`；狀態由呼叫者持有）；FreeType 在 `src/adapters/freetype/`、HarfBuzz 在 `src/adapters/harfbuzz/`（預設不用 hb-ft），`third_party/freetype`、`harfbuzz` 只被兩個 adapter 私有連結；薄起步可以先做 FreeType、HarfBuzz 的 adapter 定形（核准時確認）；釘版本與下載來源；建置時間的量測（Q6） | 上段：公開介面 | Opus／high ×3 | 量字的精確值；日文換行（禁則照 T 或 `gyo.ui` 的 layout 定）；fallback 換字型；同一個字不重複進圖集；字形對回原文的範圍；點陣圖的 golden；上下顛倒的坑（master 坑 17：在 UV 轉換的單一邊界修，那半在 R2b） | 有（受理判定） |
| R2a | 裝置在哪一步建（`sdl_gpu` 的 `Configure`，讓替換 World 時 `Build` 重跑不重建裝置）；`render` 的 `RenderCommands` 格式（P27：貼圖 id、shader id、頂點、裁切框、混合模式、實例數等）與一個 2D pass（貼圖、混合、裁切）、`render_world`、`sdl_gpu`（Gpu：裝置、swapchain、id 對照、上傳、pipeline；GpuSubmit）、**CPU 像素的貼圖：建立與局部更新、id 誰配**（K 的 ImGui 字型圖集、U 的字形圖集都用）、`GpuSubmit` 怎麼排在最後（靠 `render_world` 定的 schedule 或明寫的先後，不靠登記順序）、視窗登錄表的交接、最小的 shader 管線（HLSL 在 `render`、host 工具在 `sdl_gpu`、ABI、共通管線找編譯器） | 上段：公開介面 | Opus／high ×3 | Linux Lavapipe 上的像素比對（比確切顏色，master 坑 18）；macOS Metallib 離線編譯；幀時間的預算先宣告 | 有 |
| K | `imgui`、`world_imgui`、`imgui_render`（兩段接法：平台＝讀 `input`、鍵碼對應、寫請求資料；繪製＝draw list → `RenderCommands`）；draw list 經 Extract；全域 context 的例外（第 6.2 節，查版本寫清範圍）；KeepTool 的登記（`keep_tool`；位置 `tools/keep_tool/`，資產若有放 `assets/keep_tool/`——頂層 `tools/` 已在 gyo2（`.gitkeep`，HANDOFF `v8-arch-gyo2-object-connect-dirs-now`），K 建 `tools/keep_tool/`、拿掉 `tools/.gitkeep`；建置層的 `tools/<工具>`（`GyoTools.cmake:25`）不受「`apps/` 改 `games/`」影響）；ImGui 的字型照 Q6 的預設用 ImGui 自帶的點陣化，要用 imgui_freetype 是 AGENTS §6 的例外（FreeType 會出現在第二處），到時交使用者；組字的畫法（ImGui 不畫組字）；CJK 字型；tools 線打包與授權告示；第 9.1 節延後的實作方式守衛 | 上段：使用者指定的 Feature | Opus／high ×3 | tools 線打包；IME 實機按一次（CI 只驗文字事件的路徑）——「tools 線不需要實機證據」的現行規則要不要改，交 K 的宣告 | 有 |
| R2b | `Renderer`（對照 Bevy 的 render graph／phase）、讀相機、合成、渲染 World 的實體對應（逐種判、四個衝突點）、可畫元件的來源側單位、3D 場景、疊層（sprite、文字的繪製）、讀 `GlobalTransform`、R7、Object_Connect 的兩件（來源側每幀給頂點的 2D 形狀＋Extract、`RenderCommands` 補三角形帶與不貼圖；照相機的視區畫、清黑邊，縮放模式與換算是 X1 留的相機欄位）；疊層與 UI 之間的先後（游標在 HUD 文字之上） | 上段：公開介面 | Opus／high ×3 | 像素比對；動態頂點每幀換內容不漏畫、不累積；視窗大小改變時固定解析度的畫面照選的縮放方式；縮放後滑鼠位置換回遊戲座標正確；來源實體刪掉後對應的實體被刪、換來源後對照清空；幀時間的預算先宣告 | 有 |
| U | `ui`、`world_ui`、`ui_render` | 上段：資料契約（Editor↔Runtime） | Opus／high ×3 | `gyo.ui` v1／v2 的資料契約測試；文字框的組字 | 有（受理判定） |

- 敵對式檢查（navigator：作者 Opus → 檢查者用別的、不小的模型 → Fable）：每個子批的宣告、以及表裡標「有」的突變清單，都送 **Fable／high**。Fable 用不到時，照 navigator「同じモデルで行い、報告に書く」。
- 檔位：照使用者 11-19「xhigh 只在需要調查時用」，全部 high；W 的時間戳若同一個判定兩次不成立，照 §14 停下回報，不自己升檔。
- ultracode：不用（本統合已經拆開）。

## 11. Architecture Delta（AGENTS §3 的七項，整條線寫一次；各子批只寫差分）

1. **Feature**：KeepTool（P14）、2D 遊戲的移植（P24）；之後的工具與 PvP Client；Match 不受影響。
2. **現在的問題**：gyo2 沒有視窗、輸入、畫面、文字、UI、音訊輸出、Transform、相機，Kernel 沒有實體之間的關係；master 的 platform 三種角色混在一起，被 P1 退掉；Extract 的執行在第 4 批拿掉。
3. **變的邊界**：新的列（第 3.3 節與第 9.1 節：`transform`、`camera`、`input`、`window`、`render`、`render_world`、可畫元件的來源側單位、`image`、`text`、`ui`、`world_ui`、`ui_render`、`sdl*`、`imgui`、`world_imgui`、`imgui_render`）；頂層 `plugins/` 的第一批內容（`sdl*`，P27）；`render` 的 `RenderCommands` 格式（R2a）；只建選了的 Plugin（`GyoBuild.cmake`）；Kernel 的 `RunExtract`；Kernel 的關係機制與 `ChildOf`／`Children`、DI-KS 15 多一項「關係」（KR，使用者已同意）；**KR 可能改 DI-KS 9、11、12、18（第 6.5 節 K1～K7），每一條在 KR 的宣告交使用者**；Kernel 的 RC 16 已觸發、使用者已答（P26），RC 18 可能觸發；編排層的 `extractFrom` 與 O-E9；來源替換（R1b 交使用者，會改 DI-KS 13、DI-KS 26、RC-O13）；world_time 的取樣移給 runner（WT-6 (2)）；audio 頁 5.2 的例子；shader 編譯器的登記（R2a）。
4. **影響的 Module／Product／Tool**：kernel（KR、R1a、R1b）、邊界統合宣告的職責清單（KR）、回滾 Module（R-2 前提 1 裡 `Children` 屬哪一類，K6）、讀檔的說明頁（K7）、網路 Module（`ChildOf` 的複製與編號，K8）、orchestration、world_time、audio（只改說明頁的例子，DI 2 不動）、建置層；KeepTool（新 tool）。
5. **依賴方向**：新 Module 只依賴 core、ecs、orchestration 與登記檔列的 Module；渲染側的單位只依賴函式庫與同一 World 的渲染側單位；SDL 只被 `plugins/sdl*` 連（P27）；依賴只有 `plugins/sdl*` → `engine/modules/*`；Engine 不依賴 Plugin。
6. **Ownership**：新 Module 的 owner 是架構（Engine）；`plugins/sdl*` 的 owner 是架構；KeepTool 的 owner 在 K 定。
7. **為什麼沒有更小的改法**：不帶 platform（P1）就必須把它的功能各自找家；KeepTool 需要視窗、輸入、畫面同時存在；Extract 不做執行，render 就只能回到 master 的「產品自己填佇列」（為舊形狀找位置）；2D 與 3D 都要一份姿態與相機（P24、P25）；父子照 Bevy 在 Kernel（P26）；兩個 World 的產品照現在的依賴檢查，單位必須分角色（第 3.3 節）。

## 12. Design Invariants 的草案（整條線；各 Module 的 DI 在各子批寫）

**放哪**：新頁 `docs/architecture/target/v2/sdl.zh-Hant.md`（`plugins/sdl*` 的 owner 文件），由子批 P 開，各子批補。各 Module 的 DI 在各自的頁（`target/v2/window.zh-Hant.md` 等，跟著子批開）。

- **SDL-1 SDL 只在 `plugins/sdl*`**：SDL 的型別與呼叫只出現在 `plugins/sdl*`；`engine/` 底下沒有 SDL（P27）。守衛：子批 K 的 scenario 通過後立（第 9.1 節）。
- **SDL-2 沒有 SDL 也成立**：拿掉 SDL 的 Plugin（與登記檔的列），Engine、所有 Module、headless 的測試仍 configure、build、通過，SDL3 不被抓、不被建。守衛：子批 P 的刪除建置測試。
- **SDL-3 一條時間線**：輸入事件的時刻、送進混音器的時刻、runner 的取樣都是 `Time::TimePoint`；SDL 的時刻不出 SDL 的實作。守衛：第 8 節的測試；審查。
- **SDL-4 主執行緒**：視窗、事件泵、swapchain 的取得與送出只在建視窗的執行緒；由 runner 安排、SDL 的實作在進入點斷言（`SDL_IsMainThread`），不由 Kernel 檢查（P19）。
- **SDL-5 渲染只讀**：受 DI-KS 13 約束（全文只在 Kernel 頁）。

**Replacement Criteria**（整條線）：
- RC-SDL 1：要第二個非 SDL 的視窗或 GPU 實作時，某個 Module 的資料（例：`RenderCommands`、`InputFrame`）露出 SDL 才有的概念（換不掉）→ 資料切錯了，重切（master `platform-sdl.md` RC 1、2）。
- RC-SDL 2：事件泵必須離開主執行緒，而 SDL 的主執行緒規則滿足不了 → 換掉有視窗的 runner 的形（master RC 6）。
- RC-SDL 3：保留式的實體對應（R6）保不住 DI-KS 13 的唯讀（例：增刪的通知非由讀者清空來源不可、對應表非寫回來源不可）→ 不放寬唯讀，交使用者決定。
- RC-SDL 4：保持 SDL-3 需要逐事件取樣或睡眠偵測 → 換掉時間戳的換算（master RC 5）。
- 各 Module 的 Replacement Criteria 在各子批寫（例：RC-TF：深的父子階層或大量實體讓傳播超過 R2b 宣告的幀時間預算，計測）。

## 13. 卡片照 L17 判的（不是使用者的話；可以推翻）

| # | 判了什麼 | 理由 |
|---|---|---|
| L-1 | 列的名字（`input`、`window`、`render`、`render_world`、`image`、`text`、`ui`、`world_ui`、`ui_render`、`sdl*`、`imgui`、`world_imgui`、`imgui_render`）與 `world_<x>`／`<x>_render` 的命名規則 | 命名規範 N7／N8；gyo2 的 `world_asset` 慣例、Bevy 的 `bevy_ui_render`。`transform`、`camera` 的名字照使用者 P25 的詞 |
| L-2 | 列的四種角色、只做 `Configure` 的單位不放進任何 World、多 World 的產品明列 `units` | 第 3.3 節（修訂 1 的 blocker） |
| L-3 | `transform`、`camera`、`input`、`window` `default=yes`；渲染側、UI、物理 `default=no`；`window`、`camera` 不依賴別的列（相機的目標大小交 X1） | 第 3.3 節；主會話的傾向；Q4 |
| L-4 | `window` 不依賴 `input`、`camera` 不依賴 `window`、`render` 不依賴 `window` | 第 3.3 節（依賴沒有內容） |
| L-5 | 不帶檔案對話框、log 轉接、`PlatformName`、`InputActionMap`、手把 | 第 3 節各列。（不帶 SDL_image 是使用者 11-21 定的，P20；不帶 `native/` 來自 P21、P22；不帶 `SDL_main` 來自 P18；不帶 SDL_Renderer 來自 P22、P23——都不是卡片的判斷） |
| L-6 | 輸入經「輸入服務＋套用點」進 World（用 `Message` 還是 Resource 照 `gyo2-kernel-summary-msg`） | 第 4.3 節、第 6.3 節 |
| L-7 | Extract：不放第一張不算錯、`render_world` 預設把它插在 `Main` 之前；渲染 World 不帶 world_time（只對 5.5（二））；來源替換傾向「准許換來源」（R1b 交使用者） | 第 6.1 節 |
| L-8 | 有視窗的 runner 由 SDL 視窗的實作在 `Configure` 裝；runner 共用的部分（取樣交給 world_time 的交換函式、等待零件）放 world_time 或另一個中性的單位（W 定），沒畫面的 runner 也用。**不放編排層**：O-4 寫明編排層不認得時間，放那裡是職責清單多一項（RC-O5），要交使用者 | 使用者 11-11「runner 由 plugin 安裝」；時間設計第 9 節 (a) 標「我的套用」 |
| L-9 | 混音器預設由音訊後端的單位建、產品可經選項覆寫；裝置開不了不經 `ProductSetup::Report` | 第 7 節 |
| L-10 | `plugins/sdl` 拆成角色各一列（`sdl`、`sdl_window`、`sdl_gpu`、`sdl_audio`；`sdl_ttf` 因 Q6 拿掉）；視窗登錄表經共用處交接；條號的分配 | 第 9.1、9.2 節；P27 留給本卡的選擇 |
| L-11 | `GlobalTransform` 的雜湊函式什麼都不寫、判定不讀它；；Kernel 的關係拆成先行的子批 KR；X 拆 X1／X2 | 第 6.4、6.5 節 |
| L-12 | `sdl_window` 只放進一個在主執行緒的 World；第二個 World 要輸入時作者轉，或 W 把 InputEvents 的套用拆成另一列（W 定）；事件緩衝有上限；`sdl_gpu` 在 `Check` 回報沒帶 `sdl_window`；「改視窗的請求」是照 P27 的同一原則推的（原文只有輸入法兩項）；輸入的中性邏輯留在 `input` 做成函式 | 第 9.1 節、第 6.6 節 |
| L-13 | 子批的先後與改 Kernel 的三個子批的合併先後、條號的分配（建議，主會話排） | 第 7 節、第 10.1 節 |
| L-14 | 第 6.6 節的處置欄（哪些照 Bevy、哪些是有理由的偏離） | P26 的判法；偏離的都寫了衝突點與膠水量 |
| L-15 | 渲染 World 的實體對應照 Bevy 0.15 起分三種、R2b 逐種判（修訂 2 以前的「每輪全部重抽」拿掉） | 第 6.1 節 8；P26 |
| L-16 | 縮放照 Bevy 用 `Vec3`，不再交使用者 | 第 6.4 節；P26 |

## 14. 要使用者定的

### Q1：SDL 的 adapter 放哪 —— **已定：B'**（使用者 2026-11-23，side chat，P27；HANDOFF `v8-arch-gyo2-sdl-q1-b-prime`）

**原話**（依序）：對 A／A'／B 的比較「那我們就不做adapters，做一個簡單的介面，直接對接sdl 如何？」；釐清「不不不，就沒有Module，只用plugin」；聽到只用 Plugin 會卡住（Module 不能依賴 Plugin，ui／網路／回滾拿不到輸入型別；換 SDL 產品要改）「卡住的地方，好難受，就只有B方案了麼。」；問 IME 怎麼處理、看完放進現有結構的圖後問「render呢？」；對最後的形「可以，轉給任務卡」。

**比過的選項**：A（SDL 當外部組件、adapter 在 Module）／A'（使用者 11-18 的想法）／B（每個 Module 定後端介面＋Plugin 注入，修訂 5 以前第 9.2 節那套）／「直接對接、不做 adapters」（＝A 去掉目錄；SDL 進 `engine/`、推翻 11-16）／「只用 Plugin、沒有 Module」（Module 不能依賴 Plugin → ui、網路、回滾拿不到輸入型別；換 SDL 產品要改）／**B'** ←選。render 先被 side chat 塞進 Plugin，使用者問「render呢？」後改成 render Module 產 `RenderCommands`、Plugin 執行。

**定下的形**：第 3.3、9.1 節（Module 只有資料〔卡片的讀法：沒有 SDL、沒有後端介面；中性的函式可以有，見第 14 節末「核准時一併確認的」〕；SDL 的呼叫全在 `plugins/sdl*`；依賴只有 `plugins/sdl*` → `engine/modules/*`；遊戲到 OS 的操作用請求資料、Plugin 每幀讀、晚一幀生效）。

**理由**（side chat 給的，使用者沒另外說）：卡點只有一個根——Engine 的 Module（ui、網路、回滾）要知道輸入長什麼樣、不能依賴 Plugin；B' 只用資料型別解它，不要 B 的注入機制。SDL 完全在 Engine 外（合 11-16「SDL 是 Plugin、不要一坨子系統」）；換 SDL 只重寫 Plugin（守 11-13「只改對接的那一塊」）；`input` 同 Bevy 形（`bevy_input` 只是資料、`bevy_winit` 產生）。和 Bevy 不同處：`bevy_render` 直接用 wgpu，GYO 多一份 `RenderCommands` 資料，膠水在 `GpuSubmit` 的轉換（第 6.6 節）。

**留給本卡的**：`plugins/sdl` 一列還是拆幾列——第 9.1 節列了三個做法，**建議拆成角色各一列**；這是卡片照 L17 判的，而且**改了使用者 11-13 的「SDL 是一個 `sdl` plugin」**，列進第 14 節末「核准時一併確認的」。

**規則文字**（交風紀委員起草、使用者核准）：`plugins/sdl*` 裡是我們寫的程式，讀寫的是 Engine 的資料型別——AGENTS §6 的判斷句（「誰が保守し、外に見せる Interface は誰のものか」）與同詞異義表第 0 節的 Plugin 定義（「第三方官方維護、接進來用的」）要補一句說明這種「接第三方組件、我們寫的單位」歸 Plugin；「一個外部組件一個 adapter」的數法（SDL 家族一組 Plugin 算一個）寫在架構文件。批次表 SDL 列的「SDL 放 `src/adapters/sdl/`」改成 `plugins/sdl*`。

### Q2：ufbx 歸哪一類 —— **已定：Module 這一側**（使用者 2026-11-23，side chat；HANDOFF `v8-arch-gyo2-sdl-q2-ufbx-stb-module`）

**原話**（依序）：「這個不是定了麼？還是說沒有結論？」→「ufbx 內化到Engine 中，還是放Module。主會話不是有相關討論嗎？」→「就是 ufbx、stb_image 走 Module 這一側。」

**定下的**：ufbx 放 model Module 的 `src/adapters/ufbx/`（model 回來那一批做，本線不實作）；stb_image 照 asset 宣告 5.11（`engine/modules/image/src/adapters/stb_image/`，已核准，這次一併確認）。同詞異義表第 0 節「還沒確認」那一行的「ufbx 歸哪」補上結論（第 16 節）。

**比過的選項**：Module 側（擁有型別的 Module 的 `src/adapters/<組件>/`）←選／Plugin（`plugins/ufbx`；11-13 舊文件的列法；model 依賴 Plugin 會撞「Module 不依賴 Plugin」）／內化進 core（使用者問的）。

**理由**（side chat 查證的，使用者沒另外說）：主會話沒有專門討論過 ufbx（HANDOFF 只有「未定」）；套用兩個既有決定——(1) 11-21 `v8-arch-gyo2-engine-layers`：外部函式庫藏在我們介面後面的放 `modules/`、不放 core（使用者當時更正「json 不是Module嗎？」）；(2) 11-23 `-transform-camera-physics-modules`：core 不認得 World、不能依賴 Module，而 ufbx 是 model 的 loader、接 asset（Module）。AGENTS §6 的判斷句字面成立，不必改規則文字。和 SDL（Q1，進 Plugin）不同：SDL 是有全域狀態、主執行緒規則的平台層，ufbx、stb_image 只是解析器，同 yyjson。

**出處**：使用者在本卡 fork 出的 side chat，2026-11-23。

### Q3：ImGui 的接法 —— **已定：(b) 接在 Engine 的 `input`／`window`／`render` 上，不碰 SDL**（使用者 2026-11-23，side chat；HANDOFF `v8-arch-gyo2-sdl-q3-imgui-on-our-input-render`）

**原話**：「b，這個判斷沒多少變化。」

**定下的**：ImGui Plugin 只依賴 Engine 的 `input`、`window`、`render`（B' 下的資料型 Module）。輸入只讀 `input` 的資料；開關輸入法、候選窗走 `TextInputRequest`；組字中的字自己畫；draw list 經 Extract 進渲染 World，轉成 `RenderCommands` 由 `sdl_gpu` 畫（§3b）。**推翻 11-13 的**「ImGui 官方的 SDL3 平台與 SDL_GPU 繪製後端放在 `sdl` plugin 內的獨立 target」；「ImGui 的全域 context 明列例外」照留（第 6.2 節）。`sdl_imgui` 那一列不需要（第 9.1 節已拿掉）。

**比過的選項**：(a) 官方的 SDL3 平台後端＋SDL_GPU 繪製後端（B' 下是 ImGui Plugin 依賴 `plugins/sdl`、直接用 SDL 型別；OS 事件多一條路；快、少寫程式；KeepTool 驗不到 `input`、`render`；換 SDL 時 ImGui 也要改）／**(b)** ←選（要寫兩段接法；鍵碼對應、游標形狀、剪貼簿也要經 `input`／`window` 的資料；K 要等 render 的 2D 那一段：貼圖、混合、裁切）。

**理由**（side chat 的建議；使用者回「這個判斷沒多少變化」，即修訂 1 起的建議在 B' 下照樣成立）：B' 的形是所有 SDL 呼叫只在 SDL 的 Plugin，(a) 會讓 ImGui 成為第二個碰 SDL 的地方；守住 O-12 的一條路與使用者 11-11 §3b（參考 bevy_egui）；KeepTool 真的驗到 `input`、`render`、Extract。

**連帶**：W 的範圍含游標形狀、剪貼簿的資料；K 接在 R2a 的 2D 那一段之後（第 10 節）。

**出處**：使用者在本卡 fork 出的 side chat，2026-11-23。

### Q4：沒選 SDL 的 Plugin（缺後端）算不算錯 —— **已定：(b) 照 Bevy，不算錯**（使用者 2026-11-23，side chat；HANDOFF `v8-arch-gyo2-sdl-q4-missing-backend-ok`）

**原話**：「（b）不算錯誤，master的match就是沒有視窗的。」

**定下的**：B' 下不加「缺後端」的檢查——`input`、`window` 是資料型 Module、`render` 是函式庫，沒有 SDL 的 Plugin 寫資料就是空的（沒有輸入、沒有視窗資料，`RenderCommands` 沒人畫）。**推翻 11-13（P22）的**「產品用到某能力卻沒選它的後端、或選了兩個，是建置 configure 時的明確錯誤」。連帶：`input`、`window` 預設帶（Match 帶著也無害）；第 6.6 節「要後端的 Module `default=no`」一項消失。「選了兩個」：現在只有一組 SDL 的 Plugin，不會發生；兩個 runner 是 O-E4、同一型別 `Create` 兩次是斷言；真有第二個實作寫同一份資料時再定。渲染側單位的 `default` 照第 3.3 節的角色規則（`render_world` 只放渲染 World，`default=no`）。

**比過的選項**：(a) 照 11-13 另加檢查（登記檔多一欄「Plugin 提供哪些能力」、CMake 比對；要後端的 Module 預設不帶）／**(b)** ←選（不檢查；忘了選 SDL 不報錯，但第一次執行就看得到沒有視窗）。

**理由**：使用者的——master 的 Match 就是沒有視窗的（沒有視窗是正常的組態，不是錯）。side chat 的補充：B' 下這道檢查是唯一還要另外加的機制，拿掉它 B' 就沒有注入或檢查；Bevy 的 `InputPlugin`、`WindowPlugin` 沒有後端也照跑。

**出處**：使用者在本卡 fork 出的 side chat，2026-11-23。（修訂 6 已照 B' 撤回這一題，結果相同；修訂 9 改記成使用者的決定。修訂 6 寫的「`camera` 照 Bevy 依賴 `window`」不是這一題的連帶結論，修訂 9 依窄複查拿掉，第 3.3 節。）

### Q5：要移植的 2D 遊戲 —— **已定：Object_Connect，需求併進 R2b、不另開 R2d**（使用者 2026-11-23，side chat；HANDOFF `v8-arch-gyo2-sdl-q5-object-connect`）

**原話**：「GitHome下有一個Object_Connect」→（看完需求表與結論後）「可以轉過去」

**定下的**：點名的 2D 遊戲是 `~/Code/GitHome/Object_Connect`（C++20＋KamataEngine／DirectX 12 的 2D 血管接續解謎 MVP，固定 1280×720，約 10,757 行，核心不依賴引擎、沒有 ECS；HEAD ca4482c）。本線多出來的只有兩件，併進 R2b：(1) 每幀即時產生的動態頂點；(2) 固定解析度遇到不同視窗大小時的縮放（加黑邊或拉伸）。（放對位置的寫法在第 6.4 節：(1) 是來源側的 2D 形狀＋Extract、`RenderCommands` 那一端 R2a 已有；(2) 的縮放模式與換算是相機的欄位、X1 留形、R2b 實作。仍不開 R2d。）其餘需求已在本線；tilemap、大量 sprite 的合批、圖集、2D 光影、物理 Module 不需要（第 6.4 節）。

**比過的選項**：現在點名、本線加子批 R2d（2D 世界管線）／不點名、跟著移植做／**點名 Object_Connect、需求併進 R2b、不開 R2d** ←選（推出來的需求很輕，幾乎都在 R2a、R2b 範圍）。

**理由**：side chat 從 Object_Connect 的 README 與 `Docs/Architecture.md`（第 10、11、11.1、13 節）推需求；只有兩件超出現有範圍，量小，併進 R2b 比另開子批省。

**出處**：使用者在本卡 fork 出的 side chat，2026-11-23；遊戲原始碼 `~/Code/GitHome/Object_Connect`。

**移植的位置**（移植本身不在本線）：`games/object_connect/`＋`assets/object_connect/`（asset catalog 也在裡面）；兩個目錄已在 gyo2 建好（`.gitkeep`，HANDOFF `v8-arch-gyo2-object-connect-dirs-now`）。建置層仍寫死 master 的 `apps/`，移植第一個遊戲時改成 `games/`（批次表後續）。

### Q6：字型的量字、排版、點陣化放哪 —— **已定：FreeType＋HarfBuzz，放在 `text` Module，不用 SDL_ttf**（使用者 2026-11-23，side chat；HANDOFF `v8-arch-gyo2-sdl-q6-freetype-harfbuzz-module`）

**原話**（依序）：「這個是字體文件？」→「我在想如果用freetype，要整合sdl會如何？」→「stb_truetype和stb_image 是一個套件的？」→「我先問下，對日文的支持如何？」→「主要這個和stb_image 不同，不單單是載入。 如果我用sdl_tff呢？」→「FreeType + HarfBuzz 放Module？」→「還是Plugin？」→「SDL_ttf 放Module 可以嗎？」→「OK，那用FreeType + HarfBuzz 放Module」

**使用者定的**（HANDOFF `-q6-freetype-harfbuzz-module`）：字型的量字、排版、點陣化用 FreeType＋HarfBuzz、放 `text` Module，不用 SDL_ttf；`text` 的介面是字型、量字、換行、排版結果（字形＋位置）、點陣圖；換行、fallback 字體、字形快取在 `text` 的 `src/`；FreeType、HarfBuzz 各一個 `src/adapters/`；拿掉 `plugins/sdl_ttf`；授權告示改成 FreeType（選 FTL 要標 credit）與 HarfBuzz。**推翻 P2／P22 裡「SDL_ttf 歸 Plugin」那一部分**（SDL_ttf 不用了，不是改了歸類）。流程：`gyo.ui` 的 layout 同步呼叫 `text` 量字 → `text` 經 HarfBuzz 排字、經 FreeType 畫進圖集 → `ui_render` 寫 `RenderCommands` → `sdl_gpu` 畫；SDL 只看到貼圖。

**side chat 與卡片補的**（不在 HANDOFF；卡片照 L17 定預設，要走例外的路時再交使用者）：
- **不用 hb-ft**：hb-ft 是 HarfBuzz 提供、接 FreeType 的膠水，照 AGENTS §6 字面不是任何一個組件的 adapter。預設讓兩個 adapter 各自從 `text` 持有的字型位元組建自己的物件（HarfBuzz 用 `hb_face_create`，FreeType 用 `FT_New_Memory_Face`〔知〕），`text` 的 `src/` 只用兩個 adapter 的介面相接。要用 hb-ft（省一份解析、字形度量一致）時，是 §6 的例外，T 的宣告交使用者。
- **ImGui 不走 imgui_freetype**：那會讓 FreeType 出現在 `plugins/imgui`（§6「外部組件只在它的 adapter 裡」的例外）。預設用 ImGui 自帶的點陣化；CJK 字型的品質不夠時，K 的宣告交使用者（imgui_freetype，或 ImGui 改吃 `text` 的圖集）。
- **薄起步**：介面一開始就是「字形＋位置＋每個字形對回原文的範圍」（HarfBuzz 進來後一個字形可以對多個字，文字框的游標、選取、組字底線才不必回頭改）；本線的 T 可以先只實作 FreeType（橫排日文不需要 shaping；FreeType 給字形編號、位移與舊式 kerning〔知〕），HarfBuzz 的 adapter 先定形——**這一點列進第 14 節末「核准時一併確認的」**（使用者定的是兩個都用）。薄起步的限制（沒有 GPOS 的字距、沒有直排的替換字形）在第 15 節。
- FreeType 的物件不能兩條執行緒同時用〔知〕；`text` 的狀態由呼叫者持有（第 3.3 節），T 寫明執行緒的約束。

**比過的選項**：stb_truetype（同 stb 套件、一個檔、最輕；沒有 hinting、小字糊、不能加 HarfBuzz、不耐惡意字體）／SDL_ttf 放 Plugin（Module 要同步量字 → 要注入，B' 的例外；GPU 文字物件會讓畫面兩條路）／FreeType＋HarfBuzz 放 Plugin（同樣要注入；刪了 `gyo.ui` 就量不了字，可刪沒有意義）／SDL_ttf 放 Module（SDL_ttf 連 SDL3，`engine/` 有 SDL、推翻 B'；Module 不能依賴 Plugin `sdl`，要另取一份 SDL3，SDL-2 守不住）／**FreeType＋HarfBuzz 放 Module** ←選。

**理由**（side chat 的說明，使用者最後「OK」）：文字是一整層（量字、換行、fallback、shaping、點陣化），不需要 OS、主執行緒、沒有平台差異，只是計算；B' 把 SDL 放 Plugin 的理由是平台約束，文字沒有。放 Module 時 AGENTS §6 的判斷句字面成立，同 JSON、Q2 的 ufbx／stb_image；Bevy 的 `bevy_text`（parley＋swash）也整套在模組裡。日文：FreeType 的 hinting 讓小字清楚，HarfBuzz 讓之後的比例假名、直排接得上；禁則、fallback、快取不論選哪個都要自己做。代價：換行、fallback、快取自己寫；兩個函式庫三平台 L1 要過。

**出處**：使用者在宣告卡 fork 出的 side chat，2026-11-23。

### 核准時一併確認的（不另開題；修訂 12 依窄複查加）

六題都已定。下面三件是卡片照 L17 判、但碰到使用者說過的話，核准本宣告時請一併看：
1. **`plugins/sdl` 拆成一組 `sdl*`**（第 9.1 節 (二)）：改了使用者 11-13（P22）的「SDL 是一個 `sdl` plugin、內含多個後端 target；整個 repo 的 SDL 呼叫只在 `sdl` plugin」。**使用者 2026-11-23 確認（side chat）**：「第 1 件：沒問題，本身我認為也要分開。」比過 (二) 照角色拆列／(三) 保留一個 `sdl`、改登記檔格式；理由：一列放不進兩個 World（O-E3 逐 World 檢查依賴）、產品只帶要的那幾列（Match 一列都不帶、只測音訊只帶 `sdl_audio`）、和 Bevy 一致（`bevy_winit`〔含 runner，`bevy_winit/src/lib.rs:158`〕、`bevy_render`、`bevy_audio`、`bevy_gilrs` 各一個 crate／Plugin），使用者本身也認為要分開。（side chat 的附帶觀察，不是決定：Bevy 另有 `DefaultPlugins` 把這些包成一組，`bevy_internal/src/default_plugins.rs:14-83`；GYO 沒有「組」，產品逐列寫，等真的不方便時再議。）
2. **B' 的「Module 只有資料」的讀法**（第 3.3 節）：讀成「沒有 SDL、沒有後端介面、注入、持有者型別」；輸入的中性邏輯（雙軌的算法、失焦清鍵）留在 `input` 做成函式，由 `sdl_window` 的套用 System 呼叫（同 Bevy 的 `keyboard_input_system` 在 `bevy_input`）。
3. **Q6 的薄起步**（第 14 節 Q6）：本線的 T 可能只實作 FreeType，HarfBuzz 的 adapter 先定形；使用者定的是兩個都用。

要走才交使用者、現在不問的：`text` 用 hb-ft、ImGui 用 imgui_freetype（AGENTS §6 的例外，T、K 的宣告真的要走時再交）。

## 15. 範囲の限界

- 行動平台（iOS、Android）的入口不支援（不帶 `SDL_main`）。
- IME 只能實機驗（CI 只驗文字事件的路徑）；Windows 的組字（`SDL_HINT_IME_IMPLEMENTED_UI`，整個 process、只讀一次，master 坑 15）在子批 W／K 實機驗。
- GPU 的 CI 證據只有 Linux Lavapipe（software Vulkan）；Windows D3D12 不在 CI；macOS 只做 Metallib 離線編譯。
- macOS `nextDrawable` 的約 1 秒停頓與縮放空窗（master 坑 1、2）在薄起步照舊存在。
- 時間戳的已知限制（第 8 節）。
- master 的 `platform_sdl_concurrency` 在 CI 逾時的原因沒查明（坑 13）；Linux 上 SDL 的 dummy 視訊驅動會開 evdev——子批 W 的 CI 出現時照 §14.1 切分。
- 「替換成另一個實作」在第二個實作出現前只是預留（R1）。
- `text`、`ui`、`world_ui`、`ui_render`、Kernel 的父子、`camera`、R2b 的 3D 場景在本線沒有使用它們的 Feature（KeepTool 不用），只能靠測試驗收；第一個點名的產品回來時才有實際的使用者。
- PvP Client 組法（三）（模擬在別的執行緒）的渲染時間來源、S 與 F 之間的交換，不在本線（第 6.1 節）。
- 2D 世界管線較重的部分（tilemap、大量 sprite 的合批、圖集、2D 光影）不在本線（Q5：Object_Connect 不需要），有需要的遊戲來時再做。
- 文字的薄起步若只做 FreeType：沒有 GPOS 的字距、沒有直排的替換字形（HarfBuzz 進來時補）。
- 預設帶的 Module 增加（`transform`、`camera`、`input`、`window`）對既有測試的 Registry 指紋與 golden 的影響沒有查，X1、W 的宣告各寫一句。
- `GlobalTransform` 不進摘要、判定不讀它（第 6.4 節）：畫面與判定的姿態可能差一幀以內，這是有意的。

## 16. 和其他卡的協調（經主會話）

- **asset（PR #283，已合併）**：圖片 loader 在子批 I（image Module，stb_image）。
- **`gyo2-audio-clip-loader`**（音效 loader 與 clip 來源）：混音器由音訊後端的單位建、產品可經選項覆寫（第 7 節），那張卡照用；點名的使用者多一個 Object_Connect（序奏接循環的 BGM、兩個音效、壓低與淡出，Object_Connect `Docs/Architecture.md:312`、README 行 241～251）。
- **FreeType、HarfBuzz 的下載**：使用者已同意用這兩個（Q6），T 的宣告寫釘哪一版、從哪裡抓（同 I 的 stb_image 照 asset 宣告第 13 節）。
- **主會話（HANDOFF）**：`v8-arch-gyo2-sdl-q1-b-prime` 那一列還寫「拆五列（含 `sdl_ttf`）」「Q4 撤回」「`camera` 依賴 `window`」——是當時的紀錄，三處都被後來改掉（`-q4`、`-q6`、修訂 9、12），請在那一列末補一句指過去。
- **網路 Module（PR #275）**：輸入的時刻給網路的預測用；事件時刻對到 tick、`Transform` 要不要複製是網路那一側的事；`ChildOf` 的複製、實體編號、父比子晚到（K8）和 KR 對。
- **回滾 Module**：`Children` 在 R-2 前提 1 裡屬哪一類、回滾的做法（K6），KR 的宣告和回滾頁一起改。
- **讀檔**：存不存 `Children`、放回時跳不跳維護（K7），KR 的宣告和讀檔的說明頁（`system-contract-page.zh-Hant.md` 的讀檔一節）一起改。
- **`gyo2-kernel-summary-msg`**（Kernel 的 `Summary` 與訊息）：輸入用 `Message` 還是 Resource 照它的結論，**W 開工前要有**（第 6.3 節）；它和 KR 都改 Kernel，誰後合併誰跟上 gyo2。
- **主會話（Kernel）**：KR、R1a、R1b 都改 Kernel，合併先後與條號分配見第 10.1 節；邊界統合宣告的職責清單由 KR 改；O-E9 與各前綴的保留（第 9.2 節），通知並行的卡。
- **`TextInputRequest` 有兩個寫者**（`world_ui` 的文字框、ImGui）：同一個 World 兩者都在時聽誰的，W 的宣告定。
- **圖書管理員**：同詞異義表行 74、124 的 Plugin 例子還寫「SDL_image、SDL_ttf」，`runtime-composition.zh-Hant.md` 行 62、64「SDL_image、SDL_ttf 各自一個 plugin」——兩個都不用了（P20、Q6），統一處理時改；asset 宣告的行 140、994 還寫「SDL 的 adapter、ufbx 歸哪一類未定」，下次整理時改（Q1、Q2 已定）；「送進 World 外服務的時刻都是時間基準」登記成 CCR（第 8 節末）。
- **風紀委員**：Q1（B'，已定）的規則文字（同詞異義表第 0 節、AGENTS §6 判斷句：`plugins/sdl*` 裡是我們寫的程式、讀寫 Engine 的資料）；Q2 不必改規則文字（判斷句字面成立），同詞異義表第 0 節「還沒確認」那一行與第 6 節 ufbx 那一列，主會話已在 gyo2 補上結論（4ece17b4；HANDOFF `v8-arch-gyo2-sdl-q2-ufbx-stb-module`），子批 P 不再改；Q3 推翻 11-13 的「ImGui 官方的 SDL3 平台與 SDL_GPU 繪製後端放在 `sdl` plugin 內的獨立 target」（使用者已定）、Q4（使用者定 (b)）推翻 P22 的「缺後端是建置時的明確錯誤」那一句，runtime-composition §4 的統一處理時一起改。
- **KeepTool**：子批 K 等於 `-keeptool` 那一批，主會話確認是否併進本線。
- **物理 2D、3D**：`physics_2d`、`physics_3d` 兩個 Module，依賴 `transform`，跟著點名的產品回來，不在本線（P24、P25）。

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

## 18. 修訂 1 的窄複查：處理對照

| 項 | 處理 | 改在哪 |
|---|---|---|
| B-1 兩個 World 的產品照依賴表組不起來 | 採用 (b)：列分四種角色（函式庫、來源側、渲染側、後端），後端的單位不放進任何 World，多 World 的產品明列 `units`；KeepTool 兩個 World 的 `units` 寫出來；`render`→`transform` 拿掉 | 第 3.3 節、第 9.1 節 |
| S-1 測試在產品的 `configure` 建持有者和 O-15 相反 | 採用：改成手寫 `RegisteredUnits` 的假後端 Plugin 單位 | 第 9.2 節第 5 點 |
| S-2 Q4 的理由不成立、少一個選項 | 採用：拿掉第二個理由；補 (c) 兩處都查與斷言訊息的代價；建議改成 (c)（不推翻 11-13） | 第 14 節 Q4 |
| S-3 Q3 的表偏向 (b) | 採用：(a) 的登記改成另一列 `sdl_imgui`；補時程一列與對 W、R2a、K 範圍的影響；「OS 事件」列改寫；(b) 的系譜引 §3b 原文 | 第 9.1 節、第 14 節 Q3、第 10.1 節 |
| S-4 B 不是 11-13 的原形 | 採用：Q1 表加「和 11-13 原形的差別」一列；補「一列多 target、改登記檔」的做法與 RC-O6 | 第 9.1 節、第 14 節 Q1 |
| S-5 shader 編譯的一句沒有著落 | 採用：寫進第 9.1 節與 R2a 的範圍 | 第 9.1、10.2 節 |
| S-6 `GlobalTransform` 可以進摘要不成立 | 採用：定成畫面用的推導值、雜湊函式什麼都不寫、判定不讀；四個洞列進 X | 第 6.4 節 |
| S-7 父子不進 Kernel 列成 L17 | 已被使用者 11-23 的決定（P25）取代：寫成使用者的決定，第 4.3 節補 flecs 的對照（**修訂 3 更正**：P25 的那一半是主會話的讀法，使用者改成照 Bevy 放 Kernel，P26、第 19 節） | 第 2 節 P25、第 4.3、6.4 節 |
| S-8 K 的路上兩件不需要的等待 | 採用：R1 拆 R1a／R1b；R2a 只等 P、W；K 等 R1a；Q3 (a) 時 K 不等 2D pass；可畫元件的單位在 R2b 加 | 第 10 節 |
| S-9 Q1 表 GPU device 交接只在 Q3 (a) | 採用：改寫，註明屬性表未查證 | 第 2 節 C3、第 14 節 Q1 |
| S-10 Q2 把 stb_image 又拿出來問 | 採用：Q2 只問 ufbx，stb_image 當既定事實 | 第 14 節 Q2 |
| S-11 L-7 把編排層列成選項 | 採用：拿掉，註明放編排層要交使用者 | 第 13 節 L-8 |
| N-1 extract 預設晚一幀 | 採用：`render_world` 在 `Build` 把 extract 插在 `Main` 之前 | 第 6.1 節 3 |
| N-2 L-4 的歸屬 | 採用：不帶 SDL_image、`native/`、`SDL_main`、SDL_Renderer 標成使用者的決定 | 第 13 節 L-5 |
| N-3 O-E8 與前綴 | 採用：改 O-E9；每個 Module 的 DI 與錯誤碼同一個前綴；主會話通知並行的卡 | 第 6.1、9.2、16 節 |
| N-4 `plugins/` 下的編譯選項 | 採用：寫進第 9.1 節，P 在 L1 驗 | 第 9.1 節 |
| N-5 只建選了的 Plugin 的做法 | 採用：寫進第 9.1 節 | 第 9.1 節 |
| N-6 用真的 `sdl_gpu` 時的依賴與 runner | 採用：第 3.3 節說明；`sdl_window` 可以不裝 runner | 第 3.3、9.1 節 |
| N-7 混音器可以預設＋覆寫 | 採用 | 第 7 節 |
| N-8 `text` 的缺後端規則寫好幾份 | 採用：`text` 給一個找後端的函式 | 第 9.2 節第 3 點 |
| N-9 I 的先後與依賴 | 採用：I 和 R2b 對像素格式；`image` 多半要 `world_asset` | 第 3.3、10.1 節 |
| N-10 Resource 每幀換一次的代價 | 採用：寫進第 6.3 節與 W 的說明頁 | 第 6.3 節 |
| N-11 Q5 兩問合一 | 採用 | 第 14 節 Q5 |
| N-12 ImGui 寫回 context 裡的貼圖資料 | 採用：K 查版本寫清例外的範圍 | 第 6.2 節、第 10.2 節 |
| N-13 A'-p 的建置代價 | 採用：補登記檔的規則也要改 | 第 14 節 Q1 |

## 19. 修訂 3：主會話轉來的更正（`v8-arch-gyo2-hierarchy-bevy-and-primary-reference`）

| 更正 | 改在哪 |
|---|---|
| 父子放進 Kernel（照 Bevy 的關係機制與 `ChildOf`），職責清單多一項 | P26；第 4.2 節 R8、第 4.3 節；新的第 6.5 節與子批 KR（先行、排在 X 之前）；第 11 節 |
| `transform` 只放 `Transform`、`GlobalTransform`、傳播 | 第 3.3、6.4 節；子批 X |
| ECS 的機制以 Bevy 為主參考，偏離要寫衝突點與膠水量 | 新的第 6.6 節逐項檢查。因此改了兩處：渲染 World 的實體由「每輪重抽」改成照 Bevy 0.15 起的保留式（第 6.1 節 8、R6、RC-SDL 3）；縮放照 Bevy 用 `Vec3`，不再交使用者。維持偏離、寫了理由的：輸入放 Resource（暫時，等 Kernel 的選項）、事件時刻保留（使用者 P11）、要後端的 Module `default=no`（登記檔沒有預設組合）、Extract 不寫回（使用者 P7） |

## 20. 修訂 3 的窄複查：處理對照

| 項 | 處理 | 改在哪 |
|---|---|---|
| B-1 回滾放回 `ChildOf` 時 hook 重建不出 `Children` | 採用：寫成 K6（三種情形、`Restore` 只對新增與拿掉的格跑 hook），三條路列出、在 KR 的宣告交使用者；拿掉 lightyear 的誤引；KR 的驗收改成選定後再寫 | 第 6.5 節、第 10.2 節 |
| B-2 Bevy 的關係 hook 和 GYO 的 hook 契約有五處衝突 | 採用：K1～K5 列表；「立即」標成主會話的描述、寫準 Bevy 實際的保證；第 11 節加「KR 可能改 DI-KS 9、11、12、18，各自交使用者」與 RC 16、18 | 第 2 節 P26、第 6.5、11 節 |
| S-1 讀檔：Bevy 存 `Children`、跳過 hook | 採用：K7 兩個選項 | 第 6.5、16 節 |
| S-2 複製：`ChildOf` 比父先到 | 採用：K8（replicon 佔位、lightyear `PendingChildOf`） | 第 6.5、16 節 |
| S-3 三處 Bevy 事實 | 採用：目標不存在是 `warn!` 並拿掉；`on_discard`；lightyear 有自己的收發 | 第 6.5 節 |
| S-4 保留式的描述與衝突點 | 採用：照 Bevy 實際寫（三種並存、只有標了要同步的種類有對應實體）；四個衝突點；6.6 該列改成「方向同、做法偏離」 | 第 6.1 節 8、R6、RC-SDL 3、第 6.6 節 |
| S-5 保留式的連帶 | 採用：R1b 加換來源時對照整批作廢；R1a 的範圍拿掉第 8 點、改成「增刪的讀法留形」；R2b 補範圍與驗收 | 第 6.1 節 4、8；第 10 節 |
| S-6 第 6.6 節漏九項 | 採用：全部補進表（`gyo.ui` 的節點模型交 U 的宣告；不開 render 執行緒；`render`／`camera` 的依賴；一列一角色；extract 在清單；視窗事件；讀檔的 `Children`；Extract 不寫回的素材搬走；雙軌與 `Renderer`） | 第 6.6 節 |
| S-7 `default=no` 的衝突點 | 採用：改成 P22 與第 9.2 節第 3 點；Q4 加照 Bevy 的 (d)、建議 (d) | 第 6.6 節、第 14 節 Q4 |
| S-8 輸入放 Resource 的回頭改 | 採用：寫進膠水量；改成 W 開工前要主會話回答 | 第 6.6、16 節 |
| S-9 第 11、16 節沒改齊 | 採用 | 第 11、16 節 |
| S-10 條號、RC、合併先後、X 等 KR | 採用：DI-KS 29～40 與 RC 25～29 分配、R1a 先合、KR 寫關係的 RC、X 拆 X1／X2 | 第 6.5 節需求 5、第 10 節 |
| N-1 `on_discard` | 採用 | 第 6.5 節 |
| N-2 gilrs 形只對一半 | 採用：寫成接合點 | 第 6.6 節 |
| N-3 混音器「同 Bevy」 | 採用：只有「開裝置的一方擁有輸出」相同 | 第 6.6 節 |
| N-4 `Time<Real>` 的回推 | 採用：統一寫 channel | 第 6.6 節 |
| N-5 會碰同一批檔的那一句 | 採用 | 第 10.1 節 |
| N-6 L-14 的位置、兩件改判 | 採用：L-14 移位、加 L-15、L-16 | 第 13 節 |
| N-7 第 18 節 S-7 列過時 | 採用：加註 | 第 18 節 |
| N-8 UI 的階層三處對齊 | 採用：統一成「U 判，選『節點是實體』時才用」 | 第 4.3、6.5、6.6 節 |
| N-9 P26 漏了接合點那半句 | 採用 | 第 2 節 P26、第 6.6 節 |
| N-10 `StartupCode`、內建元件的登記 | 採用：K9 | 第 6.5 節 |
| N-11 遊戲的例子 | 採用 | 第 6.5 節開頭 |
| N-12 薄起步的邊界 | 採用：需求 4 | 第 6.5 節 |
| N-13 `ChildOf` 被拿掉或換掉時子樹重算 | 採用：X 的洞 (5) | 第 6.4 節 |

## 21. 修訂 6：使用者定 Q1＝B'（side chat 轉來，P27）

| 改了什麼 | 改在哪 |
|---|---|
| Module 只有資料（`input`、`window`）；`render` 產出 `RenderCommands`、HLSL 在 `render`；SDL 的呼叫全在 `plugins/sdl*`；依賴只有 Plugin → Module；遊戲到 OS 用請求資料 | P27；第 3.1、3.2、3.3 節；第 6.2 節 |
| `plugins/sdl` 拆成角色各一列（`sdl`、`sdl_window`、`sdl_gpu`、`sdl_audio`、`sdl_ttf`）——P27 留給本卡的選擇；一列放不進兩個 World（InputEvents 在來源 World、GpuSubmit 在渲染 World）；視窗登錄表經共用處交接 | 第 9.1 節、L-10 |
| 後端的注入通則拿掉；Q4 撤回（沒有缺後端了） | 第 9.2 節、第 14 節 Q4 |
| `input`、`window` `default=yes`；`camera` 照 Bevy 依賴 `window` | 第 3.3 節、第 6.6 節、L-3 |
| `RenderCommands` 這一層和 Bevy 不同（`bevy_render` 直接用 wgpu），膠水在 `GpuSubmit` | 第 6.6 節 |
| Q3 的比較改寫：(a) 讓畫面有兩條路（官方後端繞過 `RenderCommands`），(b) 合 P27 | 第 14 節 Q3 |
| 子批 W、A、T、R2a 的範圍照 Plugin 的各件改寫 | 第 10 節 |

## 22. 修訂 7：使用者定 Q2（side chat 轉來）

| 改了什麼 | 改在哪 |
|---|---|
| ufbx、stb_image 都走 Module 這一側；Q2 記成已定 | 第 14 節 Q2、P2、第 3.2 節 |
| 同詞異義表第 0 節的「還沒確認」補 ufbx 的結論（不改規則文字） | 第 16 節 |

## 23. 修訂 8：使用者定 Q3（side chat 轉來）

| 改了什麼 | 改在哪 |
|---|---|
| ImGui 走 (b)；Q3 記成已定；推翻 11-13 的一句、「全域 context 明列例外」照留 | 第 14 節 Q3、第 16 節 |
| `sdl_imgui` 那一列與 (a) 的分支拿掉 | 第 9.1 節、第 10 節 |
| 剪貼簿、游標形狀的請求資料進 W；剪貼簿從「不帶」改成 `window` 的資料 | 第 3.1 節、第 10.2 節、L-5 |

## 24. 修訂 9：Q4 的決定與修訂 6～8 的窄複查

| 項 | 處理 | 改在哪 |
|---|---|---|
| 使用者定 Q4＝(b) | 記成已定；推翻 11-13 的一句 | 第 14 節 Q4 |
| B-1 `text`／`sdl_ttf` 沒照 B' 重寫 | 採用：列成新的 Q6（建議 (ii) Module 側的 FreeType 或 stb_truetype）；兩列標未定 | 第 3.2、3.3、9.1 節、第 14 節 Q6、第 10 節 T |
| S-1 拆列的理由 | 採用：改成 O-E3 對依賴的聯集；補「一列＋選項」的變體 | 第 3.3 節、第 9.1 節 (一) |
| S-2 (二) 改了 11-13 的一句、§4 三項 | 採用：建議句寫明改了哪一句、核准即同意；系譜、Feature 補進 | 第 9.1 節 |
| S-3 函式庫依賴有單位的列 | 採用 (i)：角色表寫明「只取型別、O-E3 不追、後果」；`render` 的依賴欄補 `transform`、`camera` | 第 3.3 節 |
| S-4 `sdl_window` 一列包四件 | 採用：只放進一個在主執行緒的 World；第二個 World 的輸入交 W；緩衝有上限；退掉「runner 直接寫」的理由改寫 | 第 9.1 節、第 4.3、6.6 節、L-12 |
| S-5 `camera` → `window` 沒內容 | 採用：拿掉依賴、目標用 `camera` 定的中性 id；`WindowPlugin` 的條件改正 | 第 3.3、6.4、6.6 節 |
| S-6 第 6.6 節漏三項 | 採用：輸入的狀態邏輯（留在 `input` 做成函式）、視窗不是實體與輸入法資料、`RenderCommands` 的衝突點與對照對象 | 第 6.6 節 |
| S-7 B' 的殘留九格 | 採用：逐格改 | 第 1 節、P9、P15、P16、性質段、第 5、6.2、6.4 節、L-12 |
| S-8 明列 `units` 時預設帶的 Module | 採用：KeepTool 的表補 `world_time`、`entity_ids`；第 3.3 節寫後果 | 第 3.3、9.1 節 |
| S-9 Q3 未進 HANDOFF | 轉主會話（第 16 節） | 第 16 節 |
| N-1 行號 | 改 `:170`、`:215-219` | 第 3.3 節 |
| N-2 等垂直同步的位置 | 改：發生在 `sdl_gpu` 送出時 | 第 9.1 節 |
| N-3 剪貼簿 | 改：鏡像到 `window` 的資料、只有複製是請求 | 第 9.1 節 |
| N-4 `window` 資料的雜湊與換 World | 改 | 第 3.3 節 |
| N-5 `TextInputRequest` 兩個寫者 | 交 W | 第 16 節 |
| N-6 裝置在哪一步建 | R2a 的範圍：`sdl_gpu` 的 `Configure` | 第 10.2 節 |
| N-7 `sdl_gpu` 的檢查是卡片加的 | 列進 L-12 | 第 13 節 |
| N-8「改視窗」是卡片推的 | 列進 L-12 | 第 13 節 |
| N-9 過時的字 | 改 | 文首、P3、P19、第 3.1、6.4 節 |
| N-10 asset 宣告的過時行 | 請圖書管理員 | 第 16 節 |

## 25. 修訂 10：使用者定 Q5（side chat 轉來）

| 改了什麼 | 改在哪 |
|---|---|
| 2D 遊戲＝Object_Connect；需求併進 R2b、不開 R2d；Q5 記成已定 | 第 14 節 Q5、第 4.1 節、第 6.4 節 |
| R2b 加兩件：`RenderCommands` 的每幀動態頂點、固定解析度的縮放 | 第 10.1、10.2 節 |
| 2D 世界管線較重的部分列進範囲の限界 | 第 15 節 |

## 26. 修訂 11：使用者定 Q6（side chat 轉來）

| 改了什麼 | 改在哪 |
|---|---|
| 字型用 FreeType＋HarfBuzz、放 `text` 的 `src/adapters/`；不用 SDL_ttf；Q6 記成已定；推翻 P2／P22 的 SDL_ttf 那一部分 | 第 14 節 Q6、第 3.2、3.3 節 |
| `plugins/sdl_ttf` 那一列拿掉；授權告示換成 FreeType、HarfBuzz | 第 3.3、9.1 節、L-10 |
| 子批 T 不再等 P，範圍照 Q6 寫 | 第 10.1、10.2 節 |

## 27. 修訂 12：修訂 9～11 的窄複查

| 項 | 處理 | 改在哪 |
|---|---|---|
| B-1 Q6 混進 HANDOFF 沒有的三句、撞 AGENTS §6 | 採用：分開「使用者定的」與「side chat／卡片補的」；預設不用 hb-ft、ImGui 不走 imgui_freetype（要走時交使用者）；薄起步列進核准時確認 | 第 14 節 Q6、末尾的確認清單、第 10.2 節 T、K |
| S-1 `text` 的接法沒落到表上 | 採用：`ui` 依賴 `text`；`text` 是純函式庫、狀態由呼叫者持有、傾向照 Bevy；R2a 補「CPU 像素的貼圖：建立與局部更新」；第 6.6 節補 `text` 一列 | 第 3.2、3.3、6.6 節、R2a |
| S-2 T 的驗收是舊介面 | 採用：驗收改寫；介面加「字形對回原文的範圍」；第 5 節跟著改；薄起步的限制進第 15 節 | 第 3.3、5、10.2、15 節 |
| S-3 相機的目標大小誰給 | 採用：列給 X1 的宣告三個做法、傾向 (b)；第 6.6 節膠水量補 | 第 3.3、6.6、10 節 |
| S-4 Q5 的兩件放錯端 | 採用：(1) 改成來源側的 2D 形狀＋Extract；(2) 縮放模式與換算是 X1 留的相機欄位、R2b 實作；驗收加滑鼠座標換算；仍不開 R2d | 第 6.4 節、Q5、R2b |
| S-5 R2b 之後 KeepTool 會撞 R-E4 | 採用：R2b 定讀相機的 extract System 由誰登記、回頭驗 KeepTool | 第 10.1 節、第 9.1 節 KeepTool 的表 |
| S-6 (二) 的理由說過頭、確認的位置 | 採用：(一) 的變體照實寫；第 14 節末加「核准時一併確認的」，Q1、文首指過去 | 第 9.1 節、第 14 節 |
| S-7 殘留八格 | 採用：L-3、第 9.2 節、第 16 節（Q4 是決定不是撤回）、MSVC 那句、會碰同一批檔、Q5 的目錄、核心一句、第 5 節 | 各處 |
| S-8 第 16 節沒跟上 | 採用：同詞異義表與 runtime-composition 的 SDL_image／SDL_ttf、FreeType／HarfBuzz 的下載、Object_Connect 的音訊、`tools/` 不受 `apps/` 影響、HANDOFF q1 那一列 | 第 16 節、K |
| S-9 「只有資料」兩種寫法 | 採用：統一成「只有資料與中性的邏輯」；讀法列進核准時確認 | 第 3.3、14 節 |
| N-1 行號 | 改 `:175-205` | 第 3.3 節 |
| N-2 `sdl_gpu` 在哪一步做事 | 改：R2a 定、重跑要安全、沒帶 `sdl_window` 在 `Configure` 就查 | 第 9.1 節 |
| N-3 GpuSubmit 怎麼排在最後 | 進 R2a 的範圍 | 第 10.2 節 |
| N-4 KeepTool 的表 | 併進同一格 | 第 9.1 節 |
| N-5 R2b 等 T、`render` 的依賴何時加 | 改 | 第 10.1 節 |
| N-6 授權告示照產品、字型檔的授權 | 改 | 第 9.1 節 |
| N-7 Object_Connect 的小漏 | 補：失焦、滾輪、隱藏 OS 游標、游標在 HUD 之上 | 第 6.4 節、`window` 一列、R2b |
| N-8 SDL_ttf 是不用了、不是改歸類 | 補註 | P2、Q6 |
| N-9 FreeType、HarfBuzz 每個組態都建 | 補：T 量建置時間、關選用依賴 | 第 9.1 節 |
| N-10 剪貼簿是鏡像 | 改；鏡像有大小上限 | 第 5 節、`window` 一列 |
| N-11 Registry 指紋與 golden | 進 X1、W 與第 15 節 | 第 10.2、15 節 |
| N-12 `window` → `input` 沒內容 | 拿掉這個依賴 | 第 3.3 節 |
