# UI（`engine/modules/ui`）與 `world_ui`、`ui_render`

更新：2026-11-24（新頁，SDL 線子批 U `v8-arch-gyo2-sdl-u`）。由來：[SDL 總宣告](sdl-declaration.zh-Hant.md)修訂 21（使用者 2026-11-23 動工前核准「SDL 照建議核准」，HANDOFF `v8-arch-gyo2-sdl-declaration-approved`）第 3.2 節 `GYO::Ui`、`UiRenderer` 兩列、第 3.3 節 `ui`、`world_ui`、`ui_render` 三列、第 6.6 節「`gyo.ui` 的節點模型」一列、第 10.2 節 U 列；前提 P17（ui 留 Engine，使用者 2026-11-13）。新 AGENTS（#296）之後，事前宣告寫完就動手；Design Invariants 沒有推翻使用者決定的條文，不逐條核准（照 R2b 的先例）。

Owner：架構。程式：`engine/modules/ui/`（`GYO::Engine::Ui`，target `GYO::Ui`，函式庫）、`engine/modules/world_ui/`（`GYO::Engine::WorldUi`，登記單位 `WorldUiModule`，來源側）、`engine/modules/ui_render/`（`GYO::Engine::UiRender`，登記單位 `UiRenderModule`，渲染側）。行為測試：`tests/common/ui/`。

**用詞**：「卡片照 L17 判」＝這張卡照命名規範、別家原始碼、已定的原則自己判的，不是使用者的話，可以推翻。

## 一個例子

PvP 的大廳（`games/object_fps_pvp/docs/porting-inventory.zh-Hant.md` PV12，master 的 `assets/object_fps_pvp/ui/pvp_lobby.json`）：遊戲在客戶端 World 生一個實體，掛 `UiRoot`（文件＝`gyo.ui` 素材的 Handle、畫布 `lobby`）與 `UiBindings`（`address`＝`"127.0.0.1:8080"`、`room`、`status`、`hint`）。視窗 1280×720、Retina 的 2 倍：相機的視區是 2560×1440 個實體像素，設計畫布 1280×720 照 `fit` 放大 2 倍。

- 玩家按 Tab：焦點照 `focus_order` 從 `refresh` 移到 `create`，`create` 的底色換成 `focused`。
- 滑鼠點位址欄：開始編輯，游標落在點到的字之間（`text` 的 `HitTest`）；`world_ui` 把 `TextInputRequest` 設成開、候選窗的矩形是位址欄（換回視窗的邏輯像素）；`sdl_window` 下一幀開輸入法。
- 用日文輸入法打「がっこう」：組字中的字畫在游標處、底下畫一條線；確定之後字進到編輯中的文字。按 Enter：送出 `UiAction{"pvp.submit_address", "address", 字串}`（`Message`）；遊戲讀到、改寫 `address` 這個 binding。
- 按「Join Room」：送出 `UiAction{"pvp.join", "join"}`。
- 畫面：`world_ui` 在 `PostUpdate` 排好每個字形（字形進 `UiText` 的圖集）、產出這一幀的繪製清單；渲染 World 的 `ui_render` 經 Extract 讀它與圖集的變更，把圖集頁上傳成貼圖、在那台相機的 2D pass 之後加一個 UI 的 2D pass。

## 1. 位置與依賴

```text
GYO::Json、GYO::Text、GYO::Asset、GYO::Image（只取型別）◄── GYO::Ui（函式庫）
GYO::Ui、GYO::Input、GYO::Window、GYO::Camera、GYO::WorldAsset ◄── GYO::WorldUi（來源側）
GYO::Ui、GYO::Render、GYO::RenderWorld、GYO::CameraRender ◄── GYO::UiRender（渲染側）
```

