# 審查報告：`v8-arch-kernel-audit`（現有模組對規格 v0.2 的審查）

更新：2026-11-11。Owner：架構（架構師會話）。**狀態：完成（五條判定成立；對抗複核 4/5 一致，不一致的一列已重判）；待使用者決定 §3、§4.5 的事項。** 事前宣告：[audit-declaration.zh-Hant.md](audit-declaration.zh-Hant.md)。三份部分報告（逐列證據在這三份，本稿不重複檔名行號）：[engine](audit-part-engine.zh-Hant.md)、[framework-tools](audit-part-framework-tools.zh-Hant.md)、[product-thirdparty](audit-part-product-thirdparty.zh-Hant.md)。主會話逐列讀過三份後合併；判法在三份間對齊（§0）。只讀：未改程式、未改契約。

**件数の突き合わせ：一致**。共用腳本對全範圍一次掃描（`build/evidence/kernel-audit/scan-full/`）＝44 模組、645 命中；三份分別 22＋8＋14＝44 模組、279＋180＋186＝645 命中。報告列數：engine 22＋framework-tools 7＋product-thirdparty 14＝43 列，加掃描根內的未審 1 列（`tools/object_fps_preview`）＝44。掃描根之外另有未審 3 列（`apps/object_fps`、`apps/object_fps_v2`、`services/gyo_gateway`）與事實列 5（#176 候選與 host 工具鏈），不計入掃描件數。

## 0. 判法（合併時對齊；修正了事前宣告的一句）

事前宣告原寫「Q1 為是 → 組織層」。第一份報告（framework-tools）指出它比 spec I.3 粗：I.3 只把 Q1（生命週期、更新迴圈）當「留」的排除條件，包還是重寫由 Q2～Q4 定。主會話在三份完成前對齊為：

- Q1 為是 → 只排除「留」；Q2～Q4 皆否 → **包**（狀態變 Resource、每幀入口變 System、生命週期交 Host 或 plugin）；任一為是 → **偏重寫**那一層介面或先拆狀態再包（三份記為「包（先拆／先收攏）」或「包†」）。
- **組織層**（Host／重寫骨架）＝模組自己排序其他元件（寫死的呼叫序、擁有迴圈或執行緒並決定誰先誰後），對應 spec H-7；單純每幀被呼叫一次不算。
- 只持 OS 資源、只由 Host 使用的 adaptor（os、time、threads、platform、FixedTickRuntime）判 **Host**（依 spec §L 的定義與 G-6，不是「排序其他元件」）。
- **留**＝純函式且三否定皆滿足。第三方：葉子／包／服務；**依附在服務內的函式庫隨服務判**（asio、httplib、protobuf 字面是包，落點是 IO 服務的內部實作；product-thirdparty 觀察 1 的建議，本稿採納並寫進宣告）。

對抗複核者（§8）指出判法五處模糊，主會話定義如下（合併時適用，之後的批照用）：
- **Q2 的範圍**：含建構子注入的協作者參考（它們將成為 Resource）與指向 World 內資料的快取指標兩類；前者的處置是「先收攏／先拆」，不改判定詞；後者才偏重寫。
- **「包（先拆）」與「重寫」的分界**：拆掉的狀態能改成輸入或參數、內部邏輯不動 → 包（先拆）；第三方全域 context 是資料主路徑、或元件混了 H.7 的 Host 職責（固定步累加、回滾觸發、幀排序）→ 重寫。
- **第三方全域的粒度**：只寫出不讀回（`SDL_GetError` 取錯誤字串）比照 I-5 例外，不算 Q3 為是；讀回參與判斷（ImGui context、SDL 全域查詢）才算。
- **一個目錄兩種性質**：拆成元件列各給一個判定詞；一致率以元件計。
- **純函式庫的 zone**：填「被哪個區呼叫」，不影響判定詞。

三份都已依此逐列重核（engine 重核改一列：input Host → 包（先拆）；product-thirdparty 改六列，見其 §0.1；framework-tools 兩種讀法並列）。這是判法的對齊，不是看結果後放寬判定。

## 1. 逐模組判定（最終；證據見各部分報告）

### 1.1 engine（22 列；行數 30,490）

