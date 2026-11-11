# 第三方方案逐列評估（`v8-arch-third-party-evaluation`）

更新：2026-11-10。Owner：架構。評估者：主會話（Fable 5.1／high；卡片指定 Opus，主會話模型由使用者設定）。對抗檢查：第 5 節。
**狀態：評估完成，停下等使用者決定（第 4 節）。** 只寫文件：不改程式、不加依賴、不改契約。

## 0. 方法與讀法

- **問題**（使用者 2026-11-10）：「Engine 裡有大量重複的輪子，是否需要自己實作。」
- **判定以 Feature 為準**（AGENTS §2 Derivation Order）：已排或已點名的 Feature 需要它，才算需要；只因「有這個函式庫」不算。已點名的 Feature：big-map（區塊化地圖、broadphase 已有）、場景線（互動物件、門的動態碰撞、邏輯連線、物品）、AI 小怪與 Boss、技能（投射物、範圍效果）、寵物（裝備式表現）、pv10。
- **使用者的立場**：基盤的協定不能等壓力出現才改（事後補代價最高）；功能層的擴充才等壓力。本文把每列標成「基盤」或「功能層」，基盤的列即使現在沒有 Feature 也給方向。
- **為什麼每列寫「限制」**（使用者 2026-11-10，起因是 SDL_GPU）：GYO 採用 SDL_GPU 後碰到的限制——Metal 後端在不阻塞模式下仍無條件呼叫 `[CAMetalLayer nextDrawable]`、最多阻塞約 1 秒（`dev_logs/2026_10_04_pvp_v6_batch01.zh-Hant.md`）；swapchain 的 claim／acquire／present 與事件 pump 只能在視窗執行緒，所以不能另開 render 執行緒（input-and-present D27）；shader 要自己備齊 DXIL／SPIR-V／Metallib 三種格式與 host 工具鏈（DXC、SDL_shadercross、SPIRV-Cross、Xcode）——都寫在 `SDL_gpu.h` 與 SDL 原始碼裡，整合前就讀得到。所以每列先寫「採用後會卡在哪裡」，來源是官方標頭、文件、授權檔與 issue。
- **來源**：候選方案的事實來自 5 份偵察報告（主 checkout `build/evidence/third-party-evaluation/recon-0*.md`，每條事實附 URL）與對抗檢查者自己補讀的來源（同夾 `adversarial-check-20261110.md`，條目編號 A1～A26，本文引用時標 A 編號）；兩者寫「未找到來源」的，本文標「未查到」，不憑記憶補。GYO 側的事實來自 repo 的契約、計畫、dev_log 與原始碼（清單在同夾 `sources.txt`）。偵察子代理：general-purpose、Opus、high、只讀。
- **「能否當 Kernel 下的 plugin」**：對照使用者的架構願景稿（Bevy 式 Kernel／IoC；架構師 2026-11-10：Kernel 進 Engine、Stage 固定序列、協定先設計實作薄）。判斷標準：候選方案是否自帶迴圈、排程、執行緒或全域狀態（會和 Kernel 搶控制權），以及它的資料能否留在自己的世界、只經 Stage 交換。
- **數字**：行數、commit 數為 2026-11-10 讀取值；工時是量級（天／週），未經實作驗證。

### 0.1 GYO 自製子系統盤點（2026-11-10 的工作樹）

| 模組 | 原始碼（檔／行） | 連結 | 測試（`tests/common/<m>` cpp 行） | 突變目錄（數） | 契約 |
|---|---|---|---|---|---|
| `GYO::EntityStore` | 3／155（header-only） | Base | 295 | entity-store 15 | DI 10、RC 15 |
| `GYO::Collision` | 5／962 | Math、Base | 1,378 | big-map engine_grid 19 | DI 3、RC 5（格子） |
| `GYO::Audio`＋SDL 後端 | 12／1,619 | Base、Time；後端 SDL3 私有 | 2,140 | audio 85＋tsan 6＋asan 1 | DI 11、RC 8 |
| `GYO::Model`（含動畫、事件） | 10／1,174 | Engine、Math；ufbx 另為 adapter | 993 | anim_events 13 | DI 5、RC 3（事件） |
| `GYO::Ui`＋`UiRenderer` | 11／4,471 | Engine、Math | 2,144 | （ui_editor 線） | DI 7、RC |
| `GYO::Render`＋SDL_GPU 後端 | 26／3,676（不含 shader 工具 `shaders/pipeline`） | Engine、Math；SDL3 私有 | 2,406 | input-and-present 83（含平台） | rendering_architecture |
| `GYO::VoxelMesh`、`GYO::MapMarkers` | 2／1,012；2／457 | Base、Math | 1,198；490 | engine_format 68、voxel_mesh 20 | DI 7／5 |
| `GYO::Math` | 22／1,451 | Base | 4,061 | — | math.md |

共同點：每個自製模組都有「中立」（不認識產品）、「決定性」或「一個驗證者」的 Design Invariant，且有連結守衛（configure-time `gyo_expect_*_links`）。候選方案若要進來，首先要過的是這些 DI，不是功能清單。

---

## 1. 逐列評估

每列的欄位：GYO 現況與為什麼自製｜需要它的 Feature｜候選方案的事實｜**限制（採用後會卡在哪裡）**｜判定｜代價與不採的代價｜要使用者決定的事｜讀了什麼。

### 1.1 ECS：EnTT、Flecs vs `GYO::EntityStore`（基盤）

**GYO 現況與為什麼自製**
- `GYO::EntityStore`（2026-11-05 核准，PR #156）：只有「容器」——每元件一條 `std::vector<T>`、依呼叫端給的 `EntityId` 嚴格升冪、binary search、保序 erase、`DeferredChanges`（生成先、刪除後、各依 id 升冪，呼叫端在它的同步點套用）。沒有 id 配發、world、system、scheduler、event、plugin（DI 1、4、5；`docs/architecture.md` 的禁令：no umbrella framework、plugin ABI、service locator；ECS 只剩容器）。
- 自製的理由不是「做得到」而是**不變式**：DI 2「順序是不變式不是操作」——只依 id 的順序在每個持有相同 id 的程序裡都一樣，和插入／刪除歷史無關；DI 8「由構造保證決定性」（無 unordered、hash、clock、thread）。Match 的 digest 依 id 升冪逐位元比對（entity-model E-5、E-6、E-10）。
- 這個判斷已經做過一次參照調查（Bevy v0.20.1、EnTT v3.16.0，`plans/entity-store/survey.zh-Hant.md`，對抗檢查 Fable／xhigh），結論：兩個參照都「不以不變式提供順序、只以操作提供」（Bevy 每個 iter 不保證順序，要排就 `sort::<Entity>()`；EnTT 要順序就 `registry.sort`）。本文不重做那份調查，只補 Flecs 與 EnTT v4 的事實。

**需要它的 Feature**：AI 小怪（第一個非玩家實體；entity-requirements 7-1）、場景線的物件（門、物品、容器；3b `DeferredChanges` 與第一個非玩家批同時）、pv10 的每收件者分送（RC 6 的 change tick 候選）。這些都是「容器之上」的需求：id 配發、種類、相關性選擇，依 DI 留在產品。

**候選方案的事實**（recon-01）
| | EnTT v4.0.0（2026-07） | Flecs v4.2.0（2026-11） |
|---|---|---|
| 授權 | MIT | MIT（含 Meta 部分版權） |
| 語言／建置 | C++20 header-only、無依賴；約 2.4 萬行 | C99 核心零依賴＋C++17 包裝；src 12.7 萬行＋include 3.8 萬行；CMake／Bazel／Meson／單檔 |
| CI | Linux g++ 12–14、clang 16–18；MSVC、v143、clang-cl；macOS | gcc 9–14、clang 14–18；MSVC、v142、clang-cl、MSYS2；macOS 15 |
| 迭代順序 | 無穩定保證；view 選最小的 pool 迭代，順序「depends on how the components are arranged in their pool」；刪除 swap-and-pop | archetype（table）；從 table 移除時把最後一個搬到被刪的位置；pipeline 依 system 的 entity id 排序但 id 可回收故「not guaranteed」；同一事件多個 observer 順序「undefined」 |
| id | registry 配發；預設 32 位＝entity 20 bit＋version 12 bit；用盡 assert | world 配發；可回收 |
| 結構變更 | 迭代中可新增；刪除「當前」可以、刪其他實體是 UB；官方建議延後處理 | 一律經 stage／deferred，merge 後可見；指令所指實體若已刪除則丟棄 |
| 執行緒 | 不開執行緒；registry 非 thread-safe | 預設單執行緒；`set_threads(N)` 自開 worker 或 `set_task_threads` 外接；每執行緒要自己的 stage |
| 記憶體／錯誤 | Allocator 模板參數；`ENTT_ASSERT` 可自訂、支援無例外 | `ecs_os_api_t` 可換 malloc／thread；非 NDEBUG 時 assert 中止，「Application code does not need to check for errors」 |
| 維護 | 近一年 779 commit；skypjack 7642、其次 ≤ 8（實質單人） | 近一年 1182 commit；SanderMertens 5951、其次 49（實質單人） |

**限制（採用後會卡在哪裡）**
1. **順序**：兩者都在刪除時重排（swap-and-pop／搬尾）。要得到 GYO 需要的 id 升冪 digest，每 Tick 要再排序（EnTT `sort`、Flecs `order_by`），或自己維護一份順序——後者就是 EntityStore 本身。Flecs 的排序查詢依賴 change detection，且「需求衝突的查詢會不停重排」（Queries.md:4394）。
2. **id 的權威**：兩者都由容器配發 id（EnTT 帶 version 位、Flecs 可回收）。GYO 的 id 由 Gateway（玩家段）與 Match（非玩家段）配發、保證不重用（E-2、E-3）；容器配發違反 DI 1。EnTT 預設 20 bit entity（約 100 萬）可用 `ENTT_ID_TYPE=uint64_t` 放到 32 bit，不是硬上限；真正的坑是它的 sparse 頁表隨**最大** id 增長，而 GYO 的 id 單調不重用（程序生命期約 340 萬，entity-requirements 1-4），頁表只會變大不會回收（對抗檢查 A23）。
3. **UB 邊界**：EnTT 迭代中刪其他實體是 UB；GYO 的規則在 Tick 內凍結成員、同步點才套用（E-12），這條紀律要靠 GYO 自己守（EnTT 沒有結構性守護；Flecs 有 stage 但 merge 的順序是入隊順序不是 id 順序）。
4. **Kernel／plugin**：Flecs 的 pipeline／system／observer 就是一個 scheduler＋event bus，和使用者願景的 Kernel 搶同一個位置，而它的 system 順序與 observer 順序都明寫不保證——不能當 Match 的 Stage 序列。EnTT 是容器＋signal＋organizer，沒有迴圈，可當 plugin 的資料層，但 GYO 要的順序不變式它不提供。
5. **錯誤風格**：Flecs「assert 中止、應用程式不檢查錯誤」和 GYO「Assert＝Programmer Error、Result＝Runtime Error」在容器層一致（EntityStore DI 7 也不回 Result），這點不衝突。
6. **維護**：兩者都實質單人維護（和 ufbx、Jolt、ozz、meshoptimizer 一樣）；GYO 已接受 ufbx 這種風險，方法是釘版本＋adapter。

**判定：自製（維持）。** 理由：GYO 需要的是「順序不變式」與「id 由外部給」，兩個候選都不提供，借來之後還要自己補，等於保留 EntityStore 再加一層。這不是「候選不好」，是它們解的問題（通用 world、查詢、archetype 快取）現在沒有 Feature 需要（RC 3：> 4,000 實體或欄維護 p95 > 1 ms；RC 4：元件組合動態；RC 5：同一交集手寫 > 3 處）。
- **代價**（不採）：查詢與交集手寫（雙指標合併），RC 5 到了要自己做 query 層；沒有 archetype 快取（實體約 400，entity-requirements 1-2）。
- **採的代價**：重做 survey／DI／RC（已花約 2.5 h 的調查＋Delta），容器搬進 Engine 的 PR #156 作廢；順序與 id 的補丁層。量級：週。
- **對 Kernel 願景的推論**：使用者願景稿 §12 問「EntityStore 是否足夠？是否需要統一 Scheduler？」——本評估的答案是：Scheduler／Stage 是 GYO 自己的協定（Flecs 的不決定性說明為什麼不能借），容器留 EntityStore；要不要在 Engine 做 Kernel 是另一張卡（架構師 2026-11-10 的看法），不是這一列的替換問題。

**要使用者決定的事**：無（維持現狀不需要決定）；Kernel 的卡另開。

**讀了什麼**：entity-store.md、plans/entity-store/survey.zh-Hant.md §4–§5、entity-requirements.md、recon-01 §1–§2。

### 1.2 Physics：Jolt、Bullet vs `GYO::Collision`（基盤：權威判定的一部分）