- `ui`：`library_only=yes`；`depends=json;text;asset;image`。總宣告第 3.3 節寫 `json、text`；卡片照 L17 加 `asset`（`gyo.ui` 的 loader、對字型與圖片的引用）與 `image`（引用圖片時的型別 `ImageData`，只取型別）——第 3.3 節「子批可照規範微調」。**不依賴 `render`、`input`、SDL**（同 Bevy 的 `bevy_ui` 不依賴 `bevy_render`；總宣告第 3.2 節）。
- `world_ui`：來源側的單位，`default=no`；`depends=ui;input;window;camera;world_asset`。總宣告寫 `ui、input`；卡片照 L17 加 `camera`（UI 畫在哪台相機、它的視區與縮放，同 Bevy 的 `bevy_ui` → `bevy_camera`）、`window`（貼上讀 `window` 的剪貼簿鏡像）、`world_asset`（讀文件與字型）。這三個都是 `default=yes` 的單位，帶 `world_ui` 的 World 本來就有它們。
- `ui_render`：渲染側的單位，`default=no`；`depends=ui;render;render_world;camera_render`。總宣告寫 `ui、render、text、render_world`；`text` 經 `ui` 傳遞（ui_render 只讀圖集，型別在 `ui` 的公開標頭裡已經帶到）；卡片照 L17 加 `camera_render`（UI 的 pass 放進相機的槽，`CameraPasses` 由它清與合成；同 `sprite_render`）。
- 來源 World 要有 `world_ui`（它登記 `UiRoot` 等元件與 `UiText`），否則渲染側讀不到——Kernel 的 R-E4。

## 2. 範圍

### 2.1 這一批做的

| # | 內容 |
|---|---|
| U1 | `gyo.ui` v1／v2 的資料型別、codec（讀、驗證、寫）、資料契約測試。讀寫都經 JSON Module |
| U2 | `gyo.ui` 的 asset loader（型別 `"ui"`、格式 `json`），文件裡的字型、圖片以 id 宣告為引用 |
| U3 | layout：RectTransform（anchor、pivot、position、size_delta）、設計畫布 `fit`、裁切、`fixed_step_list` 的項目；文字同步量字（`text` 的 Shape／Layout）後照對齊放進框 |
| U4 | runtime：焦點（`focus_order`、`default_focus`、指標）、button、horizontal_slider、text_field（插入、刪除、移動、全選、組字、送出、取消、binding 被改時丟棄編輯）、`cancel_action`、binding 與型別檢查、文字過濾函式 |
| U5 | 繪製清單：矩形、圖片、字形（圖集頁＋矩形）；游標、選取、組字底線也是矩形 |
| U6 | `world_ui`：`UiRoot`、`UiBindings`、`UiRootState`、`UiAction`（`Message`）、`UiText`（Resource）、`UiInput`（Resource）；預設的輸入對應 System、互動 System、繪製清單 System、寫 `TextInputRequest` |
| U7 | `ui_render`：Extract UI 的繪製清單與圖集變更；圖集頁上傳成 `RenderTextures`；每台相機一個 UI 的 2D pass |
| U8 | 大廳（PV12）的情境測試：真的 `pvp_lobby.json`、真的字型、注入 `input` 的 `Message`，經整條 `world_ui` → Extract → `ui_render` 到 `RenderCommands` |

### 2.2 不在這一批

| 項目 | 去處 |
|---|---|
| 大廳的接線（`LobbyActions`、位址的字元過濾、按鈕對到網路） | PvP 移植卡（本頁第 9 節寫接法） |
| `ui_editor`（Editor 端） | 工具回來時；本批只守資料契約 |
| 文字框的捲動、多行、依字詞移動、復原 | 有點名的 Feature 時（master 也沒有；`max_length` 讓欄位放得下） |
| 文字框的滑鼠拖曳選取 | 同上（master 只有全選） |
| 手把的焦點移動 | 有點名的 Feature 時（`UiInput` 的欄位已經是中性的，作者可以自己寫） |
| `hit_feedback.json` | 不是 `gyo.ui`（PvP 自己的 JSON，盤點第 32 列） |

## 3. 別家怎麼做（Bevy 為主參考，P26 的判法）

Bevy 6a911653f（`~/Code/GitHome/bevy`）；第二家 Godot（`~/Code/GitHome/godot`）；Unity UI Toolkit 只憑一般知識〔知〕。