| 模組 | 判定 | 一句 | 工時 |
|---|---|---|---|
| base | 留（附例外） | log／assert 的全域只寫不讀回，spec I-5 例外；`Sha256`、`StrictJson` 在 (c) 邊界內 | 0～2 h |
| collision | 留 | 純查詢；`StaticAabbGrid` 跨 tick 時放不可變 Resource | 0～2 h |
| entity_store | 留＋成為 World 欄的實作 | W-1、W-2 的演算法；Delta 計入 World 批 | 0 h |
| map_markers、voxel_mesh、net、math | 留 | 純函式；math 的 E1 是 determinism 線 | 0 h |
| model | 留（取樣、loader）＋包或刪（`AnimationInstance`） | ufbx loader 是 (c) 內容身分；`AnimationInstance` 只有 object_fps_v2 用 | 1～4 h |
| asset | 包（先收攏） | 五個協作者的參考要先收進同一個 Resource；`Update` 內同步 IO 要宣告 `Io:file` | 8～20 h |
| input | **Host**（`SdlInput` 後端，pump 的零件）＋留（`InputActionMap`、`PhysicalInputFrame`） | 後端讀 SDL 全域、持 `platform_`、只由 Host 的 pump 呼叫，產物是 boundary 資料 `PhysicalInputFrame`；主會話依對抗複核重判（§8）：審查者最終的「包（先拆）」是把狀態機拆出後的設計選項，不是現況的判定 | 4～10 h |
| io | 包（檔案服務）＋留（路徑、Uri、記憶體串流） | `WriteFileAtomically` 讀時鐘與亂數，(c) 檔不得 include | 2～6 h |
| render | 包（先收攏；渲染 World 的 Resource，親和 `GraphicsContextOwner`） | 裝置、`ShaderLibrary`、`ModelResource` 的指標收進同一 Resource | 8～24 h |
| text | 包（後端）＋留（介面） | SDL_ttf 全域 context 拆不掉，審查項 | 1～3 h |
| ui | 包（`UiRuntime`）＋局部重寫（`UiRenderer`）＋留（文件、版面） | `UiRenderer` 跨幀存 `AssetManager*` 與裝置指標，Frame／Render 分開後是跨 World 指標 | 8～18 h |
| os、time、threads | Host | adaptor；(c) 檔不得 include time | 0～4 h |
| platform | Host | pump、live frame、dialog 登記表與 `thread_local` 無法成 Resource | 4～10 h |
| audio、trace | 服務 | 自有執行緒；`Submitter` 是帶親和的 Resource | 2～6 h 各 |
| runtime | 重寫（`RuntimeLoop`／`IRuntimeClient`）＋Host（`FixedTickRuntime`）＋刪除候選（`IRuntimePort`） | 三段硬編碼調度由 Host＋ScheduleDef 取代 | 8～20 h |
| config | 不適用 | `projects.csv` 列有要移除的兩個產品，移除批一併刪 | 0 h |

分佈（重判後）：留 8、包 5、Host 5、服務 2、重寫 1、不適用 1。工時 52～150 h。

### 1.2 framework 與工具（7 列＋未審 1；行數 19,806）

| 模組 | 判定 | 一句 | 工時 |
|---|---|---|---|
| framework `WindowShell` | Host（重寫成 Editor host 骨架） | 擁有 `RuntimeLoop` 與 pump；今天只有四個工具用，PvP 不用 | 16～32 h |
| framework `ImGuiShell` | Host（骨架重寫）；ImGui context 以 effect `Gpu:imgui` 承載 | `Update`／`Render` 是寫死的幀排序，不只是「包」 | 12～24 h |
| framework `HeadlessShell` | Host（原樣） | 行程入口 | 0 h |
| framework `OffscreenPreview`、`ReadOnlyAssetCatalog` | 包 | 前者渲染 Runtime 的 Resource；後者值狀態＋`Io:file` | 5～10 h |
| framework 其餘（`EventSounds`、`CatalogPlan`、`PreviewCamera`、`PreviewTexture`、`UiTextInput`、`AbsolutePath`、json binding） | 留 | 純函式 | 0 h |
| voxel_map_editor | client Host（重寫）；`EditSession`、`MapPreview`、`PreviewScene` 包；葉子留 | 工具線給的起點成立；預覽 readback 回流見 §3.2 | 24～48 h |
| audio_editor | client Host；`PreviewPlayer` 包；核心留 | — | 8～16 h |
| model_viewer | client Host；預覽與音訊包；文件留 | `pendingAction_` 跨幀捕獲 `this`，改成資料 | 16～32 h |
| ui_editor | client Host；`EditorApp` 重寫（介面）；其餘包或留 | `EditorApp` 存下兄弟物件的參考 | 32～56 h |
| object_fps_preview | 未審 | K.3 第 8 項移除 | — |

分佈（細分）：framework Host 3、包 2、留 8；工具 Host 4（client）、重寫 1、包 15、留 9 組、量測 harness 2 不適用。工時 framework 33～66 h、工具 80～152 h。

### 1.3 產品與第三方（14 列；行數 10,225，`src/Pvp` 9,152 另分 13 子列）

