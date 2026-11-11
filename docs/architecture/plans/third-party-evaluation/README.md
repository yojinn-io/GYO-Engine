# 第三方方案評估（`v8-arch-third-party-evaluation`）

更新：2026-11-10。Owner：架構（Engine 契約的判斷）。卡片由架構師會話（「[Job][架構師]例行架構掃描」）開出。
**狀態：評估完成、對抗檢查（Opus／high）26 條全部處置（判定 1 處改：Navigation「採用」→「第一候選」），停下等使用者決定（evaluation §4 的 D0～D9；**D0「決定性要哪一級、各模組付了什麼」是上游的架構卡，排在 D1、D2 之前**）。只寫文件：不改程式、不加依賴、不改契約。**

2026-11-11（架構師）：使用者定方向「Bevy 的抽象能力帶入現有 GYO、改造完成前功能線停」（`plans/kernel/mechanism.zh-Hant.md`）。影響：ECS 列待重判（evaluation §1.1 末）；D1～D4、D7、D8 延後；其餘葉子層判定不受影響（改造不動葉子）。

## 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-third-party-evaluation` | 使用者 2026-11-10 問「Engine 裡有大量重複的輪子，是否需要自己實作」；對 14 個子系統逐列比較自製與成熟開源方案，給判定與取捨，不替使用者決定 |

## 文件

- [逐列評估](evaluation.zh-Hant.md)：每列的 GYO 現況、Feature 需求、候選方案的事實、**限制（採用後會卡在哪裡）**、判定、代價、要使用者決定的事；末節是對抗檢查與逐條回應。
- [交接（HANDOFF）](HANDOFF.md)：記錄器（起因、模型／檔位、估時、經過、決定）。
- 來源清單：主 checkout `build/evidence/third-party-evaluation/sources.txt`（git 不追蹤；偵察子代理的原始報告也在同一夾）。

## 目的與範圍

- **目的**：讓「自製還是採用」這個決定能被重現：換一個人讀同樣來源，應得到同樣的事實與同樣的取捨。判定以 **Feature** 為準（已排或已點名的 Feature 需要它才算需要），不以「有這個函式庫」為準（AGENTS §2 Derivation Order）。
- **範圍**：使用者給的 14 列（ECS、Physics、2D Physics、Navigation、UI、Model Import、Mesh Optimization、Audio、Shader Compiler、GPU Abstraction、Animation、VFX、Image Processing、Compression）。
- **不做**：不整合、不試建、不量測；不改 Design Invariants／Replacement Criteria（只指出採用時要改哪些）；不替使用者決定 Feature 方向。
- **為什麼要寫限制**（使用者 2026-11-10）：SDL_GPU 採用後才發現的限制（Metal 的 swapchain 取得會阻塞到約 1 秒、swapchain 與事件 pump 只能在視窗執行緒、shader 要自己備齊三種格式與 host 工具鏈）都是整合前就能從原始碼與文件讀到的。這次每列先寫「採用後會卡在哪裡」，對抗檢查專門找沒寫出來的限制。

## 判定總表