**GYO 現況與為什麼自製**
- `GYO::Collision`（962 行）：膠囊／盒子／射線的精確查詢（「每個查詢在 double 計算、對 float 捨入一次；一個容差 1e-7」，`Collision.hpp`），加 `StaticAabbGrid`（DI 1：每個查詢逐位元等於依索引順序線性掃描的結果；平手規則是呼叫端的參數）。沒有剛體、沒有動態 broadphase、沒有約束求解。
- 產品的 Movement（`StepMovement`，60 Hz 固定 tick）與 ShotQuery 在 Match 與 Client 跑同一套，Client 預測、Match 權威。全部程式 `-ffp-contract=off`、MSVC `/fp:precise`（math.md）。
- **GYO 現在的決定性範圍**（對抗檢查 A3 修正）：L1 的 `object_fps_pvp.authority_digest`（label `cpu`）在 L1 的三個平台（linux-x64、macos-arm64、windows-x64；macos-x64 自 2026-10-31 不在 L1，決定性卡核對）都跑，但只對**零角度情境的 golden 子集**要求跨平台逐位元一致；其餘情境「依 libm 合法地不同」（`tests/object_fps_pvp/CMakeLists.txt:407-452`、`authority_digest_check.cmake`；math.md：sin／cos／tan／pow／acos 依平台的 libm，Math 不保證跨平台逐位元）。也就是：GYO 保證的是「同一份二進位在同一平台可重現＋不含三角函數的子集跨平台一致」。**而且後者也不乾淨**：決定性卡（`v8-arch-determinism-levels`，PR #177）核對 11/9 場次，有輸入、狀態、位置全同而 `sweep_fraction` 差 1～4 ULP 的步驟（windows-x64 MSVC 對 macos-x64 AppleClang，路徑上沒有 libm），原因未明，先做偵測批 E0（DET-0）。
- 自製的理由：權威判定要的是**可宣告、可用暴力掃描比對的契約**。big-map survey 讀過 Jolt 的 `Docs/Architecture.md`：「Jolt 明寫它的 broadphase 查詢不是決定性的」，GYO 因此把「結果＝線性掃描」寫成自己的契約（survey §2.6）。

**需要它的 Feature**
- 已點名：門（場景線，≤ 50 扇，開關改碰撞）、陷阱、投射物（entity-requirements 1-5：生成事件＋純軌跡，不當每 Tick 同步的實體）、技能（投射物、範圍效果）、Boss（不受推擠）。
- **沒有點名**：剛體玩法（可推的物理道具、載具、可破壞物——entity-requirements 1-1 明寫「沒有載具與可破壞物」）、ragdoll。
- 架構師對門的看法（scene/README）：門 ≤ 50 扇各一個 AABB＋開關位，Movement 查完靜態格再線性掃動態盒子；Collision RC 1「會動的碰撞體」是它的出口，但 50 個盒子的線性掃描不需要動態樹。

**候選方案的事實**（recon-02；對抗檢查 A4、A5、A17 補）
| | Jolt v5.6.0（2026-07） | Bullet 3.25（2022-04） |
|---|---|---|
| 授權 | MIT | zlib（Extras、examples/ThirdPartyLibs 除外） |
| 語言／建置 | C++17、只依賴 STL、不用 RTTI／例外；約 11.4 萬行；CMake 選項 `CROSS_PLATFORM_DETERMINISTIC`（預設 OFF）、`DOUBLE_PRECISION`、ISA（SSE4.x／AVX／AVX2／**FMADD 預設 ON**）；**預設 `JPH_USE_DX12／VK／MTL／CPU_COMPUTE=ON`**（v5.6 的 hair 模擬 GPU compute：Windows 找 SDK 的 DXC、Linux `find_package(Vulkan)`、macOS 要 dxc 與 spirv-cross；`Docs/ReleaseNotes.md:35-40`）；**預設靜態 MSVC CRT**（`USE_STATIC_MSVC_RUNTIME_LIBRARY` ON）；非決定性模式下 MSVC 預設 `/fp:fast`；`Build/` 的 CMake 另加 `/Wall /WX`、IPO；`DEBUG_RENDERER`／`PROFILER` 在 Debug 與 Release 預設 ON，出貨組態是 Distribution | 「C++ 2003」；約 24.3 萬行；MSVC 預設 `/fp:fast`；沒有決定性選項 |
| CI | VS CL（含 32-bit、ARM）、VS Clang、MinGW、Linux Clang／GCC、macOS、Android、iOS、Emscripten；另有 determinism_check（14 個平台組合比對 hash） | 只有 ubuntu-latest 一個 job |
| 決定性 | 預設：同一份二進位＋同一 API 呼叫順序。開 `CROSS_PLATFORM_DETERMINISTIC`（約慢 8%）：結果與編譯器、組態、OS、架構、字長無關，前提同一份原始碼與 define；**應用程式自己也要** precise 模式＋`-ffp-contract=off`、FPU 狀態一致、不用標準 sin/cos／std::sort／std::hash。**不保證**：BroadPhaseQuery 結果（多執行緒放寬 AABB）、NarrowPhaseQuery 的順序、各種 Listener 的回呼順序、GetActiveBodies 順序；文件自承「less tested than other features」。issue：#1551 `CharacterVirtual` 的 `GetMaxHitsExceeded` 一旦 true 模擬不再決定；#2008（2026-05）double 精度 Android 與 Windows 分歧（當時只驗 single）。**每個 minor 都改模擬結果**（v5.5 新摩擦模型、v5.6 再改）、二進位序列化不相容（`APIChanges.md:16-28`） | 官方說明未查到。`m_deterministicOverlappingPairs`（預設 false）、solver 隨機種子、多執行緒 solver 註解「may affect determinism」；issue #4722（2025）問如何取得決定性，open 無回覆 |
| 執行緒 | `Update(TempAllocator*, JobSystem*)`；有 `JobSystemSingleThreaded`、`JobSystemWithBarrier`（接自家 job system） | `btITaskScheduler` 外接 |
| 錯誤 | Trace／AssertFailed 可覆寫；Update 回傳錯誤旗標（cache full＝有接觸被忽略）；序列化只能給可信資料；多個 PhysicsSystem 共用全域（Factory、allocator、Trace） | btAssert；官方 assert 鉤子未查到 |
| 單獨查詢 | 可對單一 shape／TransformedShape 查詢，不建世界 | 可當 collision detector library |
| 角色 | `Character`（只平移的剛體）／`CharacterVirtual`（不進世界、只用 NarrowPhaseQuery；一般 CastRay 打不到它；角色間碰撞要自己接） | `btKinematicCharacterController`（ghost object；與剛體互動要使用者自己做） |
| 維護 | 近 3 個月 ≥ 100 commit，jrouwe 1571、第二名 29 | 近一年 3 commit（改 README）；issue tracker 名義上關閉 |

**限制（採用後會卡在哪裡）**
1. **決定性的條件不同向**（A3 修正）：GYO 現在是「同平台可重現＋零角度子集跨平台一致」，三角函數依 libm 合法地不同。Jolt 開 `CROSS_PLATFORM_DETERMINISTIC` 後反而**比 GYO 的 Math 更嚴**（自帶 sin／cos、全平台 hash 比對），但條件也更多：同一組 define、FPU 狀態、禁用標準 sort／hash／sin／cos 於碰到結果的程式、慢 8%、文件自承「less tested」、每個 minor 改結果（golden 與回放每次升級要重錄）。採用後 GYO 要決定「digest 的範圍跟 Jolt 一起變嚴（全部情境跨平台）還是維持現在的子集」，而不是「Jolt 比較不決定」。
2. **broadphase 契約要重寫**：Jolt 的 broadphase 查詢不決定（文件建議自訂 collector 再過濾、再排序）。GYO 的 `StaticAabbGrid` DI 1「等於線性掃描」正是為這個寫的；換 Jolt 等於放棄這條契約，再在 Jolt 外面補一層排序。
3. **狀態的權威**（AGENTS §3）：Jolt 的 `PhysicsSystem` 持有每個 body 的位置與速度，是第二份狀態；要嘛 Jolt 成為位置的權威（EntityStore 的欄變成寫回的副本，Movement 的預測／回溯／`MovementCorrection` 全部改寫），要嘛 Jolt 只當查詢器（不進世界、每 Tick 用 `TransformedShape` 查靜態幾何）——後者就是 GYO 現在的 Collision 在做的事，只是換了演算法與捨入。
4. **角色控制器語意不同**：`CharacterVirtual` 的更新順序副作用（文件例：下降電梯上略懸空）、`GetMaxHitsExceeded` 破壞決定性、一般射線打不到角色（ShotQuery 要另接）；GYO 的 `StepMovement` 是產品的規則，Client 預測也用它——Client 也要連 Jolt。
5. **回滾**：`SaveState`／`RestoreState` 不涵蓋回滾區間內的增刪 body；GYO 的 rewind-line 命中判定用的是歷史位置不是物理狀態，不衝突，但投射物（每局 2 萬件生滅）若進 Jolt 世界就踩到這條。
6. **全域狀態**：Factory、預設 material、allocator、Trace／Assert 是程序全域；一個程序多個 PhysicsSystem（例如 Match 同時跑多局）共用它們。
7. **建置整合**（A1、A2、A4、A5；細節在 §2.5）：(a) GYO 的 `-ffp-contract=off` 在 `add_subdirectory(third_party)` **之後**才加（`build/cmake/GyoBuild.cmake:35-42`），照慣例放進 `third_party/` 的 Jolt 拿不到它；(b) Jolt `USE_FMADD` 預設 ON，直接用 FMA intrinsic，contraction 旗標管不到；(c) 預設會編 DX12／Vulkan／Metal 的 GPU compute 後端——第二條 shader 工具鏈，且和「GPU 由 SDL_GPU 持有」衝突，必須全部設 OFF 並加連結守衛；(d) 預設靜態 CRT（/MT）與 GYO 的 /MD＋app-local CRT 部署檢查衝突，要用 GYO 的 CRT 與旗標重建（只 include `Jolt.cmake`，不用它的 `Build/`）；(e) Debug／Release 預設帶 DEBUG_RENDERER／PROFILER。
8. **Bullet**：沒有決定性選項、MSVC 預設 fast math、CI 只有 Linux、近一年 3 個 commit——三平台 CI 與決定性兩條都過不了；不列為候選。
9. **Kernel／plugin**：Jolt 作為能力接進 Kernel 沒有結構性衝突（它不開迴圈；job system 可用 `JobSystemSingleThreaded` 或接 `GYO::Threads`）；衝突在資料（第 3 點）與全域（第 6 點）。

**判定：並存——權威判定留自製，Jolt 只在「非權威的視覺物理」或「權威的剛體玩法」Feature 出現時才採（兩條路徑，後者是基盤決定 → D1）。**
- 現在沒有 Feature 需要剛體動力學；門、陷阱、投射物、範圍效果都是「形狀查詢＋產品規則」，Collision 現有的查詢加一個小的動態盒子集合（架構師的看法）就夠。把 Jolt 放進權威判定要付的是第 1～4、7 點，而回報是現在沒有人要的功能。
- 若之後要 ragdoll、死亡時的碎片、裝備掉落的物理（純表現）：Client 端採 Jolt、不進 digest、不要 `CROSS_PLATFORM_DETERMINISTIC`——這是最便宜的採用路徑（量級：1～2 週，含 adapter、Render 的接法與第 7 點的建置整理）。
- 若之後要**權威的剛體玩法**（可推的道具、載具）：這是基盤的決定，照使用者的立場要在 Feature 排進來前就定；代價是第 1～4、7 點（量級：4～8 週，含三平台 digest 範圍的重新宣告、Collision DI／RC 改寫、Movement 改寫、Client 連 Jolt）。Collision RC 1 與 2 是它的出口。
- **不採的代價**：自製的範圍停在「膠囊 vs 盒子」；要新形狀（斜面，voxel-mesh RC 1）、會動的碰撞體（Collision RC 1）時，每個都是 GYO 自己的 Delta。

**要使用者決定的事**：路線圖上有沒有「權威的剛體玩法」（會影響判定的物理）？有 → 現在就排 Jolt 的基盤評估批（含 digest 範圍的重新宣告與 CI）；沒有 → 維持，Jolt 留給視覺物理。

**讀了什麼**：collision.md、Collision.hpp、Movement.hpp、math.md、`tests/object_fps_pvp/CMakeLists.txt`（authority_digest）、big-map survey §2.6、scene/README、entity-requirements 1-1／1-5／4-1、recon-02 §1–§2、對抗檢查 A1～A5、A17。

### 1.3 2D Physics：Box2D（功能層）

- **GYO 現況**：無 2D 模擬。**Feature**：沒有 2D 產品；GYO::Ui 的版面是靜態幾何，不需要物理。
- **事實**（recon-02 §3）：MIT、C17、約 5 萬行、三平台 CI；決定性是「first class feature」：相同輸入重現、2 與 8 執行緒結果相同、v3.1 起跨平台（64-bit；MSVC precise、clang／gcc 關 contraction、自製 atan2／cos／sin、每 PR 跑 hash 測試；CI 另加 RelWithDebInfo 因為 golden hash 平常只在 -O0 驗）；v3.2 起執行期選 AVX2 否則 SSE2；單一維護者。角色控制器（mover）標 experimental。
- **限制**：2D；沒有座標環繞；模擬中讀寫 world 不安全。
- **判定：不需要（現在）。** 它的價值在**參照**：Box2D 的決定性做法（停用 FMA、自製三角函數、每 PR 比 hash、同一 hash 跨 SIMD／非 SIMD）是 GYO 若要給自己的 Collision 或未來的 Jolt 整合立 CI 門時最接近的樣本。
- **要使用者決定的事**：無。

### 1.4 Navigation：Recast & Detour（功能層，但資料契約是基盤）