| 項目 | Bevy | 別家 | GYO（本批） | 衝突點與膠水 |
|---|---|---|---|---|
| **節點模型** | 每個節點是實體，階層是 `ChildOf`；layout 用 taffy（flexbox／grid） | Godot：`Control` 是場景樹的節點（不是 ECS），位置用四個 anchor＋offset（`scene/gui/control.cpp:141-144`）；Unity：uGUI 是 GameObject＋`RectTransform`，UI Toolkit 是 UXML 文件 → `VisualElement` 樹（不在 GameObject／Entities 裡）〔知〕 | **文件樹，不是實體**（卡片照 L17 判，第 3.1 節） | 見第 3.1 節 |
| 進 ECS 的入口 | 根節點＋`UiTargetCamera` | Godot：`CanvasLayer` | 一份文件一個實體（`UiRoot`，可選 `camera`） | 接合點：`UiRoot` ≈ Bevy 的 UI 根節點；`UiRoot::camera` ≈ `UiTargetCamera` |
| 預設相機 | `DefaultUiCamera::get`：有 `IsDefaultUiCamera` 的那台，否則畫到主視窗的相機裡 `(order, entity)` 最大的（`bevy_ui/src/ui_node.rs:3471-3490`） | — | 同 Bevy：`camera` 沒給時，目標 0 的相機裡 `(order, entity)` 最大的 | 沒有 `IsDefaultUiCamera`（沒有點名的需求；要指定就給 `camera`） |
| 互動的時點 | `UiSystems::Focus` 在 `PreUpdate`（`bevy_ui/src/lib.rs:94`）；picking 也在 `PreUpdate`（`picking_backend.rs:79`） | Godot：輸入事件即時分派 | `UpdateUi` 在 `PreUpdate`、排在 `input` 的 set 之後 | 同 Bevy：作者在 `Update` 讀得到同一幀的 `UiAction` |
| layout 與文字的時點 | `PostUpdate` 的 `UiSystems::Layout`、`text_system`（點陣化也在 main world，`bevy_ui/src/widget/text.rs:389`） | — | `ComposeUi` 在 `PostUpdate`；排字、點陣化進圖集都在來源 World | 同 Bevy |
| 渲染 | `bevy_ui_render` 在 `ExtractSchedule` 抽節點與字形（`bevy_ui_render/src/lib.rs:246`、`:1628`），每個 view 一個 `TransparentUi` phase，排在主 pass 之後 | — | `ui_render` 在 extract schedule 抽繪製清單與圖集變更；每台相機一個 UI 的 `Pass2D`，排在那台相機的 2D／3D pass 之後 | 同 Bevy 的分法；GYO 沒有 phase，用 `CameraPasses` 的槽（`sprite_render` 同形） |
| 動作 | `Interaction` 元件、`Activate`／`Pointer<Click>` 的實體事件（observer） | Godot：`pressed` signal | `UiAction`（`Message`：根實體、action、元素 id、payload） | 文件樹沒有逐節點的實體，事件帶元素 id；作者照 action 名分派（同 master） |
| 文字框與輸入法 | `bevy_ui_widgets` 的文字輸入、`Window.ime_enabled`／`ime_position` | Godot：`LineEdit` 有 `ime_text`、開關輸入法並設位置（`scene/gui/line_edit.cpp:129-150`） | `world_ui` 寫 `TextInputRequest`（W 的請求資料）、讀 `TextComposition`／`TextCommitted` | 同 Bevy／Godot：請求資料、後端下一幀套用 |
| 焦點 | `InputFocus` Resource、Tab 導覽（`bevy_input_focus`） | Godot：`focus_next`／`focus_neighbor_*` | 文件的 `focus_order`（資料契約） | 照 master 的資料契約 |

### 3.1 節點模型：文件樹（卡片照 L17 判；總宣告第 6.6 節要 U 比）

比過的：

- **(甲) 文件樹（選）**：`UiDocument` 是一份資料；`UiRuntime` 持有焦點、編輯中的文字；`world_ui` 用一個實體（`UiRoot`）代表一份文件。
- **(乙) 照 Bevy，節點是實體**：codec 讀出後生成實體（每個元素一個、`ChildOf` 接成樹），互動與 layout 改走 System。

