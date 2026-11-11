# 審查部分報告：`product-thirdparty`（`v8-arch-kernel-audit`）

更新：2026-11-11。Owner：架構（架構師會話的子代理；審查者 Opus／high）。只讀：未改程式、未改契約。三份部分報告之一，主會話合併成 `audit.zh-Hant.md` 並逐列核對；本檔的判定是**提案**，不是結論。

## 0. 範圍、件數、證據、讀過的檔

- **範圍**：`apps/object_fps_pvp/src/*`（依現有子目錄每個一列）＋`third_party/` 每個庫一列；另加 #176 候選（Recast／Detour、Jolt、miniaudio、EnTT）與 host 工具鏈的「事實列」（不在 repo，依 #176 事實判，標「依 #176 事實」）；`apps/object_fps`、`apps/object_fps_v2`、`services/gyo_gateway` 寫「未審」列。
- **掃描**：`python3 …/kernel/scripts/audit_scan.py --repo <worktree> --out build/evidence/kernel-audit/product-thirdparty --roots apps/object_fps_pvp/src third_party`（腳本未改；worktree 為 master `1cff5413`）。輸出 `modules=14`。
- **件数の突き合わせ**：**一致**——掃描模組數 14＝表 1 的列數 14＋掃描根內的「未審」列 0。不在掃描根、另列的：表 2 的 `src/Pvp` 子列 13（同一模組的分解，不計入件數）、補充列 1（`apps/object_fps_pvp/{main,match_main}.cpp`）、事實列 5（候選 4＋host 工具鏈 1）、未審列 3（掃描根之外）。
- **證據的位置**（主 checkout，git 不追蹤；本機 macOS，路徑 `/Users/karasu/Code/Source/GYO-Engine/build/evidence/kernel-audit/product-thirdparty/`）：
  - `audit_hits.csv`、`audit_summary.csv`：上面那次掃描。本文的 `hits:N` ＝ `audit_hits.csv` 的第 N 行（第 1 行是表頭）。
  - `supplement-app-root/audit_hits.csv`：同一支腳本、`--roots apps/object_fps_pvp` 的補充掃描（理由見觀察 2：`src` 根不含 `include/` 的 6,867 行標頭與兩個 `main`）。本文的 `supp:N` ＝ 該檔第 N 行。
  - `thirdparty_manual_grep.txt`：第三方原始碼（主 checkout `build/target/_build/test/_deps` 的 FetchContent 複本）的人工 grep；`tp:N` ＝ 該檔第 N 行。產生它的腳本在會話 scratchpad（會消失），內容是 grep 指令，已逐行保留在輸出檔的標題列。
  - `h7_include_scan.txt`：(b) 節的 H-7 掃描清單。
- **讀過的檔**：事前宣告；spec v0.2 §0、B.3／B.3.1、G、H（全）、I、K.3、L；mechanism §1；`docs/architecture/target/` 全部（含 limits LIM-1～LIM-5）；determinism.md §4；#176 README 判定總表、evaluation §0.1、§1.1、§1.2、§1.4、§1.5～1.8、§1.13、§2.1～2.5；entity-model §3.1～3.2（第 139 行）、§5.1～5.4。程式：`apps/object_fps_pvp/src/**` 與 `include/RetroFPS/Pvp/**`（逐檔讀入口、成員與寫入點；未逐行讀完每個檔）、`main.cpp`、`match_main.cpp`、`tests/object_fps_pvp/AuthorityDigest.cpp`（情境、編碼、時鐘）、`fixtures/authority_golden.txt`、`third_party/*/CMakeLists.txt`。

**判法（架構師 2026-11-11 對齊；以 spec I.3 為準，取代事前宣告「Q1 為是 → 組織層」的寫法）**：
- Q1 為是（有生命週期或更新迴圈）→ **只排除「留」**；「包」或「重寫」由 Q2～Q4 決定：Q2～Q4 皆否 → **包**；任一為是 → 偏重寫那一層介面，或先拆掉那些狀態再包（本報告記為 **包†**）。
- **組織層**（Host／重寫骨架）＝模組自己**排序其他元件**（寫死的呼叫序、擁有迴圈或執行緒並決定誰先誰後），對應 spec H-7。只是「每 tick 被呼叫一次的 Tick」不算。
- **留**＝純函式且三否定皆滿足。第三方：**葉子**＝純函式、無全域、無執行緒；**包**＝有狀態或每幀入口、可框進 Resource 加 System；**服務**＝自己擁有執行緒或裝置回呼。
- 初稿用的是另一種讀法（Q1 分「迴圈／執行緒」與「僅建立」兩種答案，只有前者判組織層）。已逐列回頭核對；兩種讀法結果不同的列，表中兩個都寫並標「最終」，清單在 §0.1。

### 0.1 兩種讀法結果不同的列

| 列 | 初稿讀法 | 對齊後的讀法（**最終**） | 差在哪 |
|---|---|---|---|
| `third_party/imgui` | 包 | **包†** | Q3 是（`GImGui` 全域 context），照新判法不能直接包；第三方的全域拆不掉，靠 effect `Gpu:imgui` 序列化並明列為例外 |
| `third_party/sdl3_ttf` | 包 | **包†** | 同上（`TTF_state` 全域） |
| `third_party/asio`、`httplib`、`protobuf` | 服務 | 字面：包（asio、httplib 不擁有執行緒；protobuf 是包†，全域 pool）；**最終：服務的內部實作** | 新的第三方定義以「是否擁有執行緒或裝置回呼」判服務；這三個本身不擁有，但只在擁有執行緒的 IO 服務（P3、P4）裡用、不進 World，所以落點仍是服務 |
| P10 呈現 | 包 | **包†** | 補看 `Engine::Time::Now` 後 Q3 為是（`PvpSound.cpp:95`、`:150`，僅診斷） |
| P13 內容 | 留 | **留**（`NativeFiles` 除外：**包**） | `NativeFiles` 有檔案的 `Open`／`Close`（Q1 是）→ 排除留；Q2～Q4 皆否 → 包 |
| Recast／Detour、Jolt（事實列） | 包 | **包†** | Q2、Q3 為是（跨 tick 的 query／PhysicsSystem 狀態、程序全域 hook／Factory） |

其餘列兩種讀法結果相同：組織層的列（P1、P2、P5、P6、P11、S1）在新讀法下仍因「排序其他元件」判重寫或 Host（各列理由已改寫成這個依據）。

## 1. 表 1：掃描模組（14 列）

行數取 `audit_summary.csv`（只含掃描根內的 `.cpp`／`.hpp`）。工時是粗估區間（實作＋測試＋比對，不含 Kernel 本體），不是承諾。