**GYO 現況**：無導航。地圖是體素（`gyo.voxel_map`＋`gyo.voxel_region`，16³ 區塊、0.5 m cell、約 1 km²），由工具烘焙、Engine 只解析與展開（voxel-mesh DI 2「No build」）。big-map 的 RC 候選表已列「導航資料｜未定｜AI 尋路的 Feature 開始｜Godot（2.4）」。

**需要它的 Feature**：AI 小怪（3 種 ≤ 20，每 6 Tick 決策一次、每 Tick 照上次決定走 `StepMovement`）、Boss。entity-requirements 4-6：AI 決策「只要 Match 自己一致；亂數有種子、能在 digest 裡重現」——**不要求兩台 Match 給同樣輸入得到同樣結果**，Client 不跑 AI。這一條大幅放寬了導航的決定性要求：尋路的浮點只要在 Match 自己的程序裡穩定。

**候選方案的事實**（recon-03 §1；對抗檢查 A12 補）：zlib；C++98、不用 STL／例外／RTTI；Recast 約 10.5k 行、Detour 9.2k、DetourCrowd 4.2k、DetourTileCache 3.5k；CMake（選項 `RECASTNAVIGATION_DT_POLYREF64`、`ENABLE_FAST_MATH` 預設 OFF、`ENABLE_ASSERTS`）；CI macOS／Ubuntu／Windows 2022；單執行緒（Roadmap「no specific threading support」）；`rcAlloc`／`dtAlloc`、`dtAssert` 的 hook 是**程序全域**；錯誤是 `dtStatus` 位元旗標；navmesh 二進位 'DNAV' version 7，有 endian swap；查詢物件 `dtNavMeshQuery` **有內部可變狀態**（節點池、開放清單），不是純函式；節點池上限 65,535（`DetourNode.h:31-34`、`DetourNavMeshQuery.cpp:171-214`）；TileCache 的動態障礙只有圓柱、AABB、OBB，`MAX_REQUESTS` 64，每次 `update()` 只重建**一個** tile（`DetourTileCache.cpp:583-590`），且要自備壓縮器（`DetourTileCacheBuilder.h:96-104`）。維護：最近 release v1.6.0（2023-05），但 2026-01～02 有 46 commit；148 open issue。決定性：**官方未說明**；`DetourMath.h` 把 sqrtf／cosf／sinf／atan2f 包成 `dtMath*`，2013 的 PR #4 說是為了「projects requiring determinism」能換軟體數學庫；**決定性數學的 PR #688 未合併就關閉**；光柵化結果依三角形輸入順序而變（issue #232，2016 年起 open）；不可信資料越界的 issue 有 11 件（#829–#839）。

**限制（採用後會卡在哪裡）**
1. **座標系**：Recast 是右手、Y 向上、順時針繞序（FAQ）；GYO 是左手、+Y 上、+Z 前（math.md）。體素幾何送進 Recast 前要鏡射一軸，路徑回來再鏡射；這是 adapter 的事，但每個查詢都經過它，容易在平手與繞序上出錯。
2. **PolyRef 32 位**：tile 位元＋poly 位元＋salt ≥ 10 位；1 km² 的 tile 數與每 tile 的多邊形數要先算，否則 init 失敗（`DetourNavMesh.cpp:255-264`）；`DT_POLYREF64` 是出口，但改 ABI；`DT_POLYREF64` 與 `DT_NAVMESH_VERSION` 都要寫進 GYO 的資料契約。
3. **節點池 65,535**：跨 1 km² 的長路徑會 out of nodes（查詢回部分路徑）；要分段尋路或用分層，這是 AI brain 的規則，不是 Detour 給的。
4. **不可信資料**：11 件 issue 回報 `addTile` 等讀不可信資料越界。GYO 的做法只能是**把 navmesh 當烘焙產物、以自己的索引檔帶 sha256**（`gyo.voxel_map` 已這樣做 region 檔），Detour 的二進位不當 Data Contract 的驗證者——這和 voxel-mesh DI 3「一個驗證者」的精神一致：驗證者是 GYO 的包裝（版本、長度、雜湊），Detour 只讀通過雜湊的 blob。
5. **決定性**：沒有官方保證、決定性數學 PR 未合併；光柵化依輸入順序；跨平台的差異來自 libm（`dtMath*` 包裝可換）與編譯器。GYO 現在不要求跨 Match 決定（4-6），所以只要「同一份 Match 二進位」就夠；**建圖在工具側也要可重現**（兩台 host 烘焙的 blob 要相同，否則 sha256 索引沒有意義）——未查到來源，列入 D3 的試驗。若之後要求跨 Match 一致，要換掉 `dtMath*` 的 libm 呼叫並自己做 hash 測試（Box2D 的做法）。
6. **動態重建與狀態權威**：門 ≤ 50 用 TileCache 的 AABB 障礙可以，但每次 `update()` 只重建一個 tile，導航資料會落後權威的門狀態幾個 Tick（AGENTS §3：導航是權威狀態的副本）；另一條路是不動 navmesh、以 `setPolyFlags`＋query filter 把門的多邊形標成不可通行（同 Tick 生效、不重建）。地形可破壞（沒有這個 Feature）則要重跑 Recast 的區塊建圖，Recast 的建圖對體素化參數敏感、單執行緒。
7. **執行緒**：全部單執行緒；20 個 AI、10 Hz 決策在固定 tick 內查詢，量級沒問題（未量）；Crowd（局部避讓）若用，`DetourCrowd` 自帶一套每幀更新的 agent 狀態——這是「第二份位置狀態」（AGENTS §3），GYO 應只用 `dtNavMeshQuery`，避讓留給產品的 brain。
8. **建置**（§2.5）：放進 `third_party/` 同樣拿不到 `-ffp-contract=off`；Recast 本身沒有 FMA intrinsic，但 arm64 Clang 預設會融合——要在它的 target 上明確設旗標（跨 Match 不要求一致時影響小，仍要寫下）。
9. **Kernel／plugin**：`dtNavMeshQuery` 無迴圈與執行緒，接進 Kernel 當能力沒有衝突（每個查詢者持有自己的 query 物件，不共用）；建圖在工具（AGENTS §8：Editor／Script → Data Contract → Application）。
10. **自製的替代**：地圖本身是格子，整數格子上的 A*／流場是**天然決定、天然符合資料契約**的做法，但 1 km² × 0.5 m 一層就 400 萬格，20 個 AI 要分層（HPA*）才跑得動；它處理不了 agent 半徑／高度與斜面（voxel-mesh RC 1 之後），而 Recast 的體素化正是為這個設計的。

**判定：第一候選（Recast 離線建圖＋Detour 執行期查詢），由使用者在 AI 批的事前宣告時定案。**（對抗檢查 A12：原文「採用」替使用者定了一個還沒排的 Feature，改為候選。）理由：導航是 Recast 解了二十年的問題，GYO 的要求（Match 自己一致、靜態地圖、少量門）落在它的舒適區；資料契約的形狀（GYO 索引＋sha256＋Detour blob、由工具烘焙）和既有的 voxel_map 相同。
- **代價**：烘焙工具（voxel 區塊 → 三角形 → Recast → tile blob）、Engine 的 adapter（座標鏡射、PolyRef 配置、`Result` 包 `dtStatus`、連結守衛、建置旗標）、資料契約與規則測試、AI 批自己的查詢與分段；量級：2～3 週（不含 AI 的 brain）。
- **不採的代價**：自製格子 A*（含分層、半徑／高度處理、斜面）量級相近或更多，而且沒有成熟實作可對照；唯一的好處是整數決定性——現在的需求（4-6）不要它。
- **保留的出口**：若 4-6 改成「兩台 Match 要一致」，要麼換掉 `dtMath*` 的 libm 並補 hash CI，要麼改格子 A*；寫進導航契約的 RC。

**要使用者決定的事**：(1) 4-6「只要 Match 自己一致」是否維持（決定導航要不要跨平台決定性）；(2) AI 批開工前是否先做一次 Recast 對體素地圖的建圖試驗，量：建圖時間、tile 數、PolyRef 位元、**最長路徑的節點數**（對 65,535 上限）、**兩台 host 烘焙的 blob 是否逐位元相同**。

**讀了什麼**：voxel-mesh.md、big-map survey §2.4／§6、scene/README、entity-requirements §4、math.md（座標系）、recon-03 §1、對抗檢查 A12。

### 1.5 UI：Dear ImGui、RmlUi vs `GYO::Ui`（基盤：Editor／Runtime 的資料契約）

**GYO 現況與為什麼自製**
- 遊戲內：`GYO::Ui`（4,471 行）＋`UiRenderer`：`gyo.ui` JSON（封閉元素集 v1：container、panel、image、text、button、horizontal_slider、fixed_step_list）、版面、焦點、繫結、型別化動作、文字欄位（含 IME 組字只顯示不提交，DI 6）；ui_editor 是獨立工具（114＋54 個突變的守衛）。自製的理由是 AGENTS §8：**UI 是 Editor 與 Runtime 共用的 Data Contract**——Runtime 不認識 Editor，Ui 不認識動作的意義（DI 1）、不持有遊戲狀態（DI 4）、不依賴 Input（DI 2）。
- 工具：Dear ImGui v1.92.5（framework 的 ImGui 綁定，跑在 SDL 2D 渲染器上；`v8-tool-model-viewer-render` 已否決「綁定搬到 SDL_GPU」）。IME 問題已經修到 SDL 本身（`gyo-cocoa-ime-composing-keydown` 補丁）。

**需要它的 Feature**：既有（選單、HUD、大廳文字欄位）；pv10 的物品／背包（格子 10 × 6、40 件）會是 GYO::Ui 下一個壓力——RC 寫「需要分支於元素內部狀態、腳本、多行、rich text、自訂快捷鍵」時停下。

**候選方案的事實**（recon-04 §1–§2）
- **Dear ImGui v1.93.0**：MIT；無依賴；CI 三平台（含 `-fno-exceptions -fno-rtti`）。FAQ 原文：「Dear ImGui is NOT designed to create a user interface for games, although with ingenious use of the low-level API you can do it」，「the amount of skinning you can apply is limited」；README：不支援完整國際化（RTL、雙向、text shaping）與無障礙；資料驅動版面：**未查到**官方功能。Docking 仍在分支（作者「not happy with the current state」但「safe and recommended」）。SDL_GPU 後端：shader 預編譯內嵌，格式是 SPIR-V、**DXBC（不是 DXIL）**、Metallib 或 MSL；用 MSL 要在 `SDL_CreateGPUDevice` 宣告 `SDL_GPU_SHADERFORMAT_MSL`；`PrepareDrawData` 必須在 render pass 前呼叫；註解「multi-context support has never been tested」；範例只在 Win32 MSVC 建置。
- **RmlUi 6.3**：MIT；C++17；字型預設 FreeType（可 `RMLUI_FONT_ENGINE=none` 自備）；RCSS 以 CSS2 為基礎刪改＋部分 CSS3，「文件通常要專門為 RmlUi 撰寫」；資料繫結 MVC。渲染介面必須實作 CompileGeometry／RenderGeometry／ReleaseGeometry／LoadTexture／GenerateTexture／ReleaseTexture／Scissor；完整效果要 clip mask（建議 stencil）、transform、layers、filter、shader；GPU 要求 sRGB 預乘 alpha、`ONE, ONE_MINUS_SRC_ALPHA`、貼圖原點左下。官方 SDL GPU 渲染器「沒有 clip mask、filter、shader」（無漸層、box-shadow、圓角裁切），且官方 backend 定位是「複製後自改」。IME 要應用程式實作 `TextInputHandler`；官方平台層只有 SDL 與 Win32 支援 IME。近一年 142 commit，mikke89 91。

**限制（採用後會卡在哪裡）**
1. **ImGui 做遊戲內 UI**：作者自己說不是為此設計；沒有資料驅動版面（ui_editor 的存在前提沒了）；每幀由 C++ 重建 UI 就是 AGENTS §8 禁止的「通常的 Content 編輯用 C++ 改寫」；國際化缺 shaping。
2. **ImGui 的 SDL_GPU 後端**：GYO 的 device 在 Windows 宣告 DXIL＋SPIR-V、macOS Metallib；ImGui 內嵌的是 DXBC 與 MSL／Metallib——要在 device 的格式遮罩加 DXBC（SM5_1）或改走 GYO 自己的 shader 管線重編 ImGui 的 shader；多 context 未測；只在一個平台 CI。這是 `v8-tool-model-viewer-render` 否決 (B) 的具體內容。
3. **RmlUi 對 Render 的要求**：GYO Render 現在是固定管線集（`unlit`、`scene_post`，4 個 uniform 槽、一張貼圖）、沒有 stencil、沒有自訂 shader 的公開介面（rendering_architecture §4「增加新的頂點屬性、額外 texture 或新的 uniform 意義時，必須一起擴充中立介面」）。RmlUi 的完整效果要 stencil clip、layers、filter shader——等於替 RmlUi 擴充 `IRenderDevice` 與 shader 契約（Architecture Delta），或接受官方 SDL GPU 範例的缺口（沒有圓角裁切與漸層）。
4. **RmlUi 替換的不只是 Ui**：`gyo.ui` 契約、ui_editor（含 114＋54 突變）、Input→Ui adapter、IME 路徑全部換成 RML／RCSS＋RmlUi 的除錯器；字型從 GYO::Text（整段點陣）換成 FreeType 的字形級引擎。
5. **RmlUi 的全域狀態與後端**（A21）：`Rml::Initialise`、`Set*Interface`、`RegisterPlugin` 是程序全域（`Core.h`），Kernel 下無法一個 world 一份、測試不能隔離；官方 SDL_GPU 範例渲染器內嵌 SPIR-V／MSL／DXIL，macOS 走 MSL，和 GYO 只宣告 Metallib 不合；它直接 include SDL3_image 載圖，繞過 GYO 的 `TextureLoader`——都是「複製後自改」的範圍。
6. **Kernel／plugin**：兩者都是「每幀由呼叫端驅動」的函式庫，沒有迴圈，可當能力；衝突在資料契約（第 1、4 點）與 RmlUi 的全域（第 5 點）。