選 (甲) 的理由：

1. **`gyo.ui` 是資料契約**（Editor↔Runtime，總宣告第 3.2 節、第 6.6 節）。它的形是 Unity uGUI／Godot `Control` 那一型（anchor＋pivot＋position＋size_delta、`focus_order` 用 id、`fixed_step_list` 用樣板展開），不是 Bevy 的 flexbox。走 (乙) 也不會變成 Bevy 的 UI：layout 還是 RectTransform、不用 taffy，只是把一份 uGUI 型的資料拆成實體——這正是 P26 說的「縫合怪」：Bevy 的節點模型配 Unity 的 layout。
2. **(乙) 的膠水**：文件 ↔ 實體的同步（Editor 改文件、熱重載時要比對差異再改實體）、`focus_order`／`default_focus` 從 id 找實體、binding 每幀寫進各節點、`fixed_step_list` 照清單長度生滅樣板實體、裁切沿階層傳。估計比 (甲) 多一個同步層與五六個 System；master 沒有這一層，PvP 的大廳也用不到。
3. **成熟的先例**：Unity 的 UI Toolkit 也是「文件（UXML）→ 自己的元素樹」，不放進 GameObject／Entities〔知〕；Godot 的 `Control` 在場景樹裡、不在 ECS。
4. **進 ECS 的部分照 Bevy**：根是實體、畫到哪台相機照 `UiTargetCamera`／`DefaultUiCamera`、互動在 `PreUpdate`、layout 與文字在 `PostUpdate`、點陣化在來源 World、渲染側經 Extract。

(甲) 的代價（衝突點）：作者不能在單一個按鈕上掛自己的元件或 observer（動作照 action 名分派）；UI 元素不是 `ChildOf` 的子，不能拿 Kernel 的階層查 UI。改照 (乙) 的條件寫在 Replacement Criteria UI-RC1。

## 4. 設計（Contract）

### 4.1 資料契約 `gyo.ui`（v1、v2）

- 照 master（`engine/ui/include/engine/ui/UiDocument.hpp`、`UiDocumentCodec.cpp`、`UiValidation.cpp`、`UiSerialization.cpp`）：`schema`＝`"gyo.ui"`，`version` 1 或 2（v2 加 `text_field` 與字串 payload）；元素八種（container、panel、image、text、button、horizontal_slider、fixed_step_list、text_field）；顏色 `#RRGGBBAA`（sRGB）；文字來源 literal／binding／item_field／compose／select；顏色來源 color／select；binding 六種型別；`design_canvas` 的 `scale_mode` 只有 `fit`。
- **讀**經 JSON Module 的 `Parse`（RFC 8259 嚴格：BOM、註解、重複鍵、數字溢位都拒）。master 用 nlohmann 時這些多半也拒或未定義；**版本不升**（卡片照 L17 判：合法的 v1／v2 文件讀出的值不變；只有本來就不合規的文件從「未定義」變成「拒」）。JSON 宣告第 452 行要求「各契約回到 gyo2 時寫明經 JSON Module 讀的行為、版本升不升，交使用者」——寫在這裡，回報主會話（第 10 節）。
- **寫**經 `Json::Writer`：欄位順序固定、顏色大寫 `#RRGGBBAA`、結尾一個換行。**偏離 master**：master 用兩格縮排；JSON Module 只有一種輸出格式（J-5，緊湊），所以寫出的是一行。讀回的值與 master 相同（資料契約不含排版）。
- 驗證照 master 的規則（id 唯一、引用存在、binding 型別、`focus_order` 只列可互動元素等），錯誤帶 JSON Pointer。
- 錯誤型別 `UiFailure`（N25：E2，這一份文件沒讀成，其他都健康），欄位 `code`、`message`、`detail`、`source`、`jsonPointer`。

### 4.2 Loader

`UiDocumentLoader`：型別 `"ui"`（`AssetTypeName<UiAsset>`）、格式 `json`。輸出 `UiAsset{document, fonts, textures}`：文件，以及文件的每個字型、圖片 id 宣告成的引用（`LoadContext::ReferenceId`，`AssetRef<FontData>`、`AssetRef<ImageData>`）。所以「文件 Complete」＝字型與圖片都讀好了（asset 宣告 5.17）。