| 模組 | 行數 | Q1 生命週期／更新迴圈 | Q2 跨 tick 指標或參考 | Q3 全域可變狀態（含時鐘、亂數） | Q4 自起執行緒碰 World 資料 | zone 初判 | 判定 | 理由 | 工時粗估 | 受 LIM 影響 |
|---|---|---|---|---|---|---|---|---|---|---|
| `src/App` | 867 | 是（僅建立）：`WeaponViewModel.cpp:196` Initialize、`:108`、`:111` Create（hits:2–6） | **是**：`WeaponViewModel.cpp:45-46` 的 `IRenderDevice* device`、`AssetManager* assets` 成員跨幀保存（掃描**未命中**：regex 要求成員名以 `_` 結尾，人工發現；LIM-3） | 否：掃描無命中＋人工確認（`*Definition.cpp` 是 JSON 載入函式） | 否：掃描無命中＋人工確認 | B | **包†**（兩種讀法相同） | Q1 只排除留；Q2 的兩個 pimpl 裸指標指向將成為 ClientRender／ClientFrame Resource 的物件，改成每次由 System 傳入句柄後，狀態（模型實例、姿勢）可整包成 Resource | 6–12 h | LIM-1（讀 Combat 呈現）、LIM-3 |
| `src/Collision` | 163 | 否：掃描無命中＋人工確認（`CharacterCollision.cpp` 是模板與自由函式） | 否：掃描無命中＋人工確認（`span<const VerticalCapsule>` 參數，`:59`、`:79`） | 否：掃描無命中＋人工確認 | 否：掃描無命中＋人工確認 | C（移動的預測核心，determinism.md §4.3） | **留** | 純查詢；由 Movement 呼叫 | 0–2 h | — |
| `src/Gameplay` | 43 | 否：掃描無命中＋人工確認 | 否：掃描無命中＋人工確認 | 否：掃描無命中＋人工確認；`PlanarMovement.cpp:17-18` 呼叫平台 libm `sin`／`cos`（不是全域狀態，是 E1 的跨平台問題，determinism.md §4.8） | 否：掃描無命中＋人工確認 | C | **留** | 純函式；E1 換 Math 自己的三角函式時改 | 0–1 h | — |
| `src/Pvp` | 9,152 | **是（迴圈／執行緒）**：見表 2 的 P1～P6、P11（例：`PvpMatch.cpp:520` hits:183；`MatchRuntimeHost.cpp:64` hits:101） | **是**：P2～P5（例：`IpcHost.cpp:419` 的 `MatchRuntimeHost& host`） | **是**：P2～P4、P11、P12；P1、P6 經呼叫路徑（例：hits:97–148 的時鐘；`ClientConnection.cpp:400` hits:22） | **是**：P2～P5（hits:17、33、82、103） | 混合（見子列） | **重寫**（組織層；子列各自判） | 一個目錄裝了 Match、Client 兩個 host、兩個 IO 服務、(c) 規則、呈現與診斷；單一判定沒有意義，以表 2 的子列為準 | 250–480 h（子列合計） | LIM-1、LIM-3、LIM-5 |
| `third_party/asio`（asio-1-38-2） | 0（FetchContent） | 是（迴圈）：`io_context` 事件迴圈，由呼叫者的執行緒驅動（GYO：`gyo-match-ipc` `IpcHost.cpp:891`、`gyo-client-net` `ClientConnection.cpp:205`） | 否（函式庫本身）；GYO 側的 handler 捕獲 `this`（P3、P4） | 未逐檔確認（掃描 0 檔；tp:48） | 否：GYO 只用同步 resolve（`ClientConnection.cpp:415-416`），不觸發 resolver 執行緒（tp:48，未在 asio 原始碼確認） | 不適用（邊界外，B） | 字面：包（本身不擁有執行緒）；**最終：服務的內部實作**；親和＝IO 執行緒 | 只在 Link 外側、擁有執行緒的 IO 服務（P3、P4）裡用；不從 System 呼叫 | 0（隨 P3、P4） | — |
| `third_party/doctest`（v2.4.12） | 0 | 不適用（只連進測試） | 不適用 | 有（測試登錄表），不進執行期 | 不適用 | 不適用 | **不適用**（測試基礎設施） | 不在任何 Runtime；spec J 的守衛照用 | 0 | — |
| `third_party/httplib`（v0.56.0） | 0 | 否（GYO 只用 `httplib::Client`，`ClientConnection.cpp:82-83`）；伺服器端 ThreadPool 未使用（tp:27–32） | 否 | 否（GYO 用法內）：tp:27–32 的執行緒都在 server／async 路徑 | 否：同步呼叫在 `gyo-client-net` 上（`ClientConnection.cpp:390` 的 `Handle`） | 不適用（B） | 字面：包（socket 狀態、不擁有執行緒）；**最終：服務的內部實作**；親和＝`gyo-client-net` | Lobby HTTP 是 IO 服務（P4）的一部分；`async_thread_`（tp:29）在哪條路徑**未確認** | 0（隨 P4） | — |
| `third_party/imgui`（v1.92.5） | 0 | 是（僅建立）：context 建立與每幀 `NewFrame`／`Render` 由呼叫者驅動 | 否 | **是**：`GImGui` 全域 context（tp:3；`imgui.cpp:1383`） | 否 | B（只在工具） | 初稿：包；**最終：包†**（全域 context 拆不掉，明列例外）；親和＝主執行緒，effect `Gpu:imgui` | 與 spec L.2 的「包」方向一致，但 Q3 為是；不在本範圍的產品裡（`framework/bindings/imgui` 與 tools 用） | 0（本範圍） | — |
| `third_party/nlohmann_json`（v3.12.0） | 0 | 否 | 否 | 否：`thread_local`／`std::thread` 計數 0（tp:11）；人工未見可變全域 | 否 | C（內容載入；小數經 `strtod`，determinism.md §4.6） | **葉子** | 純解析；在 Startup／Host 初始化與工具呼叫 | 0 | — |
| `third_party/protobuf`（v36.2） | 0 | 否（GYO 用法：wire 編解碼） | 否 | **是**：generated descriptor pool 為程序全域、延遲建立、內部以 mutex 保護（tp:22–24；`descriptor.cc:2310`、`:2355`） | 否 | 不適用（B，wire） | 字面：包†（全域 pool）；**最終：服務的內部實作**（現況只在 IO 執行緒：`IpcHost.cpp:7`、`ClientConnection.cpp:13`）；親和＝IO 執行緒 | 若改造後由 Egress System（(b)）直接編碼，generated pool 要比照 I-5 明列例外——**不確定**，交遷移批 | 0（隨 P3、P4） | — |
| `third_party/sdl3`（release-3.4.0，含 SDL_GPU） | 0 | **是（迴圈／執行緒）**：事件 pump、音訊回呼執行緒；swapchain／pump 限視窗執行緒（#176 evaluation §0、§1.10） | 是（裝置與視窗控制代碼跨幀保存，屬 Engine 後端） | **是**：SDL 全域初始化狀態、GYO 後端的 `thread_local`（spec I.1 引 `SdlPlatform.cpp:61` 等；tp:35 只取到 GPU 標頭一句） | 是（音訊回呼執行緒），不碰 World（經 Submitter） | 不適用（B） | **服務**（音訊回呼）／**Host**（pump、live frame）／**包**（GPU 裝置＝渲染 World 的 Resource，親和 `GraphicsContextOwner`） | 一個庫三種落點，照 spec I.1、H-3；判定在 Engine 列（`engine/platform`、`render`、`audio`）落實 | 0（本範圍） | LIM-2（不影響：在 World 外） |
| `third_party/sdl3_image`（release-3.2.6） | 0 | 否：`IMG_Load_IO`（tp:37）是無狀態入口 | 否 | 否：人工未見 `static … initialized` 類全域（tp:36–37；只看了 `IMG.c`） | 否 | B（資產載入） | **葉子**（asset loader 內呼叫） | 只開 PNG（stb 後端）；在載入路徑上呼叫 | 0 | — |
| `third_party/sdl3_ttf`（release-3.2.2，含 FreeType） | 0 | 是（僅建立）：`TTF_Init` 參照計數（tp:18–20） | 否 | **是**：`TTF_state` 全域（tp:13–17；`SDL_ttf.c:386`） | 否 | B | 初稿：包；**最終：包†**（全域拆不掉，明列例外）；親和＝建立它的執行緒（主執行緒） | 與 spec L.1 `text` 後端「包」方向一致，但 Q3 為是 | 0（本範圍） | — |
| `third_party/ufbx`（v0.23.0） | 0 | 否：純載入；thread pool 是可選、由使用者提供（tp:46–47），GYO 未提供 | 否 | 否（出貨組態）：`thread_local` 只在 `UFBXI_IS_REGRESSION` 組態（tp:38–45；`ufbx.c:1055-1070`） | 否 | **C（內容身分）**：Match 與 Client 在執行期轉換 FBX，結果進 `AnimationContentDigest` 跨平台比對（determinism.md §4.9）；`ufbx_euler_to_quat` 用平台 `sin`／`cos`（tp:6–9） | **葉子**（Startup／Host 初始化） | 判定是葉子，但它是「在 contraction 旗標之前建置的第三方浮點進 (c) 比對」的實例（#176 §2.5 第 1 條） | 0 | — |