**判定：自製（維持）`GYO::Ui`；ImGui 維持只在工具；RmlUi 不採（現在）。** 理由：遊戲內 UI 的自製不是重複造輪，是 §8 的資料契約；兩個候選一個明說不適合、一個要換掉整條契約與渲染介面。
- **不採的代價**：GYO::Ui 的元素集封閉，背包格子、拖放、多行文字都是新版本的契約；RC 到了（rich text、腳本、多行）就是「修還是重建」的決定點——那時 RmlUi 是候選（量級：3～5 週，含 Render 的 stencil／自訂 shader、字型引擎、編輯器的替代）。
- **ImGui 升級的風險**（已採用的部分）：docking 永遠在分支；IME 由 GYO 自己經 Engine 的 input backend 處理，已和 ImGui 的路徑脫鉤（framework DI 2）。

**要使用者決定的事**：無（維持）；RC 觸發時再問「修還是換 RmlUi」。

**讀了什麼**：ui_toolchain.md、framework/architecture.md、rendering_architecture §4、docs/tools/plans/v8/model-viewer.md（(B) 的否決）、platform-sdl.md（IME 補丁）、recon-04 §1–§2。

### 1.6 Model Import：Assimp vs ufbx（功能層的 adapter；已採 ufbx）

- **GYO 現況**：ufbx v0.23.0 釘在 `third_party/ufbx`，`GYO::AssetUfbx` 是可選 adapter（`UFBX` 元件），只匯 FBX；Model 的中立表示（節點、蒙皮、片段、材質）與 FBX 解碼分離（3d-assets.md）；測試用 ufbx 自己的評估結果比對（mark23 dev_log）。選擇的理由（可從結構推）：單一來源格式、兩個檔、無依賴、位元級一致的 CI。
- **Feature**：既有與寵物、AI 模型（美術包都是 FBX；UAL 包另附 glTF 但未用）。
- **事實**（recon-04 §3–§4）：**Assimp v6.0.6**：BSD-3；C++17；repo 約 272 MB；contrib 內嵌 clipper（Boost）、draco（Apache-2.0）、rapidjson、pugixml、zlib 等，meshlab（GPL-3.0）只在 VRML importer 開啟時 clone（預設關）；可只編部分 importer；內部用例外（`DeadlyImportError`），對外回 nullptr＋字串；CI 三平台；標題含 FBX 的 open issue 64 件（越界 #4312、同名節點 bone 錯 #5527、不受控配置 #6948、fuzzer 洩漏）。**ufbx v0.23.1**：MIT／Unlicense；單檔 C99；fuzz＋12,618 檔私有資料集；CI 列三平台位元級一致；0.Y 的 minor 可能破壞相容；bqqbarbhg 126／128 commit（單人）。
- **限制**：Assimp 的 FBX 匯入器是它自己寫的、問題多；授權清單長（每個 contrib 一條）；內部例外要在 adapter 攔（GYO 的 Result 風格）；體積與建置時間大（未查到數字）。ufbx：單人維護、minor 升版要重跑特性測試；沒有寫出器（工具不能產 FBX——GYO 的工具產自己的 JSON 契約，不受影響）。
- **判定：維持 ufbx；Assimp 不採。** 若之後要 glTF（不在清單上），單一格式的小函式庫（cgltf、fastgltf 類）比 Assimp 合理——未評估，只點名。
- **要使用者決定的事**：無。

### 1.7 Mesh Optimization：meshoptimizer（功能層，工具側）

- **GYO 現況**：體素面由工具 greedy 合併（tool-chunked），模型是 FBX 原網格＋CPU 蒙皮；沒有 LOD（big-map todo：HLOD／遠處代理是 RC 候選，觸發是 Client 最壞角度 p95 超過宣告值——未量）。
- **Feature**：無點名。big-map 的量測（survey (a)）：497k 頂點每幀 p95 4.6 ms，離預算遠。
- **事實**（recon-01 §5）：MIT；約 1.8 萬行 C++（`CXX_STANDARD 11`，header C 相容）；CMake 或直接加原始檔；CI 三平台（含 MSVC ARM64）；只用 assert.h／string.h，grep 無 sort／thread；allocator 可換（設定非 thread-safe；allocator 失敗時必須 throw 或 abort，和 GYO 的 Result 風格不合）；決定性官方未說明（README 與標頭都沒有，A9 再查確認）；`MESHOPTIMIZER_EXPERIMENTAL` API 可能變；簡化不保證達標、假設單一材質；zeux 4213／655 commit（單人）。
- **限制**：若用在**執行期**（例如 Client 載入時重排索引），輸出未宣告決定性、且 allocator 設定非 thread-safe；用在**工具側烘焙**則沒有這些問題，但工具的輸出格式（`gyo.voxel_region` 的 quads 是整數格子面，不是三角索引）要先有一個放「最佳化後索引」的欄位——那是契約版本。
- **判定：不需要（現在）。** LOD 或 GPU 量測超標時，在**工具側**當候選（量級：天）；不放進 Engine 執行期。
- **要使用者決定的事**：無。

### 1.8 Audio：miniaudio、OpenAL Soft vs `GYO::Audio`（基盤：決定性與即時安全）

**GYO 現況與為什麼自製**：`GYO::Audio`（1,619 行）：48 kHz stereo PCM16 單一格式、片段庫、固定聲部、**整數混音**（Q16 增益、int64 累加、半上捨入飽和）、以 Engine 時間戳映射到樣本位置（每區塊讀一次錨點）、有界 SPSC 提交端、音效執行緒不配置不鎖不等（DI 7）、逐位元決定性（DI 8）、停止與播放頭（控制層重建 2026-10-31）、SDL 輸出後端私有。自製的理由在計畫 README：消費端要「在事件真正發生的時刻發聲」而畫面 30 FPS 每 33 ms 才處理一次事件；加上工具預覽。控制層規劃時已對照 miniaudio、FMOD、SoLoud、Wwise、Web Audio、Unity（CONTROL.md「成熟方案的參考」），採了它們的語意（絕對 frame 的停止點、不重用的 handle、位置只供顯示），沒採它們的實作。

**需要它的 Feature**：既有（武器事件音、模型檢視工具試聽、音效工具）；pv10 的技能與互動物件會加事件種類，不加混音功能。

**候選方案的事實**（recon-05 §1–§2）
- **miniaudio v0.11.26**：Public Domain／MIT-0；單檔 C；後端 WASAPI／CoreAudio／ALSA／PulseAudio 等；內建解碼器只有 WAV、MP3、FLAC（Vorbis 要另外 include `extras/stb_vorbis.c` 或 libvorbis；MP3 解碼是 float、依 SSE2／NEON 走 SIMD；A7）；mackron 3800 commit、次者 23（實質單人），另有 `dev-0.12` 分支（破壞性變更未查）；engine 與 node graph **只接受 f32**（「The format must be f32」），整數混音路徑未查到；可只用 device 層（`MA_NO_ENGINE`、callback 交付 s16）；排程 `ma_sound_set_start_time_in_pcm_frames` 有 frame 精度，但「due to the nature of multi-threading the times may not be 100% exact」、每節點同時只能排一次 start 與一次 stop；資源管理器預設 1 條 job 執行緒、job queue 用 spinlock；node graph 的 attach／detach 用 spinlock；不保證版本間 ABI；encoding 只支援 WAV。
- **OpenAL Soft 1.25.2**：**LGPL 2 or later**（COPYING：Library GPL v2）；C++20；預設 shared，靜態要 `AL_LIBTYPE_STATIC`；mixer 內部 float；`ALC_SOFT_loopback` 可由應用程式拉取混好的樣本（`ALC_SHORT_SOFT`）；HRTF（SADIE II 資料集 Apache-2.0）。LGPL §6 允許連結發行，條件是允許使用者為自用修改與反向工程、附授權與告示（其餘履行方式未逐條讀）。

**限制（採用後會卡在哪裡）**
1. **決定性**：兩者都是 float 混音。GYO DI 8（同一命令序列、同一區塊切法 → 逐位元相同輸出）是 L2 以錨點記錄重算起點的根據；換 float 混音後 L2 要改成容差比對，跨平台（Windows／macOS 的 libm、SIMD）不再逐位元。
2. **sample-accurate**：miniaudio 自己說排程時間「may not be 100% exact」，建議在 processing callback 內排程——那就是 GYO 現在的設計（音效執行緒在 `Render` 內取出命令並換算）。等於用 miniaudio 的 device 層取代 SDL 的音訊串流（SDL 已經提供同樣的 callback），engine／node graph 不用。
3. **執行緒與鎖**：miniaudio 的資源管理器自帶 job 執行緒與 spinlock；GYO DI 7「音效執行緒不配置不鎖不等」與 GYO::Threads 的角色執行緒會和它搶；Kernel 願景下也是「第二個排程者」。
4. **OpenAL Soft**：主要理由是第 1 點（float 混音，DI 8 失守）與沒有 Feature 需要它解的問題（3D 定位、HRTF）。授權是第二個理由：LGPL 2+ 靜態連結要履行 §6（可重新連結的物件檔、允許反向工程）；動態連結可以履行，而 architecture.md 開頭也允許「native third-party runtime libraries 部署在產品旁」——所以 LGPL 不是技術上的不可能，而是本卡的硬約束清單（使用者給架構師的「GPL／LGPL 會卡」）；**這條約束在 repo 裡沒有書面出處**（A14、A20），建議和「第三方授權通知」一起由使用者定（§2.2）。
5. **Kernel／plugin**：GYO::Audio 自己是「消費端在它的執行緒提交、音效執行緒渲染」，已是能力的形狀；miniaudio 的 engine 層自帶執行緒與圖，接進 Kernel 要把它的生命週期交出去，它的文件沒有這條路（`MA_NO_THREADING` 必須和 `MA_NO_DEVICE_IO` 一起用）。

**判定：自製（維持）；miniaudio 當 RC 1／RC 2 的候選；OpenAL Soft 不採（LGPL）。**
- audio.md 的 RC 1（串流或壓縮格式；跨執行緒載入）觸發時，miniaudio 的解碼器（WAV、MP3、FLAC；Vorbis 另加）是候選——放在**工具側**把壓縮格式烘焙成 PCM16（DI 9 單一格式不變）；解碼會配置記憶體，不能在 DI 7 的音效執行緒上跑。RC 2（buses、effects、HRTF、fades；播放中改增益／音高）觸發時它的 node graph 是候選，代價是 DI 8 的逐位元決定性。只用它的 device 層取代 SDL 輸出對應的是 RC 3（第二個非 SDL 後端），不是 RC 1／2（A14 修正）。
- **不採的代價**：沒有 3D 定位、HRTF、buses、串流；每個都要 GYO 自己做或在 RC 時換。

**要使用者決定的事**：無（維持）；RC 觸發時再問。

**讀了什麼**：audio.md、plans/audio/{README,CONTROL}.md、recon-05 §1–§2。

### 1.9 Shader Compiler：DXC、glslang、SPIRV-Cross（基盤的 host 工具鏈；已採 DXC＋SDL_shadercross＋SPIRV-Cross）