### 4.3 座標與縮放

- UI 排在**相機視區的實體像素**裡（`ComputedCamera::viewport`，原點在視區左上、y 向下）。設計畫布照 `fit` 放進視區：一個等比的倍率、置中。倍率裡已經含 DPI（Retina 上視區是 2 倍的像素），字照 `point_size × 倍率` 點陣化，所以 Retina 上字是清楚的。
- 指標：`input` 的 `CursorPosition` 是視窗的邏輯像素 → 乘 `scaleFactor`、減視區原點。
- `TextInputRequest` 的矩形：反過來，換回視窗的邏輯像素。
- **偏離 Bevy**：Bevy 的 UI 用邏輯像素＋`UiScale`，渲染時乘縮放；GYO 的 `gyo.ui` 有設計畫布，倍率由 `fit` 決定，直接排在實體像素裡少一次換算（膠水：0；接合點：指標與 `TextInputRequest` 的兩個換算）。

### 4.4 文字

- `UiText`（`world_ui` 登記的 Resource；型別在 `ui`）：`TextContext`、一個 `GlyphAtlas`（Alpha8）、字型表（素材 id → `FontId`）、排字的快取（文字、字型、像素大小 → `TextLayout`，一幀沒用到就丟）。同 Bevy 的 `FontAtlasSet`／`CosmicFontSystem` 是 main world 的 Resource（總宣告第 6.6 節 `text` 列的傾向）。
- 字型的來源：`UiRoot` 的文件是素材時，`world_ui` 從 `AssetView` 讀引用的 `FontData` 加進去；作者也可以自己 `UiText::AddFont(id, data)`（測試、工具）。字型還沒到：那段文字不畫、互動照常（同 Bevy：字型沒讀好就不排）。
- 排字：不自動換行（同 master；`\n` 照硬換行），排好之後照 `horizontal_align`／`vertical_align` 放進框；字形原點取整數像素（`text` 的 TX-8、`GlyphPlacement` 的畫法）。
- 文字框：游標、選取、組字底線的位置用 `text` 的 `CaretAt`／`RangeRects`；滑鼠點用 `HitTest` 決定游標（**改進 master**：master 點下去游標一律在尾端）。編輯照 master 以 code point 為單位（`max_length` 數 code point）；畫游標時退到最近的字素邊界（`CaretAt` 只收字素邊界）。輸入法的組字游標照 `input` 的說明是平台的單位（Windows、macOS 是 UTF-16），換成位元組時照 UTF-16 數。
- 圖集滿了：`text` 開新頁（`GlyphAtlas` 的頁）；ui_render 每頁一張貼圖。

### 4.5 runtime

照 master 的 `UiRuntime`（Design Invariants 4～7 的行為）：

- `Initialize(document)`（先驗證）、`ActivateCanvas(id)`、`SetTextFilter(fn)`。
- `Update(input, bindings, viewport, text)` → 這一幀的動作；`Compose(bindings, viewport, text)` → 繪製清單；`EvaluateLayout` 給工具與測試。
- 錯誤（缺 binding、型別不符、引用不存在）回 `UiFailure`，不斷言（E2）。
- `UiInput`（中性的一幀輸入，master 的 `UiInputFrame`）：焦點前／後、調整前／後、確定、取消、文字編輯（插入、組字、退格、刪除、左右、Home／End、全選）、指標（在不在、位置、主鍵按下／按住／放開）。

### 4.6 `world_ui`

| 名字 | 種類 | 內容 |
|---|---|---|
| `UiRoot` | 元件（作者寫） | 文件（`Handle<UiAsset>` 或作者持有的 `shared_ptr<const UiDocument>`）、畫布 id、`camera`（可選的相機實體）、`order`（同一台相機上的先後）、`acceptsInput` |
| `UiBindings` | 元件（作者寫） | binding 值的表 |
| `UiRootState` | 元件（`world_ui` 寫，衍生） | runtime（焦點、編輯中的文字）、這一幀的繪製清單、畫在哪台相機、最後一次的 `UiFailure` |
| `UiAction` | `Message` | 根實體、action、元素 id、payload |
| `UiInput` | Resource | 這一幀的中性輸入；預設由 `MapUiInput` 寫 |
| `UiText` | Resource | 第 4.4 節 |
| `UiOptions` | `WorldDescription::options` | 各 System 的 schedule／stage、`defaultInputMapping`（預設 true） |