| 模組 | 判定 | 一句 | 工時 |
|---|---|---|---|
| `src/Collision`、`src/Gameplay` | 留 | (c) 純函式；Gameplay 的 libm 三角函數是 E1 | 0～3 h |
| `src/App` | 包（先拆） | `WeaponViewModel` 跨幀存裝置與 `AssetManager` 指標 | 6～12 h |
| P1 `PvpMatch` | 重寫為 (c) plugin | `Tick` 寫死四段順序；Body 三段寫入要拆欄（§4.1） | 40～80 h |
| P2 `MatchRuntimeHost` | 重寫：Host＋Ingress／Judge／Egress | 另發現 IO 執行緒直接寫權威的第二處（§3.4） | 40～70 h |
| P3 `IpcHost` | 服務＋重寫（同步碰權威的兩處改經 Link） | `:645`、`:685` | 20～40 h |
| P4 `ClientConnection` | 重寫：服務＋Link；狀態→Resource | — | 30～50 h |
| P5 `ClientSimulationRole`／`ClientSimulation` | 重寫：ClientSim Runtime＋Host 時鐘策略 | 「何時跑下一步」判 Host | 20～40 h |
| P6 `LocalPlayerPrediction` | 重寫（回滾機制的使用者） | 自有固定步迴圈與重放順序 | 30～60 h |
| P7 `StepMovement` | 留 | 全域在呼叫點，不在它裡面 | 0～4 h |
| P8 `MovementDiag`／`MovementTrace` | 服務（Diagnostics）；全域入口改句柄 | 自起 `std::thread`，不經 `RoleThread` | 8～16 h |
| P9 射擊、姿勢、動畫 | 留（System 本體） | `ShotReferenceAge` 的改法在 P1／P2 | 2～8 h |
| P10 呈現 | 包（先拆：時鐘改由 `FrameTime` 傳入） | — | 10～20 h |
| P11 `PvpApplication` | 重寫：Client host＋ClientFrame／ClientRender System | — | 40～80 h |
| P12 診斷 | 服務 | — | 4～10 h |
| P13 內容 | 留（`NativeFiles` 包） | (c) 內容身分 | 2～6 h |
| S1 `main`／`match_main` | Host | 行程入口與停止順序 | 10～20 h |
| third_party：nlohmann、sdl3_image、ufbx | 葉子 | ufbx 是 (c) 內容身分且在 contraction 旗標前建置（#177 方案 A 的對象） | 0 |
| third_party：imgui、sdl3_ttf | 包（全域 context 明列例外） | — | 0 |
| third_party：sdl3 | 服務（音訊回呼）／Host（pump）／包（GPU 裝置） | 一個庫三種落點，在 engine 列落實 | 0 |
| third_party：asio、httplib、protobuf | 服務的內部實作（隨 P3、P4） | protobuf 的 generated pool 若改由 Egress System 直接編碼，要明列例外（待遷移批） | 0 |
| third_party：doctest | 不適用 | 測試 | 0 |
| 事實列：Recast／Detour、Jolt | 包（先拆；Jolt 只限 (b) World） | #176 §2.1 的「可當能力」收窄 | — |
| 事實列：miniaudio | device 層服務；engine 層不相容 | — | — |
| 事實列：EnTT | 不判（World 儲存層不在本批） | 待 World 實作批以 W-1～W-11 核對 | — |

分佈：子列重寫 6、服務 2、服務＋重寫 1、包 1、留 3、Host 1；第三方葉子 3、包 2、服務 4、不適用 1。工時 250～480 h。

### 1.4 合計

| 範圍 | 重寫（組織層） | 包 | 留 | Host／服務 | 工時粗估 |
|---|---|---|---|---|---|
| engine | 1 | 5 | 8 | 7 | 52～150 h |
| framework | 2 骨架 | 2 | 8 | 1 | 33～66 h |
| 工具 | 4 client＋1 | 15 | 9 組 | 2 harness | 80～152 h |
| 產品 | 6 子列＋1 | 2 | 3 | 4 | 250～480 h |
| **合計** | | | | | **約 415～850 h**（區間，不是承諾；不含 Kernel 本體） |

與 mechanism §1、spec L.5 的預測一致：重寫集中在組織層（PvpMatch、MatchRuntimeHost、PvpApplication、Client 的模擬角色、兩個連線元件的同步介面、framework 的兩個 shell、四個工具的 client）；Engine 本身大多是留或包。

## 2. 與 spec §L 初判不同之處（合併）

| 模組 | spec §L | 審查 | 原因 |
|---|---|---|---|
| input 後端 | 包 | Host | 讀 SDL 全域、持 `platform_`、只由 Host 的 pump 呼叫；審查者與對抗複核者不一致，主會話重判為 Host（§8） |
| platform | 包（Host 擁有） | Host | 永不銷毀的登記表與 `thread_local` 無法成 Resource |
| `FixedTickRuntime` | 留 | Host | 決定這一幀跑幾次 FixedUpdate（H.7、G-6） |
| `IRuntimePort` | -audit 定 | 刪除候選 | 移除 object_fps／v2 後只剩自己的測試；形狀與 Link 相反 |
| ui | 包 | 包＋`UiRenderer` 局部重寫 | 跨 World 指標 |
| asset | 包 | 包（先收攏） | 五個協作者由組合根各自持有 |
| model | 動畫推進包 | `AnimationInstance` 刪除候選；ufbx loader 是 (c) | — |
| `WindowShell`、`ImGuiShell` | 重寫骨架／包 | framework 退役：兩者的職責由 runner plugin 與 ImGui／SDL plugin 承擔；PvP 不用 WindowShell | §3.1（使用者決定） |
| `src/App`、`NativeFiles` | 未列／留 | 包（先拆）／包 | — |
| `IpcHost` 同步碰權威 | `:685` | `:645`、`:685` | — |
| `StepMovement` 的全域 | 在呼叫路徑 | 在呼叫點（P1、P6），`Movement.cpp` 乾淨 | 遷移批不要改 `Movement.cpp` |