- **GYO 現況**：host 工具鏈釘版本（`Dependencies.cmake`）：DXC v1.9.2602 的 Windows／Linux 發行包、macOS 從 libsdl-org 的 DXC fork 原始碼建置；SDL_shadercross 釘 commit `e55cf5e3`（靜態、`SDL_SHADERCROSS_DXC`）；SPIRV-Cross 釘 commit `1a616956`（靜態）；Metallib 由所選 Xcode 產生；HLSL 單一來源，反射資料檢查介面，manifest 進 bundle（rendering_architecture §2、§4）。已知限制已寫在文件：Metallib 需原生 macOS 與 Apple 工具；MSL 只是中間產物不算 Metal 驗證；交叉編譯要自備 host 工具。
- **Feature**：既有；自訂 shader 只能在既有介面內（§4）。
- **事實**（recon-05 §3–§6）：**DXC** v1.9.2609（2026-09）：NCSA 授權；發行包只有 Windows 與 Linux，**沒有 macOS 二進位**（GYO 因此在 macOS 從原始碼建——這是建置時間成本）；建置文件「complete on Linux and Unix platforms, but is incomplete but usable on Windows」；`dxil.dll`（簽章）的授權條款未查到。**glslang** 16.6.0：授權混合（BSD-3／BSD-2／MIT／Apache-2.0／「GPL 3 with special bison exception」並列，LICENSE 寫 Bison 早已移除；前處理器另有 NVIDIA 授權）；**HLSL 前端 2026-04 起 deprecated，下一大版移除、不再收 bug**——不是 HLSL 來源的選項。**SPIRV-Cross**：Apache-2.0；C++ API ABI 不穩定、建議靜態；MSL 的限制：combined image sampler 要拆兩個 binding、資源陣列佔多個 id、argument buffer 需 MSL 2.0；最後 release 2021、以滾動 commit 維護。**SDL_shadercross**：zlib；「There aren't any releases here」、無 tag；執行期依賴 spirv-cross-c-shared 與 dxcompiler／dxil，DXBC 在非 Windows 要 vkd3d-utils。
- **限制**：(1) 工具鏈三個元件都沒有正式 release（shadercross、SPIRV-Cross）或沒有 macOS 包（DXC）——GYO 釘 commit 是對的，升級是手動事件；(2) **DXIL 簽章（A6 結案）**：DXC 1.8.2502 起驗證器的雜湊開源（#6846）、1.8.2505 起編譯器一律用內建驗證器、不再找外部 `dxil.dll`；授權是 LLVM／NCSA（只有 `d3d12shader.h` 是 MIT）；GYO 的 macOS 從 libsdl-org fork 的 `2c84a1c5` 建 `dxildll`，該 commit 等於微軟 v1.9.2602 加 2 個 commit，所以三個 host 的 DXC 是同一版；正式執行環境要求有效雜湊的 DXIL，PREVIEW_BYPASS 只在開發者模式的實驗 shader model 可用（來源：DXC `docs/ReleaseNotes.md`；D3D12 runtime 拒絕無效雜湊的一手規格仍未查到，只從 release note 推得）；(3) MSL 經 SPIRV-Cross 的綁定映射是 adapter 的知識（rendering_architecture §4 已這樣寫）。
- **判定：維持。** 唯一值得點名但未評估的替代是 Slang（Khronos；單一來源出 DXIL／SPIR-V／Metal）——不在使用者清單、偵察未讀，只記為「若 HLSL 管線成為壓力時再評估」。
- **要使用者決定的事**：無。

### 1.10 GPU Abstraction：SDL3 GPU vs wgpu／Dawn（基盤；已採 SDL_GPU）

**GYO 現況**：`IRenderDevice`（CreateMesh／Texture／Shader／Pipeline、UpdateMeshVertices、AcquireFrame／SubmitFrame／AbandonFrame、ReadTexture）之後是 SDL_GPU adapter（SDL 3.4.0）；離屏裝置給工具；Render 的固定 pass 序列（World → WorldOverlay → ViewModel → post → HUD）。已碰到的限制（本文 §0）：Metal 的 `nextDrawable` 阻塞、swapchain 與 pump 限視窗執行緒（不開 render 執行緒）、三種 shader 格式、ImGui 的 SDL_GPU 後端格式不合（§1.5）。

**Feature**：既有；big-map 的 Client「依 region 畫與剔除」（product-arena-v3）；沒有點名需要 compute、bindless、GPU 驅動渲染、多視圖的 Feature。

**事實**（recon-05 §7–§8）
- **SDL3 GPU（SDL_gpu.h，3.4.20）**：以 Metal／Vulkan／D3D12 為模型，「If you need cutting-edge features with limited hardware support, this API is probably not for you」；後端只有 d3d12／metal／vulkan（＋xr），無 OpenGL／D3D11；每 stage 4 個 uniform slot（只適合小資料）；綁定佈局嚴格規約、FAQ 說佈局錯誤是常見問題且 API 很難自動驗證；render pass 最多 4 color＋1 depth、pass 中不能開另一個 pass、begin pass 成本高；command buffer 綁定取得它的執行緒；**ClaimWindow 與 acquire swapchain 只能在建視窗的執行緒**；一定支援的 present mode 只有 VSYNC、SDR；frames in flight 1～3、改動時 stall；swapchain texture 只能寫；D24 與 D32 只保證其一；**沒有** geometry／tessellation、query（timestamp／occlusion；issue #11696 open、目標 3.6）、pipeline cache（PR #11629 open）、multi-queue／async compute、bindless（#11148 closed not planned：維護者不想並存兩種綁定模型）；MSAA ≤ 8x；FAQ：ray tracing、mesh shader「definitely not near future」；系統需求 D3D12 FL11_0 Tier 2、Vulkan 1.0＋若干功能、Metal macOS 10.14+。
- **wgpu-native v29.0.1.1**：MIT／Apache-2.0；**Rust**（基於 wgpu-core）、C 介面 webgpu.h＋wgpu.h；提供三平台預建二進位；原生擴充 Immediates（push constant）、binding array、pipeline statistics、GLSL／SPIR-V 輸入。**Dawn**：BSD-3；C++；主要以 depot_tools／GN 建置，CMake 可用但要抓依賴；編譯器主要 Clang 19+、MSVC 19.41+、Xcode 12.2+——**GCC 不是主要支援**；shader 是 WGSL（Tint）。**WebGPU 規格**：shader stage 只有 vertex／fragment／compute；無 geometry、ray tracing、bindless、push constant（有 immediate data 64 B）；maxBindGroups 4、maxUniformBufferBindingSize 64 KiB。

**限制（採用後會卡在哪裡）**
- **SDL_GPU（已採，持續）**：上面那串「沒有」就是天花板。對 GYO 近期 Feature 有關的：(a) 沒有 GPU 時間戳查詢——Client 的畫面預算（big-map p95）只能量 CPU 端與整幀；(b) 每 stage 4 個 uniform slot（GYO 的 ABI 只用槽 0）＋嚴格綁定——每加一種材質參數都是 shader 契約版本（rendering_architecture §4 已這樣規定）；(c) 單執行緒 swapchain——錄製可多執行緒，但 present 綁主執行緒；(d) VSYNC 以外的 present mode 要查詢，不保證；(e) 沒有 pipeline cache——每次啟動重建 pipeline（GYO 的 pipeline 數少，現在不是問題）；(f) **depth 格式**：SDL_GPU 只保證 D24_UNORM 與 D32_FLOAT 其一，GYO 把 depth 寫死成 `D32_FLOAT`（`SdlGpuRenderDevice.cpp:17`），不支援時回 `UnsupportedOperation`、沒有退路（A22）——這是「採用後才會在某台機器上卡住」的典型，列入 D5 的 RC 候選。
- **換到 wgpu／Dawn 會卡在**：(a) 建置：wgpu-native 把 Rust 工具鏈帶進 CMake（或依賴預建二進位——GYO 對 DXC 已這麼做，但那是 host 工具不是執行期庫）；Dawn 的 GN／Clang 19 與 GYO 的 Linux GCC CI 衝突；(b) shader：WGSL 是規格語言，HLSL 要經 Naga／Tint 轉——整條 shader 管線重做；(c) WebGPU 的限制比 SDL_GPU 更保守（無 bindless、無 push constant 標準、4 bind groups），**換了不會換到更多能力**；(d) `IRenderDevice` 的 adapter 重寫（約 3,700 行的 Render 裡後端占一部分）＋離屏裝置＋capture 測試。
- **Kernel／plugin**：兩者都是函式庫、呼叫端驅動，無結構衝突；SDL_GPU 的主執行緒規則會成為 Kernel 的 Stage 約束（「Render 的 present 階段只能在視窗執行緒」）——這條應寫進 Kernel 協定。

**判定：維持 SDL_GPU；wgpu／Dawn 不採。** `IRenderDevice` 是對沖；下面這些列為 Render 後端的 Replacement Criteria 候選（rendering_architecture 目前沒有 RC 節，建議補）：Feature 需要 GPU 查詢、async compute、bindless、render 執行緒，或 SDL 的 Metal 阻塞取得在量測上超過 Client 的幀預算而 SDL 不修。
- **不採的代價**：上面那串天花板；但 WebGPU 的天花板更低，換了不解決。
- **採的代價**（若真要）：量級 6～10 週（adapter、shader 管線、CI、三平台驗證）。

**要使用者決定的事**：是否把 Render 後端的 RC 寫進 rendering_architecture（文件工作，可交圖書管理員）。

**讀了什麼**：rendering_architecture §2–§7、IRenderDevice.hpp、input-and-present D27 與 PLAN（阻塞取得）、dev_log 2026_10_04（Metal 原始碼行號）、recon-05 §7–§8。

### 1.11 Animation：ozz-animation vs `GYO::Model`（功能層；Match 側取樣是基盤）

**GYO 現況與為什麼自製**：`GYO::Model`（1,174 行）：`SamplePose`（決定性、不推時鐘）、`BlendPoses`（TRS 插值再解階層，刻意不插矩陣）、`AnimationInstance`（一個明確時鐘、轉場）、`SkinMesh`（CPU、最多 4 權重）、`TransferCompatibleAnimation`（明確節點對應的轉移，不推名字）、`PlaybackInterval::Crossed`（事件）、`gyo.anim_events` 契約。**Match 也取樣動畫**：hitbox 綁在骨頭上，Match 每 Tick 取樣 4 位玩家的姿勢、動畫狀態只由權威狀態決定，Client 照 Match 給的狀態播放（hitbox-plan R1～R3）。決定性的要求是 R5：「只要求 Match 自己可重現」，不要求 Match 與 Client 逐位元一致；B3 的量測（2026-10-31，當時 L1 的四平台）：各平台自己可重現，但取樣結果平台間**不**逐位元一致（Slerp 的 acos／sin 走 libm；math.md 也寫明這些函數不跨平台一致），含命中的 golden 因此只做同平台比對（`v8-product-hitbox-golden-policy`）。限制（mark23 dev_log）：CPU 蒙皮、無動畫混合樹、無雙四元數、無 morph、無 PBR。

**需要它的 Feature**：AI 小怪與 Boss 的動畫（「AI 也走 Match 的動畫狀態與 hitbox」，entity-requirements 4-4）；寵物（裝備式表現，Client 端）；技能的動作。20 個 AI＋4 人在 Client 做 CPU 蒙皮——**未量**。

**事實**（recon-03 §2）：MIT；C++17；runtime 只依賴標準庫；約 3.9 萬行；CI 三平台（含 `ozz_build_simd_ref` 組合）；決定性**官方未說明**；SIMD 只有 SSE2～SSE4／AVX，ARM 退回 scalar ref；混合用 `NormalizeEst`／`RcpEst`，SSE 版直接呼叫 `_mm_rcp_ps`／`_mm_rsqrt_ps`（**近似指令，結果依 CPU 廠商而異**），ref 版用位元技巧；時間與長度是 float（issue #128 提議 int64 以求跨機器決定性，open）；離線工具 fbx2ozz 需 Autodesk FBX SDK（專有），gltf2ozz 用 tinygltf；archive 無向下相容（版本不符直接拒）；功能：取樣、混合、two-bone／aim IK；retarget、dual quaternion「To be planned」；不是 blend tree；不渲染；guillaumeblanc 1600 commit、其他 ≤ 5（單人）。

**限制（採用後會卡在哪裡）**
1. **決定性**：GYO 現在的取樣已經是「同一份二進位在同一平台可重現、跨平台不逐位元」（B3）。ozz 的 SSE 路徑用 `_mm_rcp_ps`／`_mm_rsqrt_ps`，這兩個是硬體近似指令，結果依 CPU 實作而異（偵察只確認原始碼用了它們，`sampling_job.cc:396-450` 本身就用 `RcpEst`／`RSqrtEst`／`NLerpEst`；「Intel 與 AMD 的 rcp／rsqrt 結果不同」只有第二手來源：https://robert.ocallahan.org/2021/09/rr-trace-portability-diverging-behavior.html ，A10）。差別在：GYO 現在的變異是「每個平台一個值」，換 ozz 後變成「同一份 Match 二進位在不同 CPU 上也可能不同」，R5 的「Match 自己可重現」要收窄成「同一台機器」；回放與同平台兩樹比對（golden policy）會受影響。要避開只能全平台用 `ozz_build_simd_ref`（放棄 SIMD），且仍沒有官方保證。
2. **資產管線**：GYO 的來源是 FBX 經 ufbx；ozz 的離線工具要 FBX SDK（不能進 repo 的授權）或 glTF；實際路徑是用 ozz 的 offline 庫自己寫「ufbx → ozz RawAnimation」轉換器，再面對 archive 無向下相容（每次升 ozz 重烤）。
3. **範圍**：ozz 不給 blend tree、retarget、GPU 蒙皮——GYO 現在缺的（混合樹、GPU 蒙皮）它也沒有；它給的（IK、更快的 SoA 取樣）現在沒有 Feature。
4. **Kernel／plugin**：ozz 是 Job 物件（Validate／Run），呼叫端配置輸出；接進 Kernel 無衝突。

**判定：自製（維持）；ozz 當「需要 IK／大量角色的取樣效能」時的候選。** 理由：現在沒有 Feature 需要 ozz 多出來的部分（IK、SoA 取樣效能），而它會把 Match 取樣的可重現範圍從「同平台」收窄到「同 CPU」（除非放棄 SIMD）；Client 側的壓力（20 AI 的 CPU 蒙皮）未量，量到超標時，第一個選項是 GPU 蒙皮（GYO 自己的 shader 契約版本），不是換取樣庫。Match 的取樣成本已量：linux-x64 12 次取樣 0.44 ms（門檻 1.67 ms；hitbox-plan §5）——但那是 4 位玩家；AI 小怪 20 隻＋Boss 也走 Match 的動畫與 hitbox（entity-requirements 4-4），**Match 每 Tick 取樣 25 個骨架的預算還沒寫**，AI 批的事前宣告要補（AGENTS §2；A11），超預算時才輪到 ozz 或降頻。
- **不採的代價**：IK（腳貼地、瞄準）要自己寫；混合樹要自己寫。
- **採的代價**：轉換器、取樣替換、Match／Client 一致性驗證、archive 版本管理；量級 2～4 週。