## 2. 表 2：`src/Pvp` 的子列（13 列；同一掃描模組的分解，不計入件數）

行數＝該組 `src/Pvp/*.cpp` 的 `wc -l`，合計 9,152＝表 1；`include/` 的標頭依歸屬併入各組讀過，但不計行數。

| 子列 | 行數 | Q1 | Q2 | Q3 | Q4 | zone 初判 | 判定 | 理由 | 工時粗估 | LIM |
|---|---|---|---|---|---|---|---|---|---|---|
| P1 `PvpMatch`（＋`PvpMatch.hpp`、`Combat.hpp`） | 715 | **是（迴圈）**：`PvpMatch.cpp:520` `Tick`（hits:183；supp:102）內寫死 `ResolveLifeBoundaries`（`:526`）→ 移動（`:571`、`:582`）→ `ResolveActions`（`:617`）→ `AdvanceAnimation`（`:623`） | 否：tick 內參考（`:424-425` 的 `targetState`、`Decide` 的參數）只活在一次 `Tick`；跨 tick 成員只有不可變的 `hitModel_`（`PvpMatch.hpp:202`，`shared_ptr<const>`）；`ShotReferenceAge` 是參數不是成員（`Combat.hpp:228`，supp:19） | **是（經呼叫路徑）**：`TraceMovement`（`:514`、`:586`）→ 全域 sink（`MovementTrace.hpp:101-102`，supp:84–86）；`MovementDiagEnabled`／`RecordMovementDiag`（`:569-580`）→ `MovementDiag.hpp:188`（supp:77）。`PvpMatch.cpp` 自身無 static：掃描無命中＋人工確認 | 否：掃描無命中＋人工確認 | **C**（預測核心＋判定） | **重寫**（成 (c) plugin；`Decide`、`ResolveActions` 的本體大半可搬成 System 本體；兩種讀法相同） | 組織層：`Tick` 以寫死的呼叫序排序生死、移動、裁決、動畫四段（不只是「每 tick 被呼叫一次」）；Body 三段寫入（§5.1）必須拆欄；診斷的全域入口要拆（§7 第 3 條） | 40–80 h | LIM-5、LIM-3 |
| P2 `MatchRuntimeHost`（＋`MatchIngressLedger`、`MatchSubstitutionRecords`、`MatchShotTrace`、`MatchIngressTrace` 標頭） | 814 | **是（迴圈／執行緒）**：`Start`／`Stop` `:62-76`、`RoleThread` `gyo-match-sim` `:64`（hits:100–104）、`Advance`／`Step` `:362-369`（hits:118–120）、一把 `mutex_` 包整個 `Step` `:371`（hits:122） | **是**：`publishListener_`（`MatchRuntimeHost.hpp:278`，supp:50）、`now_` 時鐘回呼（`.hpp:73`、`:77`，supp:36–38） | **是**：`now_()` `:218`、`:443`、`:669`；`Body` 直接讀 `steady_clock` `:522-533`（hits:128–132） | **是**：`gyo-match-sim`（hits:103）；IO 執行緒經 `SubmitInput` 進入並**寫權威**（`:253` → `PvpMatch.cpp:255-256`，§7 第 4 條） | 混合：Host（B）＋Ingress（B）＋Judge（host judgement，(c)：`JudgeConnectionQuality` `:700-760` 寫 `connectionQualityFailures` 進 digest） | **重寫**：Host（時鐘、執行緒、交換點）＋Ingress／Judge／Egress System | 組織層：`Step` 寫死 join／leave → ack → 動作 → 輸入交接 → `match_.Tick` → `TrackSlack` → `JudgeConnectionQuality` → 發布的順序（`:385-480`），並擁有執行緒。與 spec H.4.1 一致；另發現 IO 執行緒直接寫權威的第二處（`RequestMovementResync`） | 40–70 h | LIM-5、LIM-3 |
| P3 `IpcHost`（＋`Wire`、`WireCounter`） | 943 | **是（迴圈／執行緒）**：`Start`／`Stop` `:874`、`:907`（hits:79、83）、`Pump` `:710`（hits:69）、`RoleThread` `gyo-match-ipc` `:891`（hits:80–82） | **是**：`MatchRuntimeHost& host`（`:419`）跨整個連線 | **是**：`steady_clock`（hits:44–53） | **是**：`gyo-match-ipc` 同步呼叫 `host.SubmitInput`（`:645`）、`host.SubmitActionBatch(…, &refusal)`（`:685`） | B | **服務**（位元組收送）＋**重寫**（同步碰權威的兩處改經 Link） | spec L.3 一致；`:645` 是 spec 未列的同類點 | 20–40 h | LIM-3 |
| P4 `ClientConnection`（＋`ConnectionLiveness`、`ClockSync`、`InputSendBudget`、`LobbyActions`、`AddressText`） | 1,093 | **是（迴圈／執行緒）**：`RoleThread` `gyo-client-net` `:205`、`Run` `:922`（hits:15–18、24）、`mutex` `:130`、atomics（hits:11–13） | **是**：`RoleContext* role`（`:202`，人工） | **是**：`<chrono>`、`<random>`（hits:8–10）、函式內 `static std::atomic nextRequest`（`:400`，hits:21–22） | **是**：`gyo-client-net`（hits:17）；模擬執行緒呼叫 `SendInput`／`DrainSimulation`（`ClientSimulationRole.cpp:196-197`） | B（Welcome 時的 `AnimationContentDigest` 比對是 (c) 內容身分的檢查點，`:312-314`） | **重寫**：asio 層→服務＋Link；連線狀態→Resource | spec L.3 一致 | 30–50 h | LIM-3 |
| P5 `ClientSimulationRole`、`ClientSimulation`（＋`ClientSimulationHistory`、`PredictionElapsedTime`） | 287 | **是（迴圈／執行緒）**：`RoleThread::Start` `:151`（hits:31–33）、`loop_.Run` `:196`（hits:39）、`mutex_` 交換 `:161-211`（hits:34–42） | **是**：`ClientConnection& connection_`（`ClientSimulationRole.hpp:209`，supp:13） | **是**：`Engine::Time::Now()` `:190`（人工；掃描樣式不含它；hits:29 只是 `<chrono>`） | **是**：`gyo-client-sim`（hits:33） | Host 時鐘（B）；內部跑 (c) 預測 | **重寫**：ClientSim Runtime＋Host 的時鐘策略 | 組織層：`Body` 排序「取意圖 → `loop_.Run` → `SendInput` → 發布呈現 → 等 deadline」（`:188-205`）。spec H.4.2 一致；「何時跑下一步」本報告也判 Host（不是 System） | 20–40 h | LIM-3 |
| P6 `LocalPlayerPrediction` | 539 | **是（迴圈）**：`Advance` `:347`、`ticks_.Advance` `:410`（hits:84、87）；`Reconcile` `:267` | 否：掃描無命中＋人工略讀未見跨 tick 參考（成員為值）；**不確定度中**（未逐行讀完） | **是（經呼叫路徑）**：`MovementDiag` `:21`、`:31`；`TraceMovement` `:110`、`:140`、`:249`、`:281`、`:418`、`:441` | 否：掃描無命中＋人工確認 | C（`PredictStep`＝`StepMovement`）＋B（`TrackPhase` 相位，`:176`） | **重寫**（回滾機制的使用者；H.5 的部分快照只含本地玩家；兩種讀法相同） | 組織層：自己擁有固定步迴圈（`ticks_.Advance` 決定跑幾步）並排序重放、預測、相位判斷與 trace；只看 Q2～Q4 會是包†，但它的職責（重放、步數）由機制的 Rewind／Replay 與 Host 的時鐘取代。spec L.3 一致 | 30–60 h | LIM-3 |
| P7 `Movement.cpp`（`StepMovement`） | 112 | 否：掃描無命中＋人工確認 | 否：掃描無命中＋人工確認 | 否：`Movement.cpp` 無全域、不讀時鐘；`diagnostics` 是輸出參數（`:44`、`:80`）；`Movement.hpp:8` include `<chrono>`（supp:63，型別用） | 否 | C | **留** | 純函式 (state, command)→state；全域在呼叫點（P1、P6），不在這裡 | 0–4 h | — |
| P8 `MovementDiag`、`MovementTrace`、`MovementTraceWriter`（標頭 only，src 0 行） | 0 | **是（執行緒）**：`std::thread` `MovementDiag.hpp:109`（supp:65–66，**不是 RoleThread**）、`Run` `:156`、`mutex`／`condition_variable` `:176-177` | 否 | **是**：`movementDiagWriter`（`:188`，supp:77）、`movementTraceSink`＋mutex（`MovementTrace.hpp:101-102`，supp:84–86）、`steady_clock`（`MovementTrace.hpp:59`，supp:79） | 自起執行緒，但只收事件複本，不碰 World：否 | B | **服務**（Diagnostics）；全域入口要改成句柄 | 見 §7 第 3 條與觀察 3（(c) 不得有 effect） | 8–16 h | LIM-3 |
| P9 射擊、姿勢、動畫（`ShotQuery`、`ShotGate`、`PlayerHitModel`、`PlayerHitbox`、`PlayerPose`、`PlayerAnimation`） | 1,210 | 否：掃描無命中＋人工確認 | 否：掃描無命中＋人工確認（`PlayerHitModel` 由呼叫者以 `shared_ptr<const>` 持有） | 否：掃描無命中＋人工確認；平台 libm：`ShotQuery.cpp:82-84`、`PlayerAnimation.cpp:172`、`PlayerPose.cpp:363`（E1） | 否 | `PlayerAnimation` 推進＝C（進 digest 的 animations 段）；`PlayerPose`／`PlayerHitbox`／`ShotQuery` 命中幾何＝中間地帶（DET-1，E1 前 (b)） | **留**（System 本體） | spec L.3 一致；`ShotReferenceAge` 的改法在 P1／P2 | 2–8 h | LIM-1（呈現讀姿勢） |
| P10 呈現（`PlayerPresentation`、`HitFeedback`、`PvpSound`、`SoundCues`、`PointerCapture`、`SnapshotTimeline.hpp`） | 829 | 是（僅建立）：`PlayerPresentation::Initialize` `:115`、`Resource::Create` `:54`（hits:159–160）；`PvpSound::Initialize` `:35`、`Mixer::Create` `:68`、`SdlAudioOutput::Open` `:75`（hits:185–187） | 否：人工確認 `PlayerPresentation` 的 Impl 不存裝置指標；`PvpSound` 擁有 `Submitter`／output | **是（時鐘）**：`PvpSound.cpp:95`、`:150` 讀 `Engine::Time::Now()`（僅診斷；掃描樣式不含它，人工補看發現）；全域可變：掃描無 static 命中＋人工確認；時刻是欄位（`PointerCapture.hpp:23`，supp:97） | 否：音訊回呼執行緒在 Engine audio 內 | B | 初稿：包；**最終：包†**（時鐘讀值改由 `FrameTime` 類 Resource 傳入後包成 ClientFrame 的 Resource 與 System；`PvpSound` 的 output 是服務、`Submitter` 有親和） | spec L.3「包」的方向一致 | 10–20 h | LIM-1 |
| P11 `PvpApplication` | 1,453 | **是（迴圈）**：`Update` `:1130`、`Run` `:1382`、`RuntimeLoop` `:1403`、`PumpEvents` `:1114`、`BeginFrame` `:1111`、`:1141`（hits:174–182） | **是**：live frame handler 捕獲 `loop`（`:1389`）；UI 回呼捕獲 `this`（`:526`，hits:165） | **是**：命名空間級 `applicationStart = Clock::now()`（`:71`，人工）；`Clock::now()` 多處（hits:162–163） | 否（自己不起；擁有的 P4、P5 起） | B | **重寫**：Client host＋ClientFrame／ClientRender 的 System | 組織層：`Update`（`:1130-1193`）在一個函式內排序資產、輸入、UI、意圖發布、呈現。spec H.4.2 一致 | 40–80 h | LIM-1 |
| P12 診斷（`ClientLog`、`LogFile`、`NetworkStatistics`；`ClogLine`、`SessionSummary`、`TracePaths` 標頭） | 432 | 是（執行緒）：`thread_local Waiter`（`ClientLog.cpp:37`，hits:26–27）、`TraceSink::Open`（`LogFile.cpp:109`，hits:94） | 否：`LogFile& owner_`（`:100`，hits:92）在同一物件內 | **是**：`std::atomic<LogFile*> crashLog`（`LogFile.cpp:117`，hits:95）、`inline std::mutex clogLineMutex`（`ClogLine.hpp:13`，supp:16–17）、`system_clock`（hits:88–91） | 否（writer 執行緒在 Engine trace） | B | **服務**（Diagnostics） | spec L.3 一致 | 4–10 h | — |
| P13 內容（`Arena`、`ArenaMesh`、`MovementRulesFile`、`NativeFiles`、`PlayerAnimationContent`、`PlayerAnimationDigest`） | 725 | 是（僅建立）：`NativeFiles` 的檔案 `Open`／`Close`（hits:149–155） | 否：`std::function` 是參數不是成員（`PlayerAnimationContent.cpp:74`，hits:158） | 否：掃描無命中＋人工確認 | 否 | C（內容身分：`ArenaContentDigest` `Arena.cpp:174-198`、`AnimationContentDigest` `PlayerAnimationDigest.cpp:54-108`） | 初稿：全部留；**最終：留**（`Arena`、`ArenaMesh`、`MovementRulesFile`、`PlayerAnimationContent`、`PlayerAnimationDigest`；Startup 或 Host 初始化呼叫）＋**包**（`NativeFiles`：有檔案生命週期，Q2～Q4 皆否；走 `Io:file` effect 的 (b) System 或 Host） | spec L.3 只寫「留」；`NativeFiles` 是 L.3 沒分出來的 | 2–6 h | — |