| # | 子系統 | GYO 現況 | 需要它的 Feature | 判定 | 一句話理由 |
|---|---|---|---|---|---|
| 1 | ECS | `GYO::EntityStore`（容器版） | AI 小怪、場景線（3b `DeferredChanges`） | **自製（維持）** | 已有 Bevy／EnTT 參照調查與 DI／RC；兩個候選都不保證 id 升冪迭代，為決定性要再排序 **2026-11-11：前提變更（mechanism），待規格後重判；Scheduler 自寫已定** |
| 2 | Physics | `GYO::Collision`（膠囊／盒子精確查詢＋靜態格） | 場景線的門（動態盒）、技能投射物；**沒有**剛體玩法 | **並存：權威留自製；Jolt 兩條路徑——(a) Client 端視覺物理 Feature 出現時採（不進 digest）；(b) 權威剛體玩法是基盤決定（D1）** | 現在沒有需要剛體的 Feature；Jolt 進權威要重寫 broadphase 契約、狀態權威、建置條件（§2.5），且每個 minor 改模擬結果 |
| 3 | 2D Physics | 無 | 無 | **不需要（現在）** | 沒有 2D 產品 |
| 4 | Navigation | 無 | AI 小怪與 Boss（已點名，未排） | **第一候選（Recast 離線建圖＋Detour 查詢）；AI 批事前宣告時由使用者定（D2、D3）** | 體素地圖 → navmesh 由工具烘焙、GYO 索引帶 sha256；Detour 單執行緒、查詢物件各持狀態、節點池 65,535、無官方決定性保證；entity-requirements 4-6 只要求 Match 自己一致 |
| 5 | UI | `GYO::Ui`（遊戲內）＋Dear ImGui（工具） | 既有 | **自製（維持）；RmlUi 不採** | ImGui 自述不適合玩家面對的 UI；RmlUi 要接自家渲染介面與 FreeType，和 GYO::Ui 的封閉元素集與 editor 重疊 |
| 6 | Model Import | ufbx（已採） | 既有 | **維持 ufbx；Assimp 不採** | Assimp 依賴樹與授權清單大、FBX 品質問題多；只有 FBX 一種來源 |
| 7 | Mesh Optimization | 無 | big-map（體素面已 greedy 合併）、無 LOD Feature | **不需要（現在）**；LOD Feature 出現時候選 | 體素幾何由工具烘焙，不是 index 重排的對象 |
| 8 | Audio | `GYO::Audio`（整數混音、sample-accurate 排程） | 既有 | **自製（維持）；miniaudio 當 RC 1／2 的候選** | 自製是為了 sample-accurate 與逐位元決定性；miniaudio 是 float 混音、資源管理器自帶執行緒 |
| 9 | Shader Compiler | DXC＋SDL_shadercross＋SPIRV-Cross（已採，host 工具） | 既有 | **維持；不換 glslang** | 已釘版本；限制在 Metallib 要 macOS 原生與 Xcode |
| 10 | GPU Abstraction | SDL3 GPU（已採） | 既有 | **維持；wgpu／Dawn 不採** | 換後端＝重寫 adapter＋shader 格式；wgpu-native 是 Rust 建置、WebGPU 的限制比 SDL_GPU 更多 |
| 11 | Animation | `GYO::Model`（取樣、混合、CPU 蒙皮、事件） | AI 小怪的動畫（同一套）、寵物 | **自製（維持）；ozz 當「需要 IK／大量角色取樣效能」時的候選** | ozz 不給 blend tree／GPU 蒙皮；它的近似 SIMD 讓同一份 Match 二進位在不同 CPU 上可能不同（R5 的「Match 自己可重現」要收窄） |
| 12 | VFX | 無 | 技能（範圍效果、投射物表現）、互動物件的純表現 | **不需要（現在）；技能批開工時評估：先自製最小粒子，Effekseer 要自寫 SDL_GPU 渲染器** | Effekseer 沒有 SDL_GPU 後端，要實作它的渲染介面並轉它的 shader |
| 13 | Image Processing | SDL3_image 3.2.6 adapter（已採，只開 PNG） | 既有 | **維持現版；stb_image 不另採；升級時三選一（3.4.x＋libpng／3.4.x 退回 SDL 本體／直接用 SDL `SDL_LoadPNG`）** | 現在的 PNG 已經是 stb（經 SDL_image），再採 stb 只是多一份；3.4.x 的 PNG 改走 libpng，GYO 的 STRICT＋非 vendored 會在無系統 libpng 的主機 configure 失敗 |
| 14 | Compression | 無 | pv10（snapshot 1200 B 預算）——未點名壓縮 | **不需要（現在）**；pv10 設計時若預算不夠才評估 LZ4／zstd | 1200 B 的 datagram 壓縮收益要先量；資產載入時間（big-map）另有 RC 6 的二進位快取 |

逐列的根據、限制與代價見[逐列評估](evaluation.zh-Hant.md)；採用任何候選時 GYO 自己的建置條件見 evaluation §2.5。總表已照對抗檢查定稿；判定的更動記在 HANDOFF。

## 要使用者決定的事

見[逐列評估 §4](evaluation.zh-Hant.md#4-要使用者決定的事)（D0～D9；D0 是上游問題，使用者 2026-11-10 經 side chat 提出）。

## dev_log

- [2026-11-10 `v8-arch-third-party-evaluation`](../../../dev_logs/2026_11_10_arch_third_party_evaluation.zh-Hant.md)