## 3. 跨部件的發現（要決定或要改規格）

### 3.1 Architecture Delta：framework 退役，由 plugins 層取代（使用者 2026-11-11 決定，側邊會話轉述）

審查原提案「改 framework DI 2／6、WindowShell／ImGuiShell 重寫成 Editor host 骨架」。使用者決定**不改 DI，framework 這個概念退役**：framework 是舊架構的產物，核心是組合 Engine 能力與對接第三方；plugin 機制建立後兩者都由 plugin 承擔。使用者原則：新架構若沿用舊架構的思路與原則，架構變更就失去意義——思想不變，修改就是在破船上打補丁。

去處：
- ImGui／SDL 的相容在 plugin 內實現。
- Editor host 骨架（行程入口、pump、交換點）＝一個**提供 runner 的 plugin**（照 Bevy `WinitPlugin` 的 `set_runner`）；規格 C.2／H.7 補「plugin 可在 finish 安裝 runner，一個 Runtime 只能一個」（R-E6）。
- 標準 Editor stage 序＝plugin 群在 build 插入 stage（spec C.2 第 2 點已允許）。
- 組合格式（catalog plan、event sounds）是資料契約，owner 是提供該 plugin 的人。
- 葉子（EventSounds、CatalogPlan、PreviewCamera、UiTextInput、json binding）是函式庫，與層無關。

後果（Delta，使用者已同意）：新 top-level `plugins/`（AGENTS §10）——每個 plugin 一個 target、自己的依賴正向清單，Engine 永不 include 它，產品與工具只在 main 列它；三層原則改為「Engine（Kernel＋能力）／plugins（組合與第三方對接，可選可刪）／產品與工具（plugin 清單＋Run）」；framework DI 5「兩個以上產品以同一方式組合者先進 framework」→「先成為共用 plugin」；framework 的 Design Invariants 整節**廢止**（不是改寫），`docs/framework/` 照 target 規則標廢止；spec L.2、H.4.3、target/runtime-composition 對 WindowShell／ImGuiShell 的敘述改成 plugin。審查的 33～66 h 估算大致成立，產出叫 plugin。架構師保留的一點：選擇窗記帳與關窗提示兩個帶判斷狀態的重複**必須真的進共用 plugin**，否則是 framework 的老問題換名。

### 3.2 Editor 預覽：資料只往前流（使用者 2026-11-11 決定，側邊會話轉述）

審查原提的三個選項（晚一幀交換、H-6 開例外、文件與渲染合一）都在問「readback 放哪」，等於沿用今天 `ImGuiShell` 的形狀，**全部不採**。使用者原則：不要用舊思路處理新架構。照 Bevy 的 egui 方式（bevy_egui：UI 在主 World 產生 draw data，渲染 World 消費）：

1. 文件 World 產出兩種資料：**場景**（幾何、相機）與 **UI 的繪製資料**（ImGui draw list：CPU 端的頂點與指令，普通 (b) 資料）；兩者都經 Extract 唯讀進渲染 World。
2. **合成在渲染 World**：先畫場景到 render target，再畫 UI draw list；UI 裡的預覽視窗只是 draw list 裡引用該 render target 的貼圖 id，由渲染 World 解析。沒有影像回到文件 World，**H-6 原樣、不開例外、沒有晚一幀**。
3. 選取與編輯用 CPU 射線打文件 World 的幾何（今天已如此），不依賴影像。
4. Editor 的組合因此與 Client **同形**：文件（兼 Frame）World 加渲染 World，Extract 單向。
5. **ImGui 與 SDL 一樣都是 plugin**（使用者原話）。ImGui plugin 拆兩半：輸入與 widget 邏輯在文件 World 的 System，draw list 的 GPU 繪製在渲染 World 的 System；今天 `OffscreenPreview` 的 readback 路徑與 `ImGuiShell` 的「一個函式從 NewFrame 到 Present」都消失。與 §3.1 一致。

§10.1 的 offscreen-preview 候選因此改寫：不再是「readback 經 Host 交換點」，而是渲染 World 內的 render target 加 UI draw list 合成；framework-tools 報告觀察 4 的「晚一幀」不成立。
### 3.3 (c) 區不能輸出診斷（spec 內部衝突）