System（都在公開的 set `kUiSet`）：

1. `MapUiInput`（`PreUpdate`，`after` `input` 的 set）：把 `KeyboardInput`、`TextCommitted`、`TextComposition` 依時刻合併（兩種 `Message` 的先後不保證，用事件的 `time` 排），加上 `ButtonInput<MouseButton>`、`CursorPosition`，寫成 `UiInput`。預設的鍵：Tab／Shift+Tab＝焦點後／前，↑↓＝焦點前／後，←→＝調整（slider），Enter／小鍵盤 Enter＝確定，Escape＝取消；編輯中：Backspace、Delete、←→、Home、End、Ctrl 或 ⌘＋A＝全選、Ctrl 或 ⌘＋V＝貼上 `window` 的剪貼簿（含 OS 的按鍵重複）。作者可以關掉它、自己寫 `UiInput`（AGENTS §1.10：預設方便、不綁作者）。
2. `UpdateUi`（`PreUpdate`，`after` `MapUiInput`）：每個 `UiRoot`：文件讀好了就初始化 runtime、啟用畫布；用相機的視區跑 `Update`，動作寫成 `UiAction`。
3. `ComposeUi`（`PostUpdate`）：字型加進 `UiText`、`Compose` 出繪製清單（字形在這裡點陣化進圖集）；有文字框在編輯時寫 `TextInputRequest`（開、矩形），編輯結束時寫一次關。
4. 錯誤：寫進 `UiRootState`、那一份文件這一幀不畫；**System 不回錯誤**（回錯誤會讓 World 壞，O-11；這只是「這份文件沒做成」，E2）。

`TextInputRequest` 兩個寫者（`world_ui`、ImGui）：照 IN-4，後跑的贏、作者排。`world_ui` 只在自己的意思改變時寫，不每幀蓋掉。

### 4.7 `ui_render`

- `ExtractUi`（extract schedule）：讀來源的 `UiRoot`＋`UiRootState`，抽成 `ExtractedUiRoots`（每份：相機實體、order、繪製清單的複本）；讀來源的 `UiText`（`ExtractRes`），照 `ChangesSince(上次的 revision)` 把圖集的變更寫成 `RenderTextures` 的 `Create`／`Update`（Alpha8 → RGBA：白色、alpha＝覆蓋率，`Nearest`）；圖片元素經 `RenderImages`（同 `sprite_render`）。
- `QueueUiPasses`（`PostUpdate`，在 `sprite_render`、`mesh_render` 的 `Update` 之後）：每台抽出的相機，把畫到它的 UI 照 `(order, entity)` 排好、做成一個 `Pass2D`（`viewport`＝相機的視區，`view`＝視區自己的像素），放進那台相機的 `CameraPasses` 槽的最後；`camera_render` 合成時就在那台相機的世界畫面之上。
- 顏色：`UiColor` 是線性的（照 master，從 sRGB 的十六進位解出），寫進 `Vertex2D` 時編回 sRGB 的位元組（2D pass 照存的位元組混合，`render.zh-Hant.md` 第 2 節）。**和 master 的差**：master 在線性空間混合半透明，GYO2 的 2D pass 在 sRGB 位元組上混合（同 Dear ImGui），半透明的顏色會略有不同。
- 裁切：每個基本形的 `clip` 交給 `Draw2D::clip`；同一個貼圖、同一個裁切的連續基本形合成一個 draw。

## 5. Design Invariants