補充列（不在掃描根；`supplement-app-root`）：

| 列 | 行數 | Q1 | Q2 | Q3 | Q4 | zone | 判定 | 理由 | 工時 | LIM |
|---|---|---|---|---|---|---|---|---|---|---|
| S1 `apps/object_fps_pvp/main.cpp`、`match_main.cpp` | 399 | 是（迴圈）：`match_main.cpp:169-170` 每 50 ms 醒、`:152-153` Start、`:194` 停止順序（supp:298–302）；`main.cpp:167` `application.Run()`（supp:308） | 否 | 是：`steady_clock`（supp:294–304）、`std::atomic<LogFile*> logFile`（`main.cpp:40`，supp:306） | 否（經 P2、P3 起） | B | **Host** | 行程入口與停止順序，spec H.7 | 10–20 h | — |

## 3. 表 3：事實列（不在 repo；依 #176 事實）與未審列

| 列 | Q1 | Q2 | Q3 | Q4 | zone | 判定（第三方；附執行緒親和） | 理由 |
|---|---|---|---|---|---|---|---|
| Recast／Detour（依 #176 事實） | 否：無迴圈、無執行緒（evaluation §2.1、§1.4 第 7 條） | **是**：`dtNavMeshQuery` 有內部可變狀態（節點池），每個查詢者跨 tick 持有一份（§1.4 第 9 條） | **是**：`dtAlloc`／`dtAssert` hook 是程序全域（§1.4 事實） | 否 | Match 端 AI；「只要 Match 自己一致」時為 (b)，要進 (c) 等 DET-1／D2 | 初稿：包；**最終：包†**（navmesh 與每個查詢者的 query 物件是 Resource；hook 比照 I-5 明列例外；Recast 建圖在工具，不適用執行期）；親和＝擁有該 World 的執行緒 | #176 寫「可當能力」；spec v0.2 下有全域與跨 tick 狀態就不是葉子 |
| Jolt（依 #176 事實） | 否（無迴圈；JobSystem 可單執行緒或外接，§1.2 第 9 條） | **是**：`PhysicsSystem` 持有 body 位置與速度（第二份狀態，§1.2 第 3 條） | **是**：Factory、allocator、Trace 程序全域（§1.2 第 6 條） | 否（可設單執行緒）；外接 job 時是 | 視覺物理＝B（ClientFrame／Render）；權威剛體＝與 State Authority 衝突 | 初稿：包；**最終：包†**（只限 (b) World；全域 Factory 等明列例外）；權威路徑在本機制下不成立，除非 Jolt 成為位置欄的唯一寫者（D1） | #176 寫「可當能力；衝突在狀態權威」——與機制的 B.3 一致，收窄到 (b) |
| miniaudio（依 #176 事實） | **是（執行緒）**：engine 層自帶 job 執行緒與 spinlock；device 層 callback（§2.1、§1.8 第 3 條） | 未記 | 未記（#176 無） | 是（engine 層） | 不適用（B） | **服務**（device 層＝與今天 SDL 音訊回呼同位置）；engine 層**不相容**（搶執行緒） | 與 #176 一致 |
| EnTT v4（依 #176 事實） | 否：無迴圈；signal／organizer 可不用（§2.1） | 未記（#176 記「刪除 swap-and-pop 重排」，表示疊代中的位置不穩） | #176 未記（**未查證**） | 否：不開執行緒；registry 非 thread-safe（§1.1 表） | 視用途 | **不判**：World 儲存層「借還是自寫」不在本批（事前宣告「不在本批做」）；事實照抄 | 待 World 實作批以 W-1～W-11 逐條核對 |
| host 工具鏈：DXC、SDL_shadercross、SPIRV-Cross（`build/cmake` 釘版本；不在 `third_party/`） | 不適用 | 不適用 | 不適用 | 不適用 | 不適用（建置期） | **不適用**（不連進任何 Runtime） | #176 §1.9；Kernel 不涉及 |
| 未審：`apps/object_fps` | — | — | — | — | — | 未審 | 使用者 K.3 第 8 項：移除（獨立批 `v8-arch-remove-object-fps`） |
| 未審：`apps/object_fps_v2` | — | — | — | — | — | 未審 | 同上 |
| 未審：`services/gyo_gateway` | — | — | — | — | — | 未審 | 外部服務（Go）；另一行程，經訊息邊界接入（spec I.1）。同理：`apps/object_fps_pvp/gateway`（Go）與 `protocol/` 不在掃描根，未審 |