**要使用者決定的事**：無（維持）；AI 批量 Client 蒙皮成本時再看。

**讀了什麼**：animation-events.md、Animation.hpp、3d-assets.md、mark23 dev_log、entity-requirements 4-4、object_fps_pvp/plans/v8/hitbox-plan.md §4–§5、recon-03 §2。

### 1.12 VFX：Effekseer（功能層）

- **GYO 現況**：無粒子／特效系統；命中表現、槍口標記是 Render 的 sprite 與 overlay。
- **Feature**：技能（瞬發、範圍／持續領域、召喚／建造）、互動物件的純表現（光／音／外觀）、陷阱——pv10-requirements 第 1、3 項，**未排**。這些是 Client 端表現，不進權威、不要決定性。
- **事實**（recon-03 §3；A15 補）：runtime MIT；`LICENSE_RUNTIME_VULKAN` 第 22 行起是 glslang LICENSE 的原文照抄，含「GPL 3 with special bison exception」字樣，但同檔第 29–30 行自己寫「Bison was removed long ago」，且走自訂後端不會用到 Vulkan runtime——**結案，不是授權障礙**；預設 `NETWORK_ENABLED=ON`（編輯器連線用的 socket server，出貨版必須關）、預設靜態 MSVC CRT、`BUILD_EXAMPLES`／`BUILD_TOOLS` 預設 ON；C++17；核心約 4.6 萬行、Dev/Cpp 共 31.6 萬行；submodule：LLGI、glslang、glfw、imgui、spdlog、libpng、zlib、libgd、OpenSoundMixer；CI Windows／Ubuntu／macOS。渲染後端：DX9／11／12、GL、Metal、Vulkan、WebGPU、LLGI——**沒有 SDL_GPU**；抽象介面 `Effekseer::Backend::GraphicsDevice` 要實作 VertexBuffer、IndexBuffer、UniformBuffer、**StorageBuffer**、VertexLayout、PipelineState、Texture、RenderTexture、DepthTexture、FrameBuffer、RenderPass、Shader（FromKey／FromCodes／FromBinary）、**ComputeShader**；沒有「自訂後端」官方指南，線上文件說後端層成員只給 backend 開發者；shader 原文是 HLSL `.fx`，由它的 ShaderTranspiler 轉各後端後嵌成 C header。亂數預設 C `rand()`（可逐 handle 設種子）；有 worker 執行緒選項；效果檔由獨立編輯器製作，格式規格未查到、版本上限 Version18；`ARCHITECTURE.md` 的 Runtime 節只寫 TODO。
- **限制（採用後會卡在哪裡）**：(1) **要自己寫一個 GraphicsDevice 後端**對 SDL_GPU（或對 GYO 的 `IRenderDevice`——後者缺 storage buffer、compute、render pass 物件，介面差距大）；沒有官方指南，只能照 DX11 後端抄；(2) 它的 shader 要經**它的**轉譯器，和 GYO 的 HLSL→DXIL／SPIR-V／Metallib 管線是兩套；(3) 31 萬行的相依樹（含 glslang）進第三方；(4) 美術資產來自它的編輯器，GYO 的資產契約只能包它的二進位（格式未公開）；(5) **材質編譯器**：每個官方後端各有一個 MaterialCompiler，DX11 版在**執行期**呼叫 `D3DCompile`（`EffekseerMaterialCompilerDX11.cpp`）；GYO 規定執行期不編 HLSL（rendering_architecture §2），自訂後端要另寫烘焙時的 MaterialCompiler——下面的工時估計原本沒包含這項；(6) Kernel／plugin：Manager 自帶 worker 執行緒與更新迴圈（`BeginUpdate`／`EndUpdate`），要關掉 worker 由 Stage 驅動——可以，但生命週期與渲染都在它手上；(7) 建置：`NETWORK_ENABLED`、靜態 CRT、examples／tools 的預設都要改（§2.5）。
- **判定：不需要（現在）。技能批開工時再評估，兩個選項由 D4 的答案決定（A16：原文「預設先自製、契約名 `gyo.vfx`」替技能批定了做法，改為選項）：(a) 最小自製粒子（sprite 四邊形、CPU 模擬、一份 Engine 的資料契約）；(b) Effekseer，條件是「美術需要編輯器級的特效製作」。** (b) 的量級：5～10 週（GraphicsDevice 後端＋烘焙時 MaterialCompiler＋shader 轉譯＋資產管線＋建置預設整理）。不採 (b) 的代價：特效的表現力受自製系統限制，美術沒有編輯器。
- **要使用者決定的事**：技能的特效是不是要美術用編輯器製作（決定 Effekseer 進不進候選）；可以等技能批。

### 1.13 Image Processing：stb_image vs SDL3_image（功能層的 adapter；已採 SDL_image）

- **GYO 現況**：`GYO::AssetSdlImage`（SDL3_image **3.2.6**，`SDL_IMAGE` 可選元件）把解碼結果交給中立的 `TextureLoader`；Engine 不帶解碼器。`third_party/sdl3_image/CMakeLists.txt`：`SDLIMAGE_VENDORED OFF`、`SDLIMAGE_STRICT ON`、`SDLIMAGE_BACKEND_STB ON`、**只開 PNG（`SDLIMAGE_JPG OFF`）**。SDL 3.4.0 本體另有 `SDL_LoadPNG`（內含 stb_image v2.30，`SDL_stb.c`），所以現在的建置裡有**兩份** stb_image（A13(e)）。
- **事實**（recon-04 §5–§6；對抗檢查 A13 修正）：**SDL3_image**：zlib；3.2.6 的 PNG 經 stb 後端；**3.4.x 的 PNG 改走 libpng（`SDLIMAGE_PNG_LIBPNG` 預設 ON）或 SDL 本體的 `SDL_LoadPNG_IO`，stb 只剩 JPEG**（`release-3.4.8/src/IMG_png.c:69-84`）；3.4.x 要求 SDL ≥ 3.4.0；格式 BMP／GIF／JPEG／PNG／QOI／TGA／SVG 等，AVIF／JXL／TIFF／WebP 選配。**stb_image 2.30**：MIT／PD；單檔；格式 JPEG（無 12 bpc）、PNG、TGA、BMP（無 1bpp／RLE）、PSD、GIF、HDR、PNM；`STBI_MAX_DIMENSIONS` 預設 2^24；README 明寫安全性修正可能很慢、不能接受就別用；NVD 關鍵字 29 件 CVE（含 2026 年影響 2.30 的 5185／5186／5313）。
- **限制**：(1) GYO 現在的 PNG 解碼就是 stb_image（經 SDL_image 3.2.6），stb 的安全性歷史是現在就承擔的，不是「要不要採 stb」的問題；緩解只有「資產是烘焙產物、由 catalog 與 manifest 管」。(2) **升級到 SDL_image 3.4.x 會卡**：以 GYO 的 `VENDORED OFF`＋`STRICT ON`，3.4.x 的 PNG 走 `find_package(PNG REQUIRED)`，在沒有系統 libpng 的主機（CI 的 Windows、乾淨的 macOS）configure 失敗（`release-3.4.8/CMakeLists.txt:95-101,131,183-190`）；要嘛開 vendored libpng（多一個相依）、要嘛關 `SDLIMAGE_PNG_LIBPNG` 讓它退回 SDL 本體的 `SDL_LoadPNG_IO`。(3) PNG-only 的需求其實可以**不要 SDL_image**：SDL 3.4.0 本體的 `SDL_LoadPNG` 就夠（但它解出的像素與 SDL_image 3.2.6 是否逐位元相同未查，換掉要重跑 TextureLoader 的特性測試）。
- **判定：維持 SDL_image adapter（現版）；stb_image 不另採；升級或縮減時三選一由 adapter 批決定：(a) SDL_image 3.4.x＋vendored libpng、(b) SDL_image 3.4.x 關 libpng 退回 SDL 本體、(c) 直接用 SDL 本體 `SDL_LoadPNG` 拿掉 SDL_image。** 若之後對不可信影像（使用者上傳的頭像之類）有 Feature，要在 adapter 前加大小上限與格式白名單——那是 Feature 來了才做。
- **要使用者決定的事**：無（現在不升級）。

**讀了什麼**：engine/CMakeLists.txt（`gyo_asset_sdl_image`）、third_party/sdl3_image/CMakeLists.txt、recon-04 §5–§6、對抗檢查 A13。

### 1.14 Compression：zstd、LZ4（功能層）

- **GYO 現況**：無壓縮。網路：protobuf datagram ≤ 1200 B，4 人 Snapshot 契約最大 884 B、合法值 842 B（protocol-v9）；pv10 的結構性做法是每收件者相關性選擇與可靠事件串流（multi-channel-evaluation「先縮帳面預算」），不是壓縮。資產：`gyo.voxel_region` 文字 ≤ 16 MiB，載入瓶頸在 JSON 解析（big-map survey (c)），voxel-mesh RC 6 已寫「最大地圖載入超過宣告值時，與使用者決定二進位快取」。
- **事實**（recon-01 §3–§4）：**zstd 1.5.7**：BSD／GPLv2 雙授權（選 BSD）；C；格式穩定（RFC 8878）；`ZSTD_isDeterministicBuild()`：標準旗標下只保證與「other builds of the same version」相同——**跨版本輸出不保證**（官方措辭只到同版本，A8 結案）；dictionary／prefix 的相鄰性會改輸出（`ZSTD_c_deterministicRefPrefix` 實驗參數）；多執行緒要編譯選項＋nbWorkers；多人維護。**LZ4 1.11.1**：`lib/` BSD-2、`programs/` GPL-2.0-or-later（不要建它們）；C 約 1.3 萬行；大端與小端輸出不同；跨版本輸出相同未查到；`LZ4F_MULTITHREAD` 預設 0；`LZ4_decompress_fast` 已棄用（不防惡意輸入）。
- **限制**：(1) **壓縮位元組不能進 digest 或契約比對**：兩者都不保證跨版本輸出相同，解壓縮才是決定的；(2) 1 KB 的 protobuf 封包（多為 varint 數值）壓縮收益未量，要用 dictionary 才有感，而 dictionary 是協議契約的一部分（版本、分發）；真正的槓桿是 pv10 的差量（對已確認基準）；(3) 資產側 LZ4 解壓很快，但 GYO 的瓶頸是 JSON 解析不是 I/O——先換二進位（RC 6）再談壓縮。
- **判定：不需要（現在）。** pv10 設計時若相關性選擇後仍超 1200 B，zstd dictionary 模式是候選（量級：天，加協議的 dictionary 版本欄）；big-map RC 6 的二進位快取若 I/O 成為瓶頸，LZ4 是候選（量級：天）。
- **要使用者決定的事**：無。

---

## 2. 橫向整理

### 2.1 Kernel／plugin 相容性（對照使用者願景稿）

| 候選 | 自帶迴圈／執行緒／全域 | 資料能否留在自己世界、只經 Stage 交換 | 結論 |
|---|---|---|---|
| EnTT | 無迴圈；signal／organizer 可不用 | 容器配 id、刪除重排 → 和 Match 的 id 契約衝突 | 可當資料層 plugin，但 GYO 要的順序不變式它不給 |
| Flecs | pipeline／system／observer＝scheduler＋event bus；可開 worker | stage／merge 順序非 id 順序 | **和 Kernel 搶位置**，且順序不決定 |
| Jolt | 無迴圈；JobSystem 可單執行緒或外接；**全域** Factory／allocator／Trace | PhysicsSystem 持有位置（第二份狀態） | 可當能力；衝突在狀態權威 |
| Recast／Detour | 無迴圈、無執行緒；`dtAlloc`／`dtAssert` hook 是程序全域 | `dtNavMeshQuery` 有內部可變狀態（節點池），每個查詢者持自己的一份；TileCache 與 Crowd 各有自己的狀態副本（避免或限定） | 可當能力 |
| ImGui／RmlUi | 呼叫端驅動；RmlUi 的 `Initialise`／`Set*Interface`／`RegisterPlugin` 是程序全域 | UI 狀態在它們手上（ImGui 的 io、RmlUi 的 Context） | 可當能力；衝突在資料契約；RmlUi 無法一個 world 一份 |
| miniaudio | engine 層自帶 job 執行緒與 spinlock；device 層 callback | — | device 層可；engine 層和 Kernel 搶執行緒 |
| Effekseer | Manager 有 worker 執行緒選項與 Begin／EndUpdate | 效果實例在 Manager | 關 worker 後可由 Stage 驅動；渲染在它手上 |
| SDL_GPU | 主執行緒規則（swapchain、pump） | — | Kernel 的 Stage 約束要寫「present 只在視窗執行緒」 |
| zstd／LZ4／meshoptimizer | 純函式 | — | 無關 |

結論：使用者願景稿 §12 的「是否需要統一 Scheduler」——沒有一個候選方案能提供 Match 需要的「決定性的 Stage 序列」；Flecs 最像 Kernel，卻明寫順序不保證。Kernel 若要做，是 GYO 自己的協定（架構師 2026-11-10 的看法），候選方案都是它下面的能力。

### 2.2 授權