| # | 條文 | 守的方式 |
|---|---|---|
| UI-1 | `ui` 不依賴 `render`、`input`、`window`、任何 Plugin 與 SDL；`world_ui` 不依賴 `render`；`ui_render` 不依賴 `input`、`window` | 連結的期待（`ExpectLinks`）、`naming`／`os` 守衛 |
| UI-2 | `gyo.ui` v1、v2 的文件讀出寫回、再讀出的值相同；master 的 golden 文件照讀 | 資料契約測試 |
| UI-3 | 驗證不過的文件不進 runtime（`Initialize` 先驗證）；runtime 的錯誤回 `UiFailure`，不斷言、不讓 World 壞 | 測試 |
| UI-4 | 編輯中的文字只活到送出、取消、失焦、或它的 binding 被改（master DI 4） | 測試 |
| UI-5 | 組字只畫、不進文字，確定才進（master DI 6）；Enter 在組字中不送出（組字由輸入法吃掉 Enter，`sdl_window` 不把它當 `KeyboardInput` 送；`world_ui` 這一側：有組字時的 Enter 不算確定） | 測試 |
| UI-6 | 哪個鍵是什麼操作是 `MapUiInput` 的預設、可關（master DI 7 的「產品的對應」→ 預設＋可換） | 測試 |
| UI-7 | 渲染 World 只讀：`ui_render` 不寫來源的任何東西，圖集只讀 `ChangesSince` | Kernel 的 `Extract<Read<…>>`（DI-KS 13） |

## Replacement Criteria

- **UI-RC1**（節點模型）：Editor 的 `gyo.ui` 不再是 Editor↔Runtime 的資料形式、或出現要在單一 UI 元素上掛作者元件的點名需求時，重比第 3.1 節的 (乙)。
- **UI-RC2**（排字快取）：一幀的排字時間超過幀預算的 10%（量了才算）時，改成以元素為鍵的保留式快取。

## 6. 測試、突變

### 6.1 測試（`tests/common/ui/`，標籤 `cpu`）

| ctest | 內容 |
|---|---|
| `ui.contract` | master 的 golden 文件（v1、v2）讀、寫、再讀相同；每條驗證規則的反例與 JSON Pointer；`pvp_lobby.json` 讀得進來 |
| `ui.runtime` | 焦點（Tab、Shift+Tab、指標、`default_focus`）、button、slider（指標拖曳、鍵盤調整、量化）、`fixed_step_list`、`cancel_action`、binding 錯誤 |
| `ui.text_field` | 插入、退格、刪除、移動、全選、`max_length`、過濾函式、組字只畫不進、確定、送出、取消、binding 被改時丟棄、點擊放游標 |
| `ui.layout` | anchor／pivot／`fit`、裁切、對齊、字形的精確位置（真的字型） |
| `ui.world` | `world_ui`：輸入 `Message` → `UiAction`；`TextInputRequest` 的開關與矩形；文件素材與字型經 `world_asset`；錯誤寫進 `UiRootState`、World 不壞 |
| `ui.render` | `ui_render`：繪製清單 → `Pass2D`（頂點、顏色、裁切、貼圖）；圖集變更 → `RenderTextures`；多台相機、預設相機 |
| `ui.lobby` | 第 2.1 節 U8：大廳的整條路 |

測試用字型：`tests/common/text/` 已有的字型（T 批放的）。

### 6.2 突變（`docs/architecture/target/v2/scripts/ui_mutations.json`）

只對寫明的契約與行為下突變（「誤用」不加守衛）：焦點的前後與繞圈、slider 的量化、`max_length`、組字不進文字、Enter 在組字中不送出、binding 被改時丟棄、預設相機的 `(order, entity)`、指標換算的 `scaleFactor`、`TextInputRequest` 換回邏輯像素、sRGB 編碼、UI pass 排在相機槽的最後、圖集只上傳變更、codec 的版本檢查與欄位、驗證規則各一條。

## 7. 判定

- 大廳的情境測試在本機 `ctest -L "cpu|gpu"` 與 L1 三平台都過。
- 新突變全部 killed（存活的要分「契約要補／誤用不補」，照 classify-gaps 的分法寫進報告）。

## 8. 範囲の限界