## 4. 判定分佈

最終判定（對齊後的讀法）：

- 表 1（14 列）：留 2（Collision、Gameplay）；包† 1（App）；重寫 1（`src/Pvp` 整體）；第三方：葉子 3（nlohmann、sdl3_image、ufbx）、包† 2（imgui、sdl3_ttf）、服務 4（sdl3 自己擁有回呼執行緒；asio、httplib、protobuf 是服務的內部實作；sdl3 另含 Host 與包的部分）、不適用 1（doctest）。
- 表 2（子列 13＋補充 1）：重寫 6（P1、P2、P4、P5、P6、P11）；服務 2（P8、P12）；服務＋重寫 1（P3）；包† 1（P10）；留 3（P7、P9、P13，其中 P13 的 `NativeFiles` 為包）；Host 1（S1）。
- 事實列 5：包† 2（Recast／Detour、Jolt 限 (b)）、服務 1（miniaudio device 層）、不判 1（EnTT）、不適用 1（工具鏈）。未審 3。

## 4.1 掃描會漏的三類與 `Engine::Time::Now`（人工補看；LIM-3 的範圍）

架構師要求補看的四項，本範圍的結果：

| 類別 | 找到的 | 已反映在 |
|---|---|---|
| 第三方全域 context（SDL、ImGui 類） | 產品原始碼不直接呼叫 `SDL_*` 或 `ImGui::`（grep 無命中，`src`、`include`、兩個 `main`）；全域在 Engine 後端與第三方本身：`GImGui`（tp:3）、`TTF_state`（tp:13）、protobuf generated pool（tp:22）、SDL 全域（spec I.1 已列） | 表 1 imgui、sdl3_ttf、protobuf、sdl3 列 |
| pimpl 裡跨 tick 的裸指標 | 檢查了 6 個 Impl（`WeaponViewModel`、`PlayerPresentation`、`IpcHost`、`PvpApplication`、`PvpSound`、`ClientConnection`）：`WeaponViewModel::Impl` 的 `IRenderDevice* device`、`AssetManager* assets`（`WeaponViewModel.cpp:45-46`）；`IpcHost::Impl` 的 `WakeGate{Impl* impl}`（`IpcHost.cpp:386`，跨執行緒共享）與 `MatchRuntimeHost& host`（`:419`）；`ClientConnection::Impl` 的 `RoleContext* role`（`:202`）。`PlayerPresentation`、`PvpSound`、`PvpApplication` 的 Impl 在讀到的範圍內沒有裸指標成員（`PvpApplication::Impl` 只看了 `:95-260`） | App 列、P3、P4 |
| lambda／`std::function` 捕獲 `this` 並跨 tick 保存 | 保存的：`IpcHost.cpp:464` 的 publish listener（捕獲 `gate`，存進 `MatchRuntimeHost::publishListener_`，由 sim 執行緒呼叫進 IPC 端——跨元件、跨執行緒的回呼）；`PvpApplication.cpp:1389` 的 live frame handler（捕獲 `this`、`loop`，存進 `SdlPlatform`）；`PvpApplication.cpp:526` 的 clip source（捕獲 `this`）；RoleThread 主體 `MatchRuntimeHost.cpp:65`、`ClientSimulationRole.cpp:152`、`ClientConnection.cpp:206`（執行緒存活期間）；asio handler `IpcHost.cpp:475`、`:480`、`:558`、`:788`、`:800`（IO 服務內）。其餘 `[&]` 都是當場用完 | P2、P3、P11 的 Q2 |
| 時鐘樣式 `Engine::Time::Now` | `ClientSimulationRole.cpp:190`、`PvpSound.cpp:95`、`:150`（僅診斷）、`ClientLog.cpp:42`（log 等待的 deadline） | P5、P10（因此改判包†）、P12 |

## 5. (a) Body 拆欄的範圍與受影響的 golden

### 5.1 `bodies_`（`PlayerState`）今天的寫入點（全列）

| # | 位置 | 寫的欄位 | 呼叫來源 | entity-model 第 139 行的組 |
|---|---|---|---|---|
| W1 | `PvpMatch.cpp:63-70` `Join`（`InsertEntity`） | 整列初值 | host（Tick 外） | 成員關係（生成） |
| W2 | `PvpMatch.cpp:88` `EraseEntity` | 整列刪除 | `Leave`（Tick 外）、驅逐（`MatchRuntimeHost.cpp:757`，Step 內、Tick 後） | 成員關係（刪除） |
| W3 | `PvpMatch.cpp:301-304` `Kill` | `respawnTick`、`lifeState`、`lifeStateTick` | `Decide` 內（`:434`），即 `ResolveActions`（`:617`） | 生命 |
| W4 | `PvpMatch.cpp:314-342` `ResolveLifeBoundaries`（復活） | `lifeGeneration`（`:327`）、`lifeState`、`lifeStateTick`、`respawnTick`（`:328-330`）；`position`、`yaw`、`pitch`、`verticalVelocity`、`grounded`、`crouched`、`sprinting`（`:331-337`） | `Tick` 第一步（`:526`） | 生命＋身體 |
| W5 | `PvpMatch.cpp:493-499` `ResetMovementEpoch` | `movementEpoch`（`:497`）、`lastResolvedCommand`（`:498`）、`contiguousPendingCommands`（`:499`） | 復活（`:342`）、排定的重設（`:534`）、序號用盡（`:544`） | 輸入游標 |
| W6 | `PvpMatch.cpp:571`、`:582` `state = StepMovement(…)` | 整份 `PlayerState` 賦值；實際改身體七欄與 `lastResolvedCommand`（`Movement.cpp:104`） | `Tick` 移動迴圈 | 身體＋輸入游標 |
| W7 | `PvpMatch.cpp:585` | `contiguousPendingCommands` | `Tick` 移動迴圈 | 輸入游標 |
| W8 | `PvpMatch.cpp:659` `Reset` | 整欄清空 | host（世代切換） | 成員關係（機制下＝重建 World，A.1） |