spec G.4 第 3 條要移動的呼叫點「改成宣告的 `Diagnostics` 副作用」，但 S-8／Z4 禁止 zone=C 的 System 宣告任何 effect（只有 hook 有 `Diagnostics` 例外），Z1 又禁止它寫 (b) 資料。照現行規格，(c) 完全不能輸出診斷，而 `TraceMovement` 的輸出是診斷資料契約。**規格要改**：要嘛 S-8 比照 hook 開 `Diagnostics` 例外（只寫不讀回，I-5 同理），要嘛定一條 (c)→(b) 的單向診斷通道。另：`Combat.hpp`、`Movement.hpp` 為了 `std::chrono::nanoseconds` 型別 include `<chrono>`，J-17「(c) 檔不得 include `<chrono>`」會誤判，要區分「型別」與「讀時鐘」或改成整數 ns。

### 3.4 spec G.4 的修正與補列

- 第 1 條 `ShotReferenceAge`：今天在 tick A 量（pv7 D-3），不是到達時刻；spec 改成到達時刻會讓粒度在腳本時鐘下差一個 tick。**建議射擊用 `HostClockSample`**（保 D-3 語義），連線品質用 ring 的到達時刻；兩者都要在遷移批以 digest 比對確認。
- 第 3 條位置更正：全域在呼叫點（`PvpMatch.cpp:514`、`:569-580`、`:586`；`LocalPlayerPrediction.cpp:21`、`:31`），`Movement.cpp` 乾淨；`MovementTrace.hpp:101-102`。
- **補列第 4 條**：IO 執行緒＋牆鐘直接寫權威——`SubmitInput` 以遲到量呼叫 `RequestMovementResync`（`MatchRuntimeHost.cpp:249-253`），寫 `connections_` 的 `movementResetScheduled`（`PvpMatch.cpp:255-256`），下一 tick 轉 `movementEpoch`（進 digest）。Ingress／Judge 設計要算進去。
- **補列第 5 條**：IPC 執行緒讀權威快照（`SubmitInput` 的 `match_.Snapshot()` `:178`、`SubmitActionBatch` 的同步驗證 `:309-324`）；機制下 IO 執行緒碰不到 World，驗證搬進 Ingress，回覆時機改變。

### 3.5 H-7 守衛只掃 include 不夠

`MatchRuntimeHost`、`ClientSimulationRole`、`LocalPlayerPrediction` 不 include `RuntimeLoop`，卻手排 `Tick`／`Run`／`Advance` 的順序。H-7 的掃描要加「host 原始碼不得呼叫產品模組的 `Tick`／`Advance`」這類呼叫點規則。

### 3.6 其他待決

- `IRuntimePort` 刪除（建議併入 `v8-arch-remove-object-fps`）；`AnimationInstance` 同。
- `AssetManager` 屬 Frame World 還是 Render World（貼圖上傳要裝置）；`UiRenderer` 與 `WeaponViewModel` 的跨 World 指標都卡在這一題。target 文件沒寫，改造批前要定。
- 四個工具 host 骨架的重複（選擇窗記帳三種實作、關窗提示 3/4）是 framework 標準 plugin 的候選（framework DI 5）；model_viewer `ViewerWindow.cpp:112-113` 的記帳缺陷屬工具行為變更，不在遷移批順手改。
- protobuf generated pool：若 Egress System 直接編碼，要比照 I-5 明列例外。
- 初始成員走 Startup schedule（§4.3）。

## 4. 宣告的四節（出處：product-thirdparty 報告 §5～§8）

### 4.1 (a) Body 拆欄與 golden

今天 `bodies_` 八個寫入點（W1～W8）依 entity-model 第 139 行拆成 Body、Life、InputCursor、Connection 診斷四欄。拆後：Body 仍有移動與復活兩個寫者、InputCursor 有移動與 `ResetMovementEpoch` 兩個，都可用漏斗（復活改發事件或寫中間欄，移動 System 在本 tick 移動前套用）解。**Life 無結論**：死亡在 `ResolveActions` 內且同 tick 內讀後寫；漏斗放「事件＋下一 tick 套用」會讓死亡晚一 tick 出現在 snapshot，放「同 tick 套用」要求 ApplyLife 同時處理復活與死亡兩個 Plan 位置。交遷移批的事前宣告定。`connectionQualityFailures` 進 digest 但今天只寫在 snapshot 複本上，拆後是 Connection 實體的 (c) 欄。

### 4.2 (b) Host 保留職責的現況位置

spec H.7 每列都對到現況（product-thirdparty §6 表）。例外：`MovementDiag.hpp:109` 用 `std::thread` 不經 `RoleThread`，不受 threads DI 4 約束。H-7 掃描清單在 `build/evidence/kernel-audit/product-thirdparty/h7_include_scan.txt`。

### 4.3 (c) G.4 三條的確認

三條都存在；修正與補列見 §3.4。

### 4.4 (d) 第三方重核（對 #176 判定總表）