- IME 只能實機驗；CI 驗的是 `TextComposition`／`TextCommitted` 的路徑（同總宣告第 15 節）。
- GPU 的像素只有 Linux Lavapipe；本批的渲染測試讀 `RenderCommands`，不比像素。
- 半透明的顏色和 master 略有不同（第 4.7 節）。
- 文字框不捲動、單行；游標以 code point 移動，組合字元的中間可以停（畫的時候退到字素邊界）。
- 輸入法組字游標的單位：照 UTF-16 換算；Linux 的輸入法若給 code point，BMP 以外的字會偏。

## 9. 留給 PvP 卡的接法

大廳（PV12）的接線歸 PvP 移植卡。照下面接，整條路已由 `ui.lobby` 驗過（真的 `pvp_lobby.json`、真的字型、輸入 `Message` → `UiAction`、組字、送出、`RenderCommands`）。

1. **產品的 Module 列**：客戶端的來源 World 帶 `world_ui`（它要的 `input`、`window`、`camera`、`world_asset` 都是 `default=yes`；明列 `units` 的產品要寫出來），渲染 World 帶 `ui_render`（和 `render_world`、`camera_render` 一起；R2b 的形）。來源 World 也要有 `world_render`（`camera_render` 抽相機的清除設定）。
2. **loader**：`AssetLoaderRegistry::Register<Ui::UiDocumentLoader>()`、`Register<Text::FontLoader>()`。catalog 裡 `pvp_lobby.json` 的型別寫 `"ui"`、字型 `object_fps_pvp.font.ui` 的型別寫 `"font"`；文件讀好、引用的字型也讀好之後才畫（字型由 `world_ui` 從素材加進 `UiText`）。
3. **掛上去**：生一個實體，`UiRoot{document = AssetRequests::Load<Ui::UiAsset>(id), canvas = "lobby"}`、`UiBindings{address, room, status, hint}`。相機不必指定（目標 0 上 `(order, entity)` 最大的那台；3D 場景＋HUD 時可以用 `UiRoot::camera` 指 HUD 的相機）。大廳關掉＝拿掉實體或換掉 `canvas`。
4. **動作**：在 `Update`（或 `after` `WorldUi::kUiSet`）讀 `MessageReader<Ui::UiAction>`，照 `action` 分派（`pvp.submit_address` 的 payload 是字串，`pvp.refresh` 等沒有 payload）；處理完改寫 `UiBindings`（例：送出後把新的位址寫回 `address`；binding 被改時，編輯中的文字會被丟棄，UI-4）。
5. **位址的字元過濾**（master 的 `Pvp/AddressText`）：放進 `UiRoot::filter`（`std::function<std::string(element, text)>`），只對確定的文字（打字、貼上、組字確定）作用。
6. **輸入法**：`world_ui` 在文字框編輯時寫 `TextInputRequest`（開、候選窗的矩形，視窗的邏輯像素），編輯結束時寫一次關；`sdl_window` 下一幀套用。同一個 World 還有 ImGui 時，照 IN-4 由作者排先後（`world_ui` 只在自己的意思改變時寫）。
7. **鍵的對應**：預設的 `MapUiInput` 就是大廳要的（Tab／Shift+Tab、Enter、Escape、編輯鍵、Ctrl 或 ⌘＋A／V）。要換手把或別的鍵：`UiOptions::defaultInputMapping = false`，自己寫 `Ui::UiInput` 這個 Resource（指標是視窗的邏輯像素）。
8. **錯誤**：缺 binding、文件驗證不過等，寫在 `UiRootState::failure`（那一份文件不畫，World 照跑）；要顯示或記 log 由產品讀它。

## 10. 回報主會話

- 節點模型照 L17 選了文件樹（第 3.1 節）；總宣告第 6.6 節寫的是「U 的宣告比、交使用者」——新 AGENTS 下卡片自己判，請主會話判要不要再交使用者。
- `gyo.ui` 經 JSON Module 讀的行為與版本不升（第 4.1 節）：JSON 宣告第 452 行寫「交使用者」，同上。
- 寫出由兩格縮排改成一行（J-5）：Editor 回來時，`gyo.ui` 檔的 diff 會變成整行；要恢復縮排得在 JSON Module 加一種輸出格式（動 J-5）。