不寫 `bodies_`、但寫 `PlayerState` 型別欄位的（entity-model 第 139 行的「host 診斷」組）：`movementSlackSequence`／`movementSlackMicros`（`MatchRuntimeHost.cpp:683-687`）與 `connectionQualityFailures`（`:742`）只寫在 `match_.Snapshot()` 的**複本**上；`bodies_` 裡永遠是預設值。`Snapshot` 也在複本上重算 `contiguousPendingCommands`（`PvpMatch.cpp:701-703`）。`connectionQualityFailures` 在 digest 的 `player` 列裡（`AuthorityDigest.cpp:163`），所以它是 host judgement 的 (c) 資料，只是今天不在權威欄裡。

### 5.2 拆法建議（依 entity-model 第 139 行的四組）

| 新欄（說明名） | 欄位 | 照 H.4.1 拆 System 後的寫者 | 仍是兩個寫者的地方與建議 |
|---|---|---|---|
| Body（身體） | position、yaw、pitch、verticalVelocity、grounded、crouched、sprinting | 移動 System（W6）；**復活**也寫（W4 的 `:331-337`） | 兩個寫者 → D-E2。建議：復活不直接寫 Body，改發「在 spawn 點重生」事件或寫中間欄，移動 System（唯一寫者）在本 tick 移動前套用（B.3.1 漏斗）。順序今天是「復活 → 移動」，套用點放在移動 System 開頭即同序；但移動 System 在復活那個 tick 會被 `lastMovementResetTick == tick_` 跳過（`:529`），套用要在跳過判斷之前 |
| Life（生命） | lifeGeneration、lifeState、lifeStateTick、respawnTick | 死亡（W3，在 `ResolveActions` 內）與復活（W4） | 兩個寫者 → D-E2，而且**同一個 tick 內有讀後寫**：`Kill` 之後同 tick 的其他 `Decide` 讀 `lifeState`（`:373` 判 Dead）與 `targetLifeGeneration`（`:426`）。若死亡改走事件、由 ApplyLife 在下一個同步點套用，`ResolveActions` 得自己保留本 tick 已判死的集合，否則判定改變；ApplyLife 放在動作裁決之後同一 tick，snapshot 才與今天相同。**本報告無結論**：漏斗的位置決定 golden 會不會變，要在遷移批的事前宣告定（兩種判法都試過：「事件＋下一 tick 套用」會讓死亡晚一 tick 出現在 snapshot；「同 tick 套用」要求 ApplyLife 同時處理復活與死亡兩個 Plan 位置，一個 System 只能有一個位置） |
| InputCursor（輸入游標） | lastResolvedCommand、movementEpoch、contiguousPendingCommands | 移動 System（W6、W7）；ResetMovementEpoch（W5，由復活、排定重設、序號用盡觸發） | 排定重設與序號用盡已在移動迴圈內（同一 System）；復活觸發的那次跟 Body 一樣改走漏斗。`StepMovement` 自己寫 `lastResolvedCommand`（`Movement.cpp:104`）且 Client 預測共用（`LocalPlayerPrediction.cpp:79` 整份複製）：拆出游標會改 `StepMovement` 的簽名或回傳，Client 同步改（entity-model §3.2 注記） |
| Connection 診斷（host 診斷） | movementSlack*、connectionQualityFailures | Judge System（今天 `TrackSlack`、`JudgeConnectionQuality`） | 今天不在 `bodies_`，只寫複本；拆後是 Connection 實體上的 (c) 欄（`connectionQualityFailures` 進 digest）＋(b) 欄（slack 只給 Client 看），單一寫者，沒有衝突 |

成員關係（W1、W2、W8）在機制下不是寫者：生成與刪除由 `ApplyMembership` 以 `SpawnWithId`／`Despawn` 做（LIM-5），Reset 是重建 World（A.1）。digest 的 `player` 列欄序要由編碼器從四欄重組（entity-model §5.4「不接受拆欄＝golden 重生」）。

### 5.3 Join／Leave 進 tick 內（LIM-5）會動到的 golden

情境名取自 `tests/object_fps_pvp/fixtures/authority_golden.txt`（37 行）與 `AuthorityDigest.cpp` 的情境表。依據是每個情境的成員變化發生在哪裡：

| 類別 | 情境 | 今天的成員變化 | LIM-5 之後 | 判斷 |
|---|---|---|---|---|
| 確定受影響 | `combat-rejoin/corridor` | t=60 時 `Leave(2)`＋`Join(2)` 在兩個 Tick 之間（`AuthorityDigest.cpp:318-322`、`:572`） | 請求進 ring，下一 tick 的 `ApplyMembership` 處理；`Join` 的動畫重設 tick（`PvpMatch.cpp:69` 以 `tick_` 計）差一 | 要重設基準（spec 已點名） |
| 確定受影響 | `host-play/corridor`、`host-poor-connection/corridor` | `QueueJoin` 在 `Step` 的每 tick 回呼裡、`match_.Tick` 之前處理（`MatchRuntimeHost.cpp:391`），當時 `tick_` 還是上一 tick；`quality_` 的視窗起點取 `match_.TickCount()`（`:392`） | `ApplyMembership` 在 domain tick 推進之後跑 → 動畫重設 tick 與品質視窗起點差一 | 要重設基準 |
| 確定受影響 | `host-poor-connection/corridor`（另一條） | 驅逐：`JudgeConnectionQuality` 在 `Tick` 之後同一回呼內直接 `match_.Leave`（`:757`），當 tick 的 snapshot 已無該玩家（`:760`） | 驅逐成為請求，下一 tick 才刪 → 被驅逐者多留一 tick | 要重設基準 |
| 有條件 | 其餘 34 個 golden（`move-*` 22、`combat-*` 除 rejoin 外 5、`prediction-*` 7） | 只在第一個 Tick 之前 `Join`（`Duel` 建構子 `:300-305`、`RunLink` `:585`） | 若測試 host 把初始成員放在 **Startup schedule**（B.3：Startup 的寫者不計；G-4：Startup 初始內容是 Host 寫入的 (c) 輸入）→ 不變。若初始成員也走 Ingress→`ApplyMembership` → 全部改變（玩家在 tick 1 才出現，`Duel::Move` 第一次找不到玩家、動畫重設 tick 不同） | 交遷移批決定初始成員的路徑；建議 Startup |
| 非 golden（資訊用） | `move-turning/*`、`combat-kill-respawn-aimed/*`、`combat-graze/corridor` | 只有初始 Join | 同「有條件」 | — |

Body 拆欄本身（§5.2）在編碼器重組欄序的前提下不改 golden；Life 的漏斗位置若讓死亡晚一 tick，`combat-kill-respawn`、`combat-occluded`、`combat-magazine`、`combat-cross-life`、`combat-rejoin`、`host-*` 會變（推論，未實測）。

## 6. (b) Host 保留職責的現況位置