**判定欄沒有改判**。§2.1「Kernel／plugin 相容性」收窄四處：Recast／Detour 與 Jolt「可當能力」→ 包（先拆；Jolt 只限 (b) World）；ImGui「可當能力」→ 包（全域 context 明列例外）；SDL_GPU「Stage 約束」→ H-3 親和。#176 的 14 列不含 asio、httplib、protobuf、nlohmann、doctest，相容性在 §1.3。回寫 #176 判定總表的差異清單＝這四處加五個新列，標「依 spec v0.2 重核」（下一次文件批）。

### 4.5 (e) 決定性區表（determinism.md §4）的回對（主會話合併）

| 模組 | 審查的 zone 初判 | determinism.md §4 | 回對結果 |
|---|---|---|---|
| engine/base（`Sha256`、`StrictJson`） | C（內容身分與契約資料的入口） | 未列 | **漏列**：兩者都在邊界內（`AnimationContentDigest` 用 `Sha256`；契約資料解析經 `StrictJson`） |
| engine/model：ufbx loader | C（§4.9 內容身分） | §4.2 只寫取樣 (b) | **交叉引用缺**：§4.2 應指向 §4.9 的 loader (c) |
| `FixedTickRuntime` 的 double 累加 | 不適用（Host；G-6 定為 (b)） | 未列 | **漏列**：G-6 的「累加器是 Host 邏輯、不進 (c)」要在 §4 有一列 |
| engine/time 作為 Host judgement 的時鐘來源 | 不適用（經 `HostClockSample` 進 (c)） | §2 提 injected clock；§4 未列 | **漏列**：§4 應有「時鐘樣本是輸入，不是讀」的一列 |
| engine/collision、`src/Collision`、`src/Gameplay`、P7 `StepMovement` | C | §4.3、§4.4 | 一致 |
| P9：`PlayerAnimation` 推進 | C（進 digest 的 animations 段） | §4.2 中間地帶 | **標錯候選**：動畫推進進 digest，不是中間地帶；中間地帶是命中幾何（`PlayerPose`／`PlayerHitbox`／`ShotQuery`），E1 前 (b)。請決定性線核對 |
| P2 Judge（`connectionQualityFailures`） | C（host judgement，進 digest） | §2 host judgement 區 | 一致；但今天只寫 snapshot 複本（§4.1） |
| P13 內容身分（`ArenaContentDigest`、`AnimationContentDigest`） | C | §4.9 | 一致 |
| engine/net、audio、render、ui、input、io、asset、platform、text；framework；工具；呈現 P10；診斷 P8、P12 | B 或不適用 | §4.5 audio 外；其餘未逐一列 | 一致（邊界外） |
| third_party：ufbx | C | §6 第三方（例外） | 一致；#177 方案 A 的對象 |
| third_party：nlohmann | C（契約資料；小數經 `strtod`） | §4.6 提 `strtod` | 一致 |

**漏的模組**：engine/base、`FixedTickRuntime`、engine/time（三列）。**標錯候選**：`PlayerAnimation` 推進（一列）。不改契約；交使用者與決定性線（四處小改照第一個文件批）。

## 5. 範囲の限界（本批）

- LIM-3：四問以掃描加閱讀判，判不到執行期才出現的繞路；「包」只表示靜態看不到違反。掃描本身漏三類（第三方全域 context、pimpl 裡的跨 tick 裸指標、捕獲 `this` 的跨幀 callback）與經別名讀時鐘（`Time::Now`、`Clock::now`）、匿名命名空間非 const 變數、`src` 根不含 `include/` 標頭，三份都以人工補看與補充掃描補上（證據在各 `supplementary/`、`supplement-app-root/`）。共用腳本未改（已引用的 CSV 列號不能變）；修正項記給 J-17 的掃描清單（§6）。
- LIM-1、LIM-2、LIM-5：只在對應列註記，不量成本。
- 工時是粗估區間，不是承諾。

## 6. 給規格與守衛的修正項（下一次規格修訂）

1. S-8／Z4 與 G.4 第 3 條的衝突（§3.3）：(c) 的診斷輸出要有合法路徑。
2. J-17：掃 include `engine/time/MonotonicClock.hpp` 而非函式名；區分 `<chrono>` 的型別用與讀時鐘；掃匿名命名空間的非 const 變數；掃第三方全域 context 的呼叫（`ImGui::`、`SDL_*` 全域查詢）。
3. H-7：加呼叫點規則（§3.5）。
4. G.4：第 1 條改 `HostClockSample`；補第 4、5 條（§3.4）。
5. L.2：`WindowShell` 是 Editor host 骨架，不是 Client host 的。
6. target/runtime-composition：寫明 `AssetManager` 的 World 歸屬（待使用者）。
7. 事前宣告：「依附在服務內的函式庫隨服務判」已補。

## 7. 判定（事前宣告的五條）

