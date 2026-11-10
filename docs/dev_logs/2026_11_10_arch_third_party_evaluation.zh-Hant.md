# 架構 v8：v8-arch-third-party-evaluation（第三方方案 vs 自製子系統的評估）

日期：2026-11-10。分支 `claude/arch-third-party-evaluation`（切自 master `a75152d8`）。卡片由架構師會話（「[Job][架構師]例行架構掃描」）開出。
段：上段（Architecture 判斷的根據：Engine 契約、Design Invariants、依賴）。評估者：主會話 Fable 5.1／high（卡片指定 Opus／high；主會話模型由使用者設定）；
偵察：5 個子代理（general-purpose、Opus 5.5／high、只讀）；對抗檢查者：Opus 5.5／high（卡片寫 Fable／high；因評估者已是 Fable，依領航員規則「與受檢者不同模型」改 Opus，列為決定事項 D6）。

**狀態：** 評估完成、對抗檢查處置完成，停下等使用者決定（evaluation §4 的 D1～D9）。只改文件，沒有程式變更、沒有新依賴、沒有契約變更。

相關文件：[計畫 README](../architecture/plans/third-party-evaluation/README.md)（判定總表）、[逐列評估](../architecture/plans/third-party-evaluation/evaluation.zh-Hant.md)、[HANDOFF](../architecture/plans/third-party-evaluation/HANDOFF.md)。

## 起因

使用者 2026-11-10：「我發現現在 Engine 裡面有大量重複的輪子，我在想我是否需要自己來實作這些。」給了 14 列（ECS、Physics、2D Physics、Navigation、UI、Model Import、Mesh Optimization、Audio、Shader Compiler、GPU Abstraction、Animation、VFX、Image Processing、Compression）與候選方案。
同日補充：開對抗檢查的原因是 SDL_GPU 的例子——採用後才發現的限制要先講清楚。因此每列加「限制（採用後會卡在哪裡）」一欄。

## 做了什麼

- 讀 GYO 現況（架構總覽、各模組契約與 DI／RC、渲染架構、framework、UI 工具鏈、big-map／scene／pv10／entity 需求、entity-store 與 big-map 的既有 survey、使用者的架構願景稿），盤點每個自製模組的行數、連結、測試行數、突變數（evaluation §0.1）。
- 5 個偵察子代理平行讀 23 個候選方案的官方 repo、文件、授權檔、CI 設定與少量 issue，只回「事實＋URL」；報告原文存主 checkout `build/evidence/third-party-evaluation/recon-01～05.md`。
- 每列寫：GYO 現況與為什麼自製｜需要它的 Feature｜候選方案的事實｜限制｜判定｜代價與不採的代價｜要使用者決定的事｜讀了什麼。
- 橫向整理：Kernel／plugin 相容性（對照使用者的 Bevy 式 Kernel 願景）、授權、維護風險、方法限制。
- 對抗檢查（Opus／high、只讀），結果與逐條回應附在 evaluation §5；處置後新增 §2.5（採用時 GYO 自己的建置條件）。

## 判定（對抗檢查後的定稿；更動記在 HANDOFF）

| 判定 | 列 |
|---|---|
| 自製（維持） | ECS（EntityStore）、UI（GYO::Ui；ImGui 只在工具）、Audio（GYO::Audio）、Animation（GYO::Model） |
| 並存 | Physics：權威判定留 GYO::Collision；Jolt 只在「Client 端視覺物理」或「權威剛體玩法」Feature 出現時採（後者是基盤決定 → D1） |
| 維持已採 | Model Import（ufbx）、Shader Compiler（DXC＋SDL_shadercross＋SPIRV-Cross）、GPU Abstraction（SDL_GPU）、Image（SDL_image） |
| 第一候選（對抗檢查後由「採用」改） | Navigation（Recast 離線建圖＋Detour 查詢），AI 批事前宣告時由使用者定（D2、D3） |
| 不需要（現在） | 2D Physics、Mesh Optimization、VFX（技能批再評估，D4）、Compression |

## 查證過的事實（判斷用到的，各附來源於 recon 報告）