| spec H.7 的職責 | 今天在哪裡（本範圍） |
|---|---|
| 建 World、選 plugin、ScheduleDef、Executor 策略 | 無對應（今天是 `PvpMatch` 建構子與 `MatchRuntimeHost` 建構子把元件寫死：`match_main.cpp:150`） |
| 多時鐘協調：固定步累加、追趕上限、相位、絕對 deadline | Match：`MatchRuntimeHost::Body` `:513-535`（deadline `:533`）＋`ticker_.Advance` `:385`；Client：`ClientSimulationRole::Body` `:184-208`（`WaitUntil(step.nextDeadline)` `:203`）、`LocalPlayerPrediction::TrackPhase` `:176`、`ticks_.Advance` `:410` |
| Runtime 的建立、啟停、停止順序；世代切換（Reset） | `match_main.cpp:152-153`（Start）、`:194`（`ipc.Stop(); runtime.Stop();`，IO 先停）；Reset：`IpcHost.cpp:487`、`:552` `host.RequestReset()` → `MatchRuntimeHost.cpp:373-378`（`ClearState`、`pendingReset_`）→ `PvpMatch::Reset` `PvpMatch.cpp:658-668` |
| 權威快照的解碼與 `Restore`／`Replay`（Client） | `ClientSimulation.cpp:57` → `LocalPlayerPrediction::Reconcile` `:267` |
| 執行緒的建立與角色綁定 | `RoleThread::Start`：`MatchRuntimeHost.cpp:64`、`IpcHost.cpp:891`、`ClientConnection.cpp:205`、`ClientSimulationRole.cpp:151`；**例外**：`MovementDiag.hpp:109` 用 `std::thread`，不經 `RoleThread`（不受 threads 的 DI 4「owner orders roles」約束） |
| IO 與 World 的交換點 | `MatchRuntimeHost::mutex_`（`.hpp:248`，整個 `Step` 持有 `:371`）；`ClientSimulationRole::mutex_`（`.hpp:213`；`:161-211`）；`ClientConnection` 的 `mutex`（`:130`） |
| 平台 pump 與 live frame | `PvpApplication.cpp:1111-1119`（`BeginFrame`→`PumpEvents`→`EndFrame`）、`:1389-1401`（live frame handler）、`RuntimeLoop.hpp:37` `RunLiveFrame` |
| 回滾的觸發 | `LocalPlayerPrediction::Reconcile` `:267` |
| Runtime Error 與 `Interrupted` 的處置 | `match_main.cpp:169`（`runtime.Error()`）；`ClientSimulationRole::Error` `:176` |
| 診斷視窗的彙整 | `match_main.cpp:158-191` |

**H-7 掃描清單**（`h7_include_scan.txt`；全 repo）：

- include `IRuntimeClient.hpp` 或 `RuntimeLoop.hpp`：`apps/object_fps_pvp/include/RetroFPS/Pvp/PvpApplication.hpp:5`、`apps/object_fps_pvp/src/Pvp/PvpApplication.cpp:34`、`framework/src/WindowShell.cpp:9-10`、`engine/runtime/{include/…/RuntimeLoop.hpp:6, src/RuntimeLoop.cpp:1}`、`tests/common/core/runtime/RuntimeLoopTests.cpp:9`、`tests/common/runtime_sdl/main.cpp:7`；`apps/object_fps`、`apps/object_fps_v2` 各兩處（隨移除消失）。
- 經 `WindowShellClient`／`ImGuiShellClient` 間接使用 RuntimeLoop：`tools/audio_editor/src/AudioEditorWindow.cpp`、`tools/model_viewer/src/ViewerWindow.cpp`、`tools/ui_editor/src/EditorWindow.cpp`、`tools/voxel_map_editor/src/window/EditorWindow.cpp`（framework 的 `WindowShell` 重寫後，這四個都要跟著遷移）。
- 不 include RuntimeLoop、但手排模組更新順序的 host（H-7 的「依序呼叫模組的 Update／Tick」，文字掃描抓不到 include 的那一類）：`MatchRuntimeHost.cpp:385`、`:441`；`ClientSimulationRole.cpp:196`；`LocalPlayerPrediction.cpp:410`。H-7 的守衛只掃 include 會漏掉這一類（觀察 4）。

## 7. (c) spec G.4 三條違反的逐條確認

1. **`ShotReferenceAge` 回呼**：**確認存在**。`Combat.hpp:228` 定義；Host 在 `Step` 內以 `now_() - reference->publishedAt` 計算（`MatchRuntimeHost.cpp:441-446`，spec 寫 `:441-445`）；`PvpMatch::ResolveActions` 在「第一個看到該動作的 tick A」呼叫一次並保存（`PvpMatch.cpp:451-456`，註解引 pv7 §6 D-3 (a)），`Decide` 用它判 `InvalidReference`／`Expired`（`:376-379`）。**與 spec 的差異**：今天量的是「tick A 處理時的時刻」，不是到達時刻。spec G.2／H.4.1 (a) 改成「到達時刻」並說粒度不變——對這一條不成立：在 digest 的 host 情境裡，`SubmitActionBatch` 在 `clock` 推進前、`Advance` 在推進後（`AuthorityDigest.cpp:698-705`），兩者差一個 tick（16.7 ms）。上限 250 ms（`Combat.hpp:26`），host 情境的年齡約 1～2 tick，判定翻轉**預期不會發生，但未實測**。若要保住 pv7 D-3「在 A 量一次」的語義，應從 `HostClockSample`（Host 每次喚醒取樣）算，而不是到達時刻（觀察 5）。
2. **`SubmitInput` 的 reference age**：**確認存在**。IPC 執行緒（`IpcHost.cpp:645`）同步呼叫 `MatchRuntimeHost::SubmitInput`；`now_()` 在 `:218`（`at()`），年齡 lambda 在 `:222-230`，存進 `quality_` 在 `:262-265`；`JudgeConnectionQuality` `:700-760` 取中位數判 late 與驅逐，`connectionQualityFailures` 進 digest（`AuthorityDigest.cpp:163`）。今天取的就是到達當下（鎖等待之後，`:161-164`），所以改成「IO 執行緒收到時取樣」粒度相同；在腳本時鐘下兩者取同一值（`SubmitInput` 在 `clock` 推進前呼叫），`host-poor-connection` 的 golden **預期不變，未實測**。
3. **`MovementDiag`／`MovementTrace` 全域**：**確認存在，位置更正兩處**。(i) `StepMovement` 本身不碰全域（`Movement.cpp` 無命中；`diagnostics` 是輸出參數）；碰全域的是呼叫點：`PvpMatch.cpp:514`、`:569-580`、`:586`，`LocalPlayerPrediction.cpp:21`、`:31` 等。(ii) `MovementTrace.hpp:100` 是註解；全域 mutex 在 `:101`、sink 在 `:102`。`MovementDiag.hpp:188`（atomic 指標）與 `MovementTrace.hpp:59`（`steady_clock`）正確。另：spec 的改法「改成宣告的 `Diagnostics` 副作用」與 S-8／Z4「zone=C 的 System 不得宣告任何 effect」衝突（觀察 3）。

**追加（spec 未列的同類違反）**：

4. **IO 執行緒直接寫權威的移動重設**：`SubmitInput`（IPC 執行緒）以牆鐘算遲到量 `late = Micros(at() - arrival.substitutedAt)`（`MatchRuntimeHost.cpp:249`），超過 `MovementResyncLateMicros` 就呼叫 `match_.RequestMovementResync`（`:253`），它直接寫 `connections_` 的 `movementResetScheduled`／`movementResetReason`（`PvpMatch.cpp:255-256`）；下一個 `Tick` 依此轉 epoch（`:532-534`），`movementEpoch` 進 digest。這是「IO 執行緒＋牆鐘 → (c) 狀態」，形狀與第 2 條相同但寫的是權威欄，不是 host 的 `quality_`。`substitutedAt` 本身也是 `now_()` 的讀值（`TrackSlack` `:669`）。機制下要變成 Judge 或 Simulate 的 (c) System 從 ring 的到達時刻與記錄的代換時刻計算。
5. **IPC 執行緒讀權威快照**：`SubmitInput` 在 IO 執行緒呼叫 `match_.Snapshot()`（`:178`）取游標；`SubmitActionBatch` 以 `match_.CanSubmitActions` 等同步驗證並產生 refusal（`:309-324`）。只讀，但在機制下 IO 執行緒碰不到 World（I-1、W-6），驗證要搬進 Ingress，回覆時機改變（spec H.4.1 (b) 已點名 `:685`；`:645` 的 `SubmitInput` 同理）。

## 8. (d) 第三方重核表（對 #176 判定總表）