| # | 判定 | 結果 |
|---|---|---|
| 1 覆蓋 | 每個目錄一列、third_party 每庫一列、未審附理由 | **成立**（44＝43＋1；掃描根外未審 3 列附理由） |
| 2 證據 | 每列四問各至少一個檔名行號或「掃描無命中＋人工確認」 | **成立**（三份部分報告逐列；framework/include 與 src 併列說明） |
| 3 對抗複核 | 另一模型抽 5 個模組獨立判，≥4/5 一致 | **成立**（Fable／high；4/5 一致；不一致的 engine/input 後端已重判並寫明原因，§8） |
| 4 掃描可重跑、件数一致 | 主 checkout 重跑一次 CSV 與報告引用一致；掃描目錄數＝列數＋未審 | **成立**（全域一次掃描 44／645 與三份合計一致；腳本決定性、未改） |
| 5 不改程式 | git diff 只有 docs 與 scripts | **成立**（三個 worktree 的 diff 只有各自的報告檔；主會話只加 docs） |

## 8. 對抗複核（判定 3）

複核者：Fable／high（與審查者 Opus 不同模型），只讀程式與判法（spec §I.3、§H.7、limits），**沒讀** spec §L 與三份部分報告；用時約 50 分鐘。模組由主會話挑：含一個「留」、一個「包」、一個「重寫」。

| 模組 | 審查者（最終） | 複核者 | 一致 |
|---|---|---|---|
| engine/collision | 留 | 留 | ✓ |
| engine/asset | 包（先收攏） | 包（先拆） | ✓（判定詞同；註記差異是 Q2 範圔的定義，§0 已定） |
| engine/input | 包（先拆）＋留 | Host（backend/sdl）＋留（core） | ✗（後端） |
| ui_editor `EditorApp` | 重寫（介面） | 重寫（介面層） | ✓ |
| `LocalPlayerPrediction` | 重寫 | 重寫（先拆） | ✓ |

**一致率 4/5 ≥ 4/5，判定 3 成立。** 不一致的 engine/input 後端由主會話重判：審查者的初稿本來就是 Host，依「Q1 只排除留」重核時改成「包（先拆）」；複核者依「只持 OS 資源、只由 Host 使用的 adaptor 判 Host」判 Host。兩條規則都在 §0，衝突時以後者為準——判定描述的是**現況**（後端今天是 pump 的零件、產物是 boundary 資料），「拆出狀態機再包」是遷移的設計選項。重判為 **Host（後端）＋留（core）**，§1.1、§2、§1.4 已改。

複核者提出判法模糊五處，主會話的定義寫在 §0（Q2 範圍、包（先拆）與重寫的分界、第三方全域的粒度、一目錄兩性質、純函式庫的 zone）。複核者另注意到主 checkout 當時沒有 spec 與 target（PR #179 合併前），已從審查分支讀；不影響結論。

## 10. 補遺：framework 退役後的去處（使用者 2026-11-11 要求；判定詞不變，去處改寫）

前提改變（§3.1）動到的是去處，不是判定：四問看的是程式現況，判定詞描述要對程式做什麼，兩者都不因 framework 改成 plugins 層而變。engine 22 列不受影響。本節只改三件事，並列出一個審查沒問過的新問題。

### 10.1 framework 與工具各列的去處（判定詞同 §1.2）

| 列 | 判定 | 原去處 | 新去處（plugins 層） |
|---|---|---|---|
| `WindowShell` | Host 骨架重寫 | framework 的視窗 host 骨架與標準 ScheduleDef | **SDL 視窗 runner plugin**：在 finish 安裝 runner（行程入口、pump、live frame、交換點），把 `PlatformEvents` 與對話框回覆寫進 boundary |
| `ImGuiShell` | Host 骨架重寫 | framework 的 ImGui binding | **ImGui plugin**：擁有 ImGui context（全域，明列例外）、在 build 插入標準 Editor stage 序（輸入換幀、NewFrame、Draw、Render、文字輸入套用、RenderDrawData、Present） |
| `HeadlessShell` | Host 原樣 | framework 行程入口 | **headless runner plugin**（或產品 main 自跑；Match 的 `match_main` 迴圈同形） |
| `OffscreenPreview`、`PreviewScene`、`PreviewRenderer` | 包 | framework／工具的預覽 | **offscreen-preview plugin**：渲染 World 內的 render target；場景與 UI draw list 都經 Extract 進來，在渲染 World 合成（§3.2）；readback 路徑消失 |
| `ReadOnlyAssetCatalog`、`CatalogPlan` | 包／留 | framework | **asset-catalog plugin**：Resource 加 `Io:file` System；`CatalogPlan` 是資料契約（owner＝此 plugin） |
| `EventSounds` 加 Mixer 接線 | 留／服務 | framework＋engine audio | **event-sounds plugin**：資料契約（owner＝此 plugin）加 `Submitter` Resource 的接線；PvP 也用 |
| 選擇窗記帳、關窗提示（四工具重複） | — | 各工具各寫 | **editor-dialogs plugin**（架構師保留：必須真的進共用 plugin） |
| `PreviewCamera`、`PreviewTexture`、`UiTextInput`、`AbsolutePath`、json binding | 留 | framework | **函式庫**（與層無關；放哪個 target 由計畫批定） |
| 四個工具的 client | Host 重寫 | 各工具的 Editor host | 各工具的 **main＝plugin 清單＋Run**；工具專有的編輯 System 進該工具自己的 plugin |