- Jolt：跨平台決定性要 `CROSS_PLATFORM_DETERMINISTIC`（約慢 8%）＋應用程式 precise／contraction off／不用 std::sort、std::hash、標準 sin/cos；broadphase 查詢、listener 順序、GetActiveBodies 順序不保證；文件自承「less tested」；CharacterVirtual 的 GetMaxHitsExceeded 破壞決定性（#1551）。
- Bullet：無決定性選項、MSVC 預設 /fp:fast、CI 只有 Linux、近一年 3 commit。
- EnTT／Flecs：刪除都重排（swap-and-pop／搬尾）；Flecs 的 system 與 observer 順序明寫不保證；EnTT 預設 entity 20 bit。
- Recast／Detour：右手座標、PolyRef 32 位（salt ≥ 10 位）、單執行緒、無官方決定性聲明、`dtMath*` 包裝可換、不可信資料越界 issue。
- ImGui FAQ：「NOT designed to create a user interface for games」；SDL_GPU 後端內嵌 DXBC（非 DXIL）與 MSL／Metallib。
- RmlUi：渲染介面要 stencil clip、layers、filter、shader 才有完整效果；官方 SDL GPU 範例缺這些。
- miniaudio：engine／node graph 只收 f32；排程「may not be 100% exact」；資源管理器自帶 job 執行緒。OpenAL Soft：LGPL 2+。
- SDL_GPU：無 bindless（not planned）、無 query（open）、無 pipeline cache（open）、4 uniform slot、swapchain 限建視窗執行緒。wgpu-native 是 Rust；Dawn 主要 Clang 19+／GN；WebGPU 無 bindless／push constant。
- ozz：SSE 版用 `_mm_rcp_ps`／`_mm_rsqrt_ps` 近似指令；archive 無向下相容；fbx2ozz 要 FBX SDK。
- Effekseer：無 SDL_GPU 後端；GraphicsDevice 介面含 StorageBuffer／ComputeShader；Vulkan runtime 授權檔附 glslang 的 GPL-bison 字樣。
- SDL_image 預設內建 stb_image 做 PNG／JPEG；stb_image 有 29 件 CVE（NVD 關鍵字）。
- zstd／LZ4 跨版本輸出相同：未查到；LZ4 大小端輸出不同。

## 對抗檢查

Opus 5.5／high 子代理，只讀；自己再讀候選方案的原始碼、release note、API 變更檔與 issue（本機已登入的 `gh`），並對照 GYO 的 `GyoBuild.cmake`、`tests/object_fps_pvp/CMakeLists.txt`、`SdlGpuRenderDevice.cpp`、`third_party/sdl3_image/CMakeLists.txt`。

- 結果：14 列無否定；成立 8、成立但要修 4（1.2 理由、1.8 事實、1.12 措辭、1.13 事實）、弱 2（1.4、1.11）。發現 26 條（must 7／should 15／note 4）。
- 三處系統性漏洞（都和 SDL_GPU 同類）：(1) 偵察只讀 CMake `option()` 清單、沒讀預設值帶進什麼——Jolt v5.6 預設編 DX12／Vulkan／Metal 的 GPU compute 後端、靜態 MSVC CRT、`/fp:fast`；Effekseer 預設 `NETWORK_ENABLED=ON`。(2) 沒核對 GYO 自己的建置——`-ffp-contract=off` 在 `add_subdirectory(third_party)` **之後**才加（`build/cmake/GyoBuild.cmake:35-42`），放進 `third_party/` 的函式庫拿不到；FMA intrinsic（ozz `__FMA__`、Jolt `USE_FMADD`）本來就不受旗標管。(3) GYO 的決定性現況寫錯——L1 的 `authority_digest` 四平台都跑，但只對零角度 golden 子集強制，其餘依 libm 合法地不同（`math.md`）；動畫取樣已知跨平台不一致（hitbox-plan R5／B3）。
- 處置：評估者核對 A1、A3、A13、A20、A22、A24 的 repo 事實（全部屬實）後，26 條全部接受（A14 部分接受：「GPL／LGPL 會卡」是卡片的硬約束，不因 repo 無書面出處而失效；要不要寫下列為 D9）。
- **判定改 1 處**：Navigation「採用」→「第一候選，AI 批事前宣告時由使用者定」（原文替使用者定了未排的 Feature）。理由改寫 2 處（Physics、Animation）、事實改正 3 處（Audio、Image、Compression）、結案 2 處（dxil.dll 簽章、Effekseer 的 GPL 字樣）。新增 evaluation §2.5「採用任何候選時的建置條件」與 §4 的 D7～D9。逐條回應在 evaluation §5.1。

## 用時

約 2 h（21:10～23:10），低於估的 8.5～11 h：偵察平行、既有的 entity-store／big-map survey 可沿用。代價是評估者自己讀來源的深度不足，由對抗檢查補上（26 條）；已寫進 evaluation §2.4 的方法限制。

## 驗證

只改文件。`git diff --stat` 見 PR；無 Build、無 Test 變動。

## 證據

主 checkout（`/Users/karasu/Code/Source/GYO-Engine`，Intel Mac）`build/evidence/third-party-evaluation/`（git 不追蹤）：`recon-01～05.md`（偵察原文）、`sources.txt`（來源清單）、`adversarial-check-20261110.md`（對抗檢查原文）。
偵察的 shallow clone 在會話暫存區，未保存（只用來計數與讀檔，行數為當日值）。