| # | 子系統 | #176 判定 | 重核 | 一句理由 |
|---|---|---|---|---|
| 1 | ECS | 前提變更、待重判；Scheduler 自寫已定 | **維持**（待重判） | World 儲存層借 EnTT 與否不在本批（事前宣告）；EnTT 的事實列照抄 |
| 2 | Physics | 並存；Jolt 兩條路徑 | **維持**，補註 | 機制下 Jolt 只能是 (b) World 的 Resource（`PhysicsSystem` 是第二份狀態，B.3 的單一寫者與 Z2 擋住它進 (c)）；權威路徑等於讓 Jolt 成為位置欄的唯一寫者，正是 D1 |
| 3 | 2D Physics | 不需要 | **維持** | 無 Feature |
| 4 | Navigation | 第一候選 | **維持**，補註 | §2.1 的「可當能力」收窄為「包」：query 物件跨 tick 持有、alloc hook 全域；進 (c) 仍等 DET-1／D2 |
| 5 | UI | 自製維持；ImGui 只在工具 | **維持** | ImGui＝包（全域 context，effect `Gpu:imgui`），與 spec L.2 一致 |
| 6 | Model Import | 維持 ufbx | **維持**，補註 | ufbx 是葉子，但 zone＝C（內容身分進跨平台比對）且在 contraction 旗標前建置——#177 方案 A 的對象 |
| 7 | Mesh Optimization | 不需要 | **維持** | 無 Feature |
| 8 | Audio | 自製維持；miniaudio 候選 | **維持** | miniaudio engine 層自帶執行緒，與「音訊回呼是外部服務、Submitter 是 Resource」（spec I.1）不相容；device 層＝服務 |
| 9 | Shader Compiler | 維持 | **維持** | host 工具，不在 Runtime |
| 10 | GPU Abstraction | 維持 SDL_GPU | **維持**，補註 | 「present 只在視窗執行緒」在機制下就是 H-3 的 `GraphicsContextOwner` 親和；Render World 獨立（K.3 第 6 項）與它相容 |
| 11 | Animation | 自製維持 | **維持** | 不涉及 Kernel 相容性 |
| 12 | VFX | 不需要（現在） | **維持** | 若採 Effekseer：關 worker、Manager 為渲染 World 的 Resource（包） |
| 13 | Image Processing | 維持 SDL_image | **維持** | 葉子 |
| 14 | Compression | 不需要 | **維持** | 純函式 |

**差異清單**：判定欄**沒有改判**。§2.1「Kernel／plugin 相容性」欄的收窄四處：Recast／Detour「可當能力」→ 包†（全域 hook 明列例外）；Jolt「可當能力」→ 包†（只限 (b) World）；ImGui「可當能力」→ 包†（全域 context 明列例外）；SDL_GPU「Stage 約束」→ H-3 親和。另外 #176 的 14 列不含現有依賴 asio、httplib、protobuf、nlohmann、doctest（#176 的範圍是 14 個子系統）；回寫判定總表時，這五個的 Kernel 相容性在本報告表 1。

## 9. 本範圍的觀察

1. **事前宣告的「Q1 為是 → 組織層」與 spec I.3 不一致（已由架構師對齊，§0）**。照宣告字面，`PlayerPresentation`、`PvpSound`、`WeaponViewModel` 這類只有建立與釋放的模組會被判成組織層。對齊後組織層的依據是「排序其他元件」，本範圍的組織層列（P1、P2、P5、P6、P11、S1）都能指出寫死的呼叫序。另：第三方「服務＝自己擁有執行緒或裝置回呼」的定義下，asio、httplib、protobuf 字面是包，但它們只活在 IO 服務內，最終落點仍是服務；建議事前宣告或合併稿寫明「依附在服務內的函式庫隨服務判」，否則三份的第三方列會不一致。宣告的修正是事前宣告的變更，交主會話與使用者。
2. **掃描根 `apps/object_fps_pvp/src` 漏掉 6,867 行標頭與兩個 `main`**。`MovementDiag.hpp`（自起 `std::thread`、全域 atomic 指標）、`MovementTrace.hpp`（全域 sink、時鐘）、`ClogLine.hpp`（全域 mutex）、`Combat.hpp`（`ShotReferenceAge`）都是標頭，主掃描看不到；補充掃描（`supp:*`）才抓到。Q2 的 regex 只認 `_` 結尾的成員名，`WeaponViewModel` 的 `device`／`assets` 指標沒命中（人工發現）。掃描的「無命中」在本範圍只能當線索，不能當否定（LIM-3）。
3. **G.4 第 3 條的改法與 S-8／Z4 衝突（規格前提的問題）**。spec 要 Movement 的呼叫點「改成宣告的 `Diagnostics` 副作用」，但 S-8 規定 zone=C 的 System 不得宣告任何 effect（hook 有 `Diagnostics` 例外，F.4；System 沒有）；Z1 又禁止 (c) System 寫 zone=B 的事件或欄。照現行規格，(c) 的移動 System 完全不能輸出診斷，而 `TraceMovement` 的 `Resolved`、`Reset` 是診斷資料契約（entity-model §5.1 提到它的輸出順序）。要嘛 S-8 比照 hook 開 `Diagnostics` 例外（只寫不讀回，I-5 同理），要嘛定一條 (c) → (b) 的單向診斷通道。另外 `Combat.hpp:5`、`Movement.hpp:8` 為了 `std::chrono::nanoseconds` 型別 include `<chrono>`，J-17「(c) 檔不得 include `<chrono>`」會誤判；需要區分「型別」與「讀時鐘」，或改成整數 ns。
4. **H-7 的守衛只掃 include 不夠**。`MatchRuntimeHost` 與 `ClientSimulationRole` 不 include `IRuntimeClient.hpp`，卻手排 `Tick`／`Run` 的順序（§6）；「一個 host 不會一半在機制上」的文字掃描要加上這些呼叫點，或改成「host 原始碼不得呼叫產品模組的 `Tick`／`Advance`」。
5. **`ShotReferenceAge` 的量測點**（§7 第 1 條）：spec 假設兩種 reference age 都從到達時刻算、粒度不變；實際上連線品質那一條是到達時刻，射擊那一條是 tick A 的時刻（pv7 D-3）。建議射擊改用 `HostClockSample`，連線品質用 ring 的到達時刻；兩者都要在遷移批以 digest 比對確認。
6. **spec 未列的 IO 執行緒寫權威**：`RequestMovementResync`（§7 第 4 條）。與 spec L.3 的 `MatchRuntimeHost` 初判不衝突（同樣是重寫），但 Ingress／Judge 的設計要把它算進去，否則遷移後仍有一條牆鐘進 (c) 的路。
7. **Body 拆欄後 Life 仍有兩個寫者，且同 tick 讀後寫**（§5.2）：spec B.3 預期「拆欄就解決 D-E2」，對 Body 與輸入游標靠漏斗可解，對 Life 漏斗的位置會改變判定或 golden；本報告無結論，交遷移批的事前宣告（K.2 第 10 條的計數從遷移後開始，遷移本身的拆欄不算）。
8. **與 spec §L.3 初判不同之處**：(i) `StepMovement` 判「留」同 L.3，但全域在呼叫點（P1、P6），不在 `StepMovement` 的呼叫路徑之內——L.3 與 G.4 的寫法容易讓遷移批去改 `Movement.cpp`；(ii) `IpcHost` 同步碰權威的點不只 `:685`，還有 `:645`；(iii) `src/App`（L.3 沒列）判包†；(iv) 診斷寫者 `MovementDiag` 自起 `std::thread` 而非 `RoleThread`，L.3 歸在「服務」沒錯，但它不受 threads DI 4 的角色約束，H.7「執行緒的建立與角色綁定」有一個例外。其餘子列與 L.3 一致。

## 估時

估 3 小時（上限 4.5 小時）；實際約 2.9 小時（讀規格與文件約 0.7、掃描與人工確認約 1.1、四節與撰寫約 0.7、判法對齊後的逐列核對與 §4.1 人工補看約 0.4）。