### 10.2 產品兩列的形狀改寫（判定詞仍是重寫／Host）

- **P11 `PvpApplication`**：原判「重寫：Client host＋ClientFrame／ClientRender System」。三層規則下產品只在 main 列 plugin 加 Run，所以 Client host 的骨架（pump、live frame、三個 Runtime 的交換點與時鐘策略）也是 **runner plugin** 提供，不是手寫 Host；PvP 專有的 System 進產品 plugin。spec H.4.2 要跟著改（本稿不改規格，記為規格修正第 8 項）。
- **S1 `main`／`match_main`**：Match 的每 50 ms 迴圈與停止順序＝**headless runner plugin**；`main` 只剩 plugin 清單與 Run。

### 10.3 §0 規則的敘述修正

「只持 OS 資源、只由 Host 使用的 adaptor 判 Host」的四個 engine 模組（os、time、threads、platform）與 `FixedTickRuntime`：判定不變，敘述改為「被 runner plugin 使用的 Engine adaptor」。Host 變薄後，「Host」一詞在報告裡指的是 runner plugin 加產品 main 的那一層，不再是 framework。

### 10.4 審查沒問過的新問題：plugin 的邊界（交計畫批）

審查判的是模組，沒判「哪些東西合成一個 plugin、每個 plugin 的依賴正向清單、刪掉它別的東西還成立嗎」。這是改造計畫的設計題。審查證據裡看得到的切法候選（**不是決定**）：

| 候選 plugin | 收進去的現況 | 依賴（正向） | 刪掉它會壞什麼 |
|---|---|---|---|
| sdl-window-runner | `WindowShell`、`SdlPlatform` 的 pump 與 live frame、`SdlInput` 後端、對話框回覆 | engine/platform、input、time | 所有視窗程式；headless 不受影響 |
| headless-runner | `HeadlessShell`、`match_main` 迴圈與停止順序 | engine/time、threads、os | Match 與工具 CLI 模式 |
| imgui | `ImGuiShell`、ImGui context、標準 Editor stage；**兩半**：輸入與 widget 邏輯（文件 World 的 System）、draw list 的 GPU 繪製（渲染 World 的 System） | sdl-window-runner、engine/render | 四個工具；PvP 不受影響 |
| editor-dialogs | 選擇窗記帳（三種實作統一）、關窗提示 | sdl-window-runner | 四個工具的對話框 |
| offscreen-preview | `OffscreenPreview`、`PreviewScene`、`PreviewRenderer`（render target 與場景合成；無 readback） | engine/render、imgui（draw list 的貼圖 id 解析） | 三個有預覽的工具 |
| asset-catalog | `ReadOnlyAssetCatalog`、`CatalogPlan` | engine/asset、io | 用 catalog 的工具與 PvP 載入 |
| event-sounds | `EventSounds`、Mixer 與 `Submitter` 接線 | engine/audio | PvP 與 model_viewer 的音效 |
| net-link | `IpcHost`／`ClientConnection` 的 IO 服務、asio、httplib、protobuf、`Link` Resource | engine/net、threads | Match 與 Client 的連線；Editor 不受影響 |
| trace-diagnostics | `TraceSink`／`LogFile`／`MovementDiag` 服務、`Diagnostics` effect | engine/trace | 診斷輸出 |
| object_fps_pvp（產品） | (c) 模擬 System、Ingress／Judge／Egress、呈現 System、scene v1 | 上列多個 plugins＋engine | 只有 PvP |

每個候選要在計畫批寫：目標、依賴清單、Removability 的驗證方式（AGENTS §7：刪掉後 Engine 與其他 plugins 成立）、owner。

### 10.5 規格修正追加

- 第 8 項：spec H.4.2 的 Client host 改成 runner plugin 提供骨架（§10.2）。
- 第 9 項：spec I.1 外部系統表與 H.7「Host 保留職責」的「Host」改寫為「runner plugin 加產品 main」；H.7 的職責不變，持有者變。

## 11. 下一步

- ~~§3.1~~（已定：framework 退役→`plugins/`）；~~§3.2~~（已定：資料只往前流，H-6 原樣）；使用者決定 §3.6（IRuntimePort、AssetManager 歸屬、初始成員走 Startup）、§4.5（區表三漏一錯）。
- 規格修訂批（§6 七項）與第一個文件批合併規劃。
- `-spike-pageguard`（事前宣告待寫）。
- 改造批次計畫：以本報告的成本形狀分批（組織層先：Kernel 本體 → Match host 遷移 → Client → Editor），§3 Delta 七項在計畫層寫一次。