| 授權 | 候選 | 對 GYO（靜態連結進產品）的影響 |
|---|---|---|
| MIT／BSD／zlib／Apache-2.0／PD | EnTT、Flecs、Jolt、Box2D、Recast、ozz、ImGui、RmlUi、ufbx、stb、SDL_image、meshoptimizer、zstd（選 BSD）、LZ4 lib、SPIRV-Cross、shadercross、wgpu-native、Dawn、Effekseer 核心 | 可；Apache-2.0 要附 NOTICE |
| LGPL 2+ | **OpenAL Soft** | 靜態連結要履行 §6（可重新連結的物件檔、允許反向工程）→ 與硬約束衝突，不採 |
| 混合、含「GPL 3 with special bison exception」字樣 | glslang（LICENSE 說 Bison 已移除）、Effekseer 的 Vulkan runtime（附 glslang 原文） | glslang 不在 GYO 執行期；Effekseer 的那段是 glslang LICENSE 照抄，同檔寫明 Bison 已移除，且自訂後端不用 Vulkan runtime——結案（A15）；授權文字的最終解讀仍不是本文能替使用者做的 |
| GPL-2.0-or-later | LZ4 的 `programs/`、zstd 的另一選項 | 只用 lib、選 BSD；不建 programs |
| 專有 | Autodesk FBX SDK（ozz 的 fbx2ozz） | 不進 repo；ozz 若採要走 glTF 或自寫轉換器 |
| LLVM／NCSA | DXC 含簽章器（1.8.2505 起內建驗證器，不再有獨立 `dxil.dll`） | 結案（A6）；三個 host 的 DXC 是同一版 v1.9.2602 |
| LGPL 2+ 的約束出處 | 「GPL／LGPL 會卡」 | 來自本卡的硬約束清單（使用者 → 架構師），**repo 裡沒有書面出處**（A14）；建議寫進 AGENTS 或 architecture.md，否則下一個評估者只能當成評估者的判斷 |

**獨立問題（不在本卡修，A20）**：repo 裡沒有任何 LICENSE、NOTICE 或第三方授權通知機制（`git ls-files` 無結果），但現有依賴已經有義務——FreeType 的 FTL 要求在文件致謝（經 SDL3_ttf vendored）、protobuf BSD-3 與 nlohmann MIT 要附條款。這和「要不要再加第三方」是同一件事的另一面；建議和上表「LGPL 的約束出處」一起由使用者定（D9）。

### 2.3 維護風險

實質單人維護：EnTT、Flecs、Jolt、Box2D、ozz、meshoptimizer、ufbx（已採）、stb、miniaudio（偵察未查到人數）。GYO 對 ufbx 的做法是釘版本＋可選 adapter＋特性測試，可沿用。停滯：Bullet（不採）、Recast 的 release（但 commit 活躍）、SPIRV-Cross 與 SDL_shadercross 無 release（已採，釘 commit）。

### 2.4 這次評估的方法限制

- 偵察只讀官方 repo、文件、授權檔與少量 issue；GitHub API 中途限流，部分維護者人數、star 數未取得。
- 沒有試建、沒有量測；工時是量級估計。
- 「決定性」一欄只轉述官方聲明與原始碼事實；沒有自己跑任何候選方案的 hash 測試。
- Slang、cgltf／fastgltf：點名但未讀。
- **對抗檢查指出的系統性盲點（A26，已在本版補正）**：(1) 偵察讀了 CMake `option()` 清單，但沒讀**預設值會帶進什麼**（Jolt 的 GPU compute 後端、靜態 CRT、Effekseer 的 network server）；(2) 沒讀 release note 與 API 變更檔（Jolt `APIChanges.md`、DXC `ReleaseNotes.md`、SDL_image 3.2→3.4），三處都改變了結論；(3) 沒讀長期 open 或未合併關閉的 issue／PR（Recast #232、PR #688）；(4) GYO 側的事實沒先對照 dev_log 與 checkup（digest 的範圍、動畫取樣的跨平台結果）；(5) 偵察被未認證的 GitHub API 限流，而本機有已登入的 `gh`。以後每個候選必讀「整合預設值＋GYO 的建置條件」（§2.5）。

### 2.5 採用任何候選時的建置條件（A1、A2、A4、A5）

這一節是 SDL_GPU 教訓的另一面：限制不只在對方的標頭，也在 GYO 自己的建置。

1. **contraction 旗標的順序**：`build/cmake/GyoBuild.cmake:35-42` 在 `add_subdirectory(third_party)` **之後**才加 `-ffp-contract=off`（註解寫「Applies to the directories added below」）。照慣例放進 `third_party/` 的任何函式庫（Jolt、Recast、ozz、miniaudio……）都拿不到它；arm64 的 Clang 預設會融合乘加。採用會碰到結果的函式庫時，要嘛把它加在旗標之後，要嘛對該 target 明確設 `-ffp-contract=off`，並加 configure-time 檢查（像 `gyo_expect_*_links` 那樣）。**已經發生的例子**：ufbx 在旗標之前建置，而它的執行期轉換結果進 `AnimationContentDigest` 的跨平台比對（D0 表）。決定性卡（PR #177）的修正方案 A：把旗標搬到 `add_subdirectory(third_party)` 之前＋configure-time 檢查＋intrinsic 掃描（E2）。
2. **FMA intrinsic 不受旗標管**：ozz 定義 `__FMA__` 時用 `_mm_fmadd_ps`（`simd_math_config.h:44-46`）；Jolt `USE_FMADD` 預設 ON。要決定性就要在 CMake 釘 ISA：Jolt 設 `USE_FMADD=OFF` 或開 `CROSS_PLATFORM_DETERMINISTIC`；ozz 不帶 FMA／AVX2 旗標或用 `ozz_build_simd_ref`。
3. **預設會帶進來的東西**：Jolt 的 `JPH_USE_DX12／VK／MTL／CPU_COMPUTE`（第二條 shader 工具鏈，和 SDL_GPU 持有 GPU 衝突）、Effekseer 的 `NETWORK_ENABLED`、兩者的 examples／tools——全部要在 GYO 的 `third_party/<x>/CMakeLists.txt` 明確關掉，並以連結守衛確認不連 d3d12／Vulkan／Metal／socket。
4. **CRT 與旗標**：Jolt 與 Effekseer 預設靜態 MSVC CRT（/MT），GYO 用 /MD＋app-local CRT 部署檢查；Jolt 的 `Build/` 另加 `/Wall /WX`、IPO。要用 GYO 的旗標重建（Jolt 只 include `Jolt.cmake`；只 include 它時 `JPH_USE_*` 是否預設關閉未實際 configure 確認）。
5. **出貨組態**：Jolt 的 Debug／Release 預設帶 DEBUG_RENDERER／PROFILER，出貨是 Distribution；GYO 的 preset 是 RelWithDebInfo——對應關係要寫下。

這五條是每個「採用」批的事前宣告要寫的項目；本文的工時估計已把它們算進去。

---

## 3. 判定總表

見 [README](README.md)。

## 4. 要使用者決定的事

| # | 事項 | 影響的列 | 建議 | 不照建議的代價 |
|---|---|---|---|---|
| **D0** | **GYO 的決定性要哪一級，各模組為它付了什麼**（使用者 2026-11-10 經 side chat 提出，比本文更上游；**已成架構卡 `v8-arch-determinism-levels`，PR #177，`docs/architecture/determinism.md`，等使用者定 DET-0～DET-3**；它的結論是「邊界」而不是一級：預測核心、host 判斷、契約資料、join 時比對的內容身分要 (c)；被判定不被預測的中間地帶（有動畫的命中、未來 AI）由使用者選 (b) 或 (c)（DET-1）；呈現層 (b)）。三級：(a) 同一程序內可重現（事故回放、L2 分析）；(b) 同一份二進位在同一平台可重現；(c) 跨平台逐位元。現況是**混合**：authority_digest 只對零角度子集要求 (c)，其餘 (b)；動畫取樣 (b)；Collision、Audio、VoxelMesh 的契約按 (c) 設計。各模組為 (c) 付的成本見下表 | 1.2、1.4、1.8、1.11、Math／Collision／Audio 的 DI 用詞 | **這是一張架構卡（上段），排在 D1、D2 之前**：答案若是「只要 (b)」，Collision 的雙精度單次捨入、Audio 的整數混音、Math 的 libm 保留條款都可重新評估，Jolt 與 Detour 的決定性限制也會變輕；若維持 (c)，digest 的強制範圍要從「零角度子集」明寫成契約。EntityStore 的 id 升冪不在這張表上——它守的是「規則結果不依賴歷史」（同 Tick 搶同一件事由 id 定），屬於 (a)，和浮點無關，成本 155 行 | 本文 D1、D2 的判定會在這一題定案後被推翻一次 |
| D1 | 路線圖上有沒有**權威的剛體玩法**（可推的道具、載具、會影響判定的物理）？**依 D0 卡：Collision 留在預測核心、(c) 不放；Jolt 進權威仍要 `CROSS_PLATFORM_DETERMINISTIC`＋`USE_FMADD=OFF`＋跨平台 golden（determinism.md §6 (i)(ii)）** | 1.2 | 現在的 Feature 清單沒有 → **維持 Collision；Jolt 留給 Client 端視覺物理**。有的話，照「基盤先定」排一批 Jolt 基盤評估（含 digest 範圍的重新宣告與 CI、§2.5 的建置條件），量級 4～8 週 | 若之後才加剛體玩法，Collision 的 RC 1／2 會在 Feature 中途觸發，付的就是使用者說的「事後補」 |
| D2 | entity-requirements 4-6「AI 只要 Match 自己一致、不要求兩台 Match 一致」是否維持？**這題現在等於 D0 卡的 DET-1**：中間地帶（未來 AI）選 (c) → AI 決策也要 (c)，Detour 的 libm 呼叫不能進判定路徑（換掉 `dtMath*` 或走格子 A*）；選 (b) → 本列判定維持 | 1.4 | 維持 → Recast／Detour 可作第一候選（AI 批事前宣告時定） | 若改成要一致，導航要換掉 libm 呼叫並補 hash CI，或改格子 A* |
| D3 | AI 批開工前，要不要先做一次「Recast 對體素地圖建圖」的試驗？量：建圖時間、tile 數、PolyRef 位元、最長路徑的節點數（對 65,535）、兩台 host 烘焙的 blob 是否逐位元相同 | 1.4 | 要（量級 1～2 天），讓 AI 批的事前宣告有數字。**同一份宣告也要寫 Match 每 Tick 取樣 25 個骨架（4 人＋20 AI＋Boss）的預算**（1.11、A11） | 不做則 AI 批的估時只能靠推 |
| D4 | 技能的特效是否要美術用編輯器製作？ | 1.12 | 可等技能批；答案決定 (a) 最小自製粒子或 (b) Effekseer | 若要編輯器級，Effekseer 的 SDL_GPU 後端＋烘焙時 MaterialCompiler 是 5～10 週 |
| D5 | 是否請圖書管理員把 Render 後端的 Replacement Criteria 補進 rendering_architecture（GPU 查詢、async compute、bindless、render 執行緒、Metal 阻塞超預算、**depth 格式寫死 D32_FLOAT 在某台機器不支援**）？ | 1.10 | 要（文件工作） | 沒有 RC 就沒有停下來的條件 |
| D6 | 對抗檢查者用了 Opus／high（不是卡片寫的 Fable／high；原因見 HANDOFF）。接受，或再加一輪 Fable？ | 全部 | 接受；Fable 已是評估者，再用 Fable 檢查是同模型 | — |
| D7 | pv10 的背包（格子 10 × 6、40 件、拖放）要走 `GYO::Ui` 契約的下一版（v2 元素集），還是當成 GYO::Ui 的 RC 觸發（「修還是換 RmlUi」）？ | 1.5 | 走 v2 契約（背包是格子與拖放，不是 rich text／腳本；RC 的條件沒到）；在 pv10 設計前定，因為它影響 ui_editor 的下一批 | 到 pv10 中途才發現 Ui 的元素集不夠，就是 Editor／Runtime 契約在 Feature 中途變 |
| D8 | pv10 的 wire 要不要**預留** codec／dictionary 版本欄（為之後的 zstd dictionary 壓縮留位置）？ | 1.14 | 預留一個 enum 欄（成本是幾個 byte 與一條契約規則）；不實作壓縮 | 不預留則壓縮進來時是另一次 proto 變更（protocol-v8 §2a 的規則） |
| D9 | 「GPL／LGPL 會卡」要不要寫成 AGENTS 或 architecture.md 的書面約束？repo 沒有 LICENSE／NOTICE，而現有依賴（FreeType FTL 致謝、protobuf BSD-3、nlohmann MIT）已有附條款的義務——要不要開一張獨立的卡處理第三方授權通知？ | §2.2 | 兩件都要：約束寫下、通知另開卡（不在本卡） | 下一個評估者只能把 LGPL 當成評估者的判斷；出貨包缺通知 |

**D0 的成本表**（各模組為「跨平台逐位元」(c) 付了什麼；若決定只要 (b)，這些是可重新評估的項目）：

| 模組 | 為 (c) 付的 | 守衛 | 若只要 (b) 可放掉的 |
|---|---|---|---|
| `GYO::Math` | 全部程式 `-ffp-contract=off`、MSVC `/fp:precise`；sin／cos 等明寫不跨平台一致（所以 (c) 其實只覆蓋不含三角函數的算式） | 特性測試、`GyoBuild.cmake` | contraction 旗標仍要留（同平台的 arm64／x64 也會因 FMA 不同），放掉的是「跨平台」這個宣稱 |
| `GYO::Collision` | 每個查詢在 double 計算、對 float 捨入一次；`StaticAabbGrid` 逐位元等於線性掃描 | 200 組隨機集合比對、`grid-*` 突變 | 雙精度單次捨入可改成單精度（更快）；等價契約仍有價值（可用暴力掃描驗證），但可放寬成「同一組候選」 |
| `GYO::Audio` | 整數混音（Q16、int64、半上捨入）、固定 48 kHz PCM16、DI 8 | 「output does not depend on how blocks split it」等 | 可換 float 混音（開 RC 2 的 buses／effects 之路）；L2 的錨點重算改容差比對 |
| `GYO::VoxelMesh`／`MapMarkers` | 整數單位（1/16 cell、±2^20）、`cell_size` 範圍限制讓展開在每平台得到同樣的 float 位元 | golden 位元測試 | 幾乎不必放：整數契約的成本是零 |
| `GYO::EntityStore` | （不是為 (c)）id 升冪的不變式、保序 erase | `es-row2-*` 突變 | **不放**：它守的是 (a) 的「不依賴歷史」，與浮點無關 |
| 產品 `authority_digest` | 零角度子集的 L1 三平台 golden；其餘情境只記錄差異 | L1 `cpu` 標籤 | 若只要 (b)：golden 改同平台兩樹；若維持 (c)：要明寫子集的定義 |
| 動畫取樣（`GYO::Model`） | 已經是 (b)（Slerp 走 libm） | 同平台 golden（hitbox-golden-policy） | — |
| **`GYO::AssetUfbx`（第三方 ufbx）** | **今天就在 (c) 的關卡上，但沒有付**：`AnimationContentDigest` 以 ufbx 執行期轉換的結果在 join 時逐位元比對（protocol-v9 §4.6），而 ufbx 在 `-ffp-contract=off` 之前建置、用平台的 sin／cos（決定性卡核對） | 沒有守衛（決定性卡提 E5：字面 golden；長期：烘焙成 GYO 格式） | 這是 §2.5 第 1 條「第三方拿不到旗標」已經發生的例子，不是將來的風險 |

其餘 10 列（ECS、2D Physics、UI 的維持部分、Model Import、Mesh Optimization、Audio、Shader Compiler、GPU Abstraction 的維持部分、Animation、Image Processing）的判定不需要使用者現在決定；各列的 RC 觸發時再問。

## 5. 對抗檢查（`v8-arch-third-party-evaluation-adversary`）

- **檢查者**：general-purpose 子代理，Opus 5.5／high，只讀 repo（報告寫到證據目錄）。卡片寫 Fable／high；因主會話（評估者）由使用者設為 Fable，依領航員規則「與受檢者不同模型」改 Opus（D6）。
- **讀了**：評估全文、README、HANDOFF、五份偵察報告、AGENTS、各契約、Feature 清單、兩份既有 survey；自己再讀 Jolt／Recast／ozz／Effekseer／miniaudio／DXC／SDL_image／RmlUi／zstd／meshoptimizer 的原始碼、release note、API 變更檔與 issue（本機已登入的 `gh`，避開限流），以及 GYO 的 `GyoBuild.cmake`、`tests/object_fps_pvp/CMakeLists.txt`、`SdlGpuRenderDevice.cpp`、`third_party/sdl3_image/CMakeLists.txt`。
- **原文**：主 checkout `build/evidence/third-party-evaluation/adversarial-check-20261110.md`（234 行）。
- **結果**：14 列**沒有否定**；成立 8（1.1、1.3、1.5、1.6、1.7、1.9、1.10、1.14）、成立但理由／事實／措辭要修 4（1.2、1.8、1.12、1.13）、弱 2（1.4、1.11）。發現 26 條：must 7、should 15、note 4。總評指出三處系統性的漏洞，正好和 SDL_GPU 同類：沒讀建置整合的預設值、沒核對 GYO 自己的建置、GYO 的決定性現況寫錯。
- **處置**：26 條全部接受（含 4 條部分接受並說明）；評估者在修正前核對了 A1、A3、A13、A20、A22、A24 的 repo 事實（`GyoBuild.cmake:35-42`、`tests/object_fps_pvp/CMakeLists.txt:405-452`、`third_party/sdl3_image/CMakeLists.txt`、`git ls-files`、`SdlGpuRenderDevice.cpp:17`、`wc -l`），全部屬實。判定改 1 處（1.4 採用 → 第一候選）；理由改寫 2 處（1.2、1.11）；事實改正 3 處（1.8、1.13、1.14）；新增 §2.5、D7～D9。

### 5.1 逐條回應

| # | 級別 | 列 | 發現（一句話） | 處置 |
|---|---|---|---|---|
| A1 | must | 1.2、1.4、1.11 | `-ffp-contract=off` 在 `add_subdirectory(third_party)` 之後才加，第三方拿不到 | **接受**。核對 `GyoBuild.cmake:35-42` 屬實。新增 §2.5 第 1 條；1.2 限制 7、1.4 限制 8 引用 |
| A2 | must | 1.2、1.11 | FMA intrinsic 不受旗標管（ozz `__FMA__`、Jolt `USE_FMADD`） | **接受**。§2.5 第 2 條；1.2 事實表標 FMADD 預設 ON |
| A3 | must | 1.2、§0 | 「Match 的 digest 跨平台逐位元比對」說過頭；實際只對零角度 golden 子集跨平台強制，其餘依 libm 合法地不同；Jolt 的 CROSS_PLATFORM_DETERMINISTIC 反而比 GYO 的 Math 嚴 | **接受**。核對 `tests/object_fps_pvp/CMakeLists.txt:405-452` 與 `authority_digest_check.cmake` 屬實。1.2「GYO 現況」加一條、限制 1 重寫為「條件不同向」 |
| A4 | must | 1.2 | Jolt v5.6 預設編 DX12／Vulkan／Metal 的 GPU compute 後端 | **接受**。1.2 事實表與限制 7(c)、§2.5 第 3 條 |
| A5 | should | 1.2、1.12 | 靜態 CRT、`/Wall /WX`、IPO 與 GYO 的 /MD、CRT 部署檢查衝突 | **接受**。1.2 限制 7(d)、1.12 (7)、§2.5 第 4 條 |
| A6 | should | 1.9、§2.2 | dxil.dll 可結案：1.8.2505 起內建驗證器；NCSA；三個 host 同版 | **接受**。1.9 限制 (2) 改寫；§2.2 的「未查到」列換成結案；D3D12 runtime 拒絕無效雜湊的一手規格仍標「只從 release note 推得」 |
| A7 | should | 1.8 | miniaudio 內建解碼器是 WAV／MP3／FLAC，沒有 stb_vorbis；解碼會配置、不能在音效執行緒跑 | **接受**。1.8 事實與 RC 1 段改寫（解碼放工具側烘焙成 PCM16） |
| A8 | should | 1.14 | zstd 官方只保證同版本，跨版本是「不保證」不是「未查到」 | **接受**。1.14 改寫；LZ4 維持「未查到」 |
| A9 | note | 1.7 | meshoptimizer 是 C++11；allocator 失敗必須 throw 或 abort；決定性確認無說明 | **接受**。1.7 改寫 |
| A10 | must | 1.11、README 11 | 前提「Match 與 Client 取樣要一致」與 repo 衝突；README／限制／判定三處對 ozz 給什麼不一致；廠商差異要附來源 | **接受**（評估者在收到檢查前已自查改寫前提，見 HANDOFF 22:20）。補 `sampling_job.cc` 的 Est 函數與第二手來源（標明第二手）；README 第 11 列改成「IK／取樣效能」 |
| A11 | should | 1.11、§4 | AI 20 隻＋Boss 的 Match 端取樣預算沒寫 | **接受**。1.11 判定段加一句；併入 D3 的事前宣告要求 |
| A12 | must | 1.4、README 4 | 「採用」替使用者定了未排的 Feature；漏了節點池 65,535、query 有狀態、TileCache 每次一個 tile、光柵化依輸入順序、PR #688 未合併、11 件不可信資料 issue、全域 hook、`DT_POLYREF64`／`DT_NAVMESH_VERSION` 要進契約 | **接受**。判定改「第一候選，AI 批事前宣告時由使用者定」；限制欄從 8 條擴到 10 條；D3 加兩個量測；§2.1 的「純函式」改正 |
| A13 | must | 1.13、README 13 | SDL_image 3.4.x 的 PNG 走 libpng 不是 stb；GYO 只開 PNG 沒開 JPEG；非 vendored＋STRICT 升 3.4.x 會 configure 失敗；README 理由與正文矛盾；漏了 SDL 本體 `SDL_LoadPNG` | **接受**。核對 `third_party/sdl3_image/CMakeLists.txt` 屬實（JPG OFF、PNG ON、STB ON、VENDORED OFF、STRICT ON）。1.13 整節重寫，三選一的升級路徑；README 第 13 列改寫 |
| A14 | should | 1.8、§2.2 | OpenAL「LGPL → 不採」理由弱：動態連結可履行、architecture.md 允許 runtime 庫部署在旁；「GPL／LGPL 會卡」repo 無出處；device 層對應 RC 3；miniaudio 單人維護 | **部分接受**。不採的主理由改為 float 混音（DI 8）與沒有 Feature；LGPL 改為「本卡硬約束清單（使用者 → 架構師）的約束，repo 無書面出處」並列為 D9；RC 對應改正；補維護資料。不接受的部分：約束本身不因無書面出處而失效——它是使用者在卡片上給的，評估照它判；要不要寫下由使用者定 |
| A15 | should | 1.12、§2.2 | Effekseer 的 GPL 字樣是 glslang LICENSE 照抄且 Bison 已移除，可結案；漏了 MaterialCompiler 在執行期編 HLSL、`NETWORK_ENABLED` 預設 ON、靜態 CRT、examples／tools 預設 ON | **接受**。1.12 事實與限制 (5)(7) 改寫；工時估計從 4～8 週改 5～10 週；§2.2 該列結案（仍註明授權文字的最終解讀不是本文能替使用者做的） |
| A16 | should | 1.12 | 「預設先自製、契約名 `gyo.vfx`」替技能批定了做法 | **接受**。改為選項 (a)(b) 由 D4 決定；契約名拿掉 |
| A17 | should | 1.2、README 2 | README 漏「權威剛體」路徑；Jolt 每 minor 改模擬結果、序列化不相容；DEBUG_RENDERER／PROFILER 預設 ON | **接受**。README 第 2 列寫兩條路徑；1.2 事實表與限制 1、7(e) 加入 |
| A18 | should | §4 | 缺基盤決定點：背包走 Ui v2 還是 RC；pv10 wire 預留 codec 欄 | **接受**。新增 D7、D8（附建議與不照建議的代價） |
| A19 | should | §4 | 「其餘 8 列」實為 9 且漏 Animation | **接受**。改為 10 列並補 Animation、UI／GPU 的維持部分 |
| A20 | should | §2.2 | repo 無 LICENSE／NOTICE 機制，現有依賴已有通知義務 | **接受**。核對 `git ls-files` 屬實。§2.2 加「獨立問題」段；D9 |
| A21 | should | 1.5 | RmlUi 全域初始化、官方 SDL_GPU 渲染器內嵌格式與 GYO 的 Metallib 不合、直接 include SDL3_image | **接受**。1.5 限制加第 5 條；§2.1 該列加全域 |
| A22 | note | 1.10 | GYO 把 depth 寫死 `D32_FLOAT`，SDL_GPU 只保證 D24 或 D32 其一，無退路 | **接受**。核對 `SdlGpuRenderDevice.cpp:17` 屬實。1.10 限制 (f)；D5 的 RC 候選加這條 |
| A23 | note | 1.1 | EnTT 20 bit 可用 `ENTT_ID_TYPE` 放寬，真正的坑是 sparse 頁表隨最大 id 增長 | **接受**。1.1 限制 2 改寫；判定不變 |
| A24 | note | §0.1 | Render 4,300 行含 shader 工具 `main.cpp`（624 行），不含是 26 檔／3,676 行；「4 個 uniform 槽」是每 stage 上限、GYO 只用槽 0 | **接受**。核對 `wc -l` 為 3,676。§0.1、1.10 改數字與說法 |
| A25 | note | §0 | 1.6、1.9、1.13 標題缺「基盤／功能層」；「純函式」用語不對 | **接受**。標題補標記；用語改正 |
| A26 | should | §2.4 | 偵察的五個系統性盲點（預設值、release note、長期 issue、GYO 側對照、限流） | **接受**。§2.4 照實寫入；§2.5 作為以後每個「採用」批的事前宣告項目 |

### 5.2 檢查者仍查不到、本文也不補的事

D3D12 正式 runtime 拒絕無效雜湊 DXIL 的一手規格；LZ4 跨版本輸出；meshoptimizer 的決定性；Recast 建圖跨 host 是否逐位元（→ D3 的試驗）；只 include `Jolt.cmake` 時 `JPH_USE_*` 的預設；SDL 本體 `SDL_LoadPNG` 與 SDL_image 3.2.6 的像素是否逐位元相同；Effekseer 匯出檔是否內含預編譯材質；miniaudio 0.12 的破壞性變更；Intel／AMD 的 rcp／rsqrt 差異只有第二手來源。這些都已在各列標明「未查到」或「第二手」。
