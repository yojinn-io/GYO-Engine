# Engine 啟動 Logo 的事前宣告（`v8-arch-gyo2-boot-logo`）

日期：2026-11-28。Engine 任務卡（GYO2 主會話開的卡）。作者 Opus 5.5／high。
狀態：**草稿，待使用者動工前核准**（第 5 節的四個決策點）。核准前不實作。

**使用者原話**（2026-11-28）：「Engine的啟動Logo，可以默認啟用嗎？如果作者不去手動換，就會在。」

## 0. 代號對照

| 代號 | 白話 |
|---|---|
| `v8-arch-gyo2-boot-logo` | 這一批：Engine 加啟動 Logo 的 Module `boot_logo`。預設會顯示 Engine 的 Logo，作者可以換成自己的圖、換底色或關掉；Object_PvP 的開始畫面跟著調整 |

## 1. 要解決的問題（遊戲的例子）

玩家雙擊 Object_PvP，視窗一打開就是黑底的「GYO ENGINE」，兩秒後進主菜單。另一位作者用 GYO 做了一款解謎遊戲，什麼都沒設定：視窗打開也先看到同一個 Engine Logo。第三位作者在設定裡寫了自己工作室的圖和深藍底色，玩家看到的就是工作室的圖。第四位作者在設定裡把 Logo 關掉，視窗一打開就是他的第一個畫面。

現況：Engine 沒有啟動 Logo。Object_PvP 的「GYO／ENGINE」是產品自己的一個場景（`Scene::Splash`，`games/object_fps_pvp/src/client/Scenes.cpp:218-251`、`:378-387`；文件 `assets/object_fps_pvp/ui/pvp_splash.json`；停留 `Scenes.hpp:105` 的 `splashSeconds = 2.0`，按任何鍵跳過）。它是 LAN 實機場次的回饋「啟動時沒有標識」之後，使用者決定「先用文字」做的（`games/object_fps_pvp/docs/game-flow-declaration.zh-Hant.md:230`，修訂 7）。換一個產品，這頁就不在了。

## 2. 前提：Engine 要解決，還是作者能處理

- **作者今天能自己做**：Object_PvP 就是這樣做的。所以這不是「只有 Engine 做得到」的事。
- **使用者要的是預設值**：作者什麼都不做也會出現。預設只有 Engine 給得了，作者自己寫的開始畫面做不到這一點。
- 所以這一批是 §1.10 的「既定値は便利にし、作者を縛らない」：提供一個機制（啟動時在畫面最上層蓋一張圖，蓋一段時間），預設填 Engine 的圖。作者可以換，也可以關掉。不加強制顯示的守衛，也不像 Unity 那樣用授權規定一定要顯示（Unity 那條是商業規則，不是機制）。

## 3. 別家

| | 放哪裡 | 預設 | 換圖、換色、關掉 | 顯示多久 |
|---|---|---|---|---|
| Godot | 渲染的核心：`RenderingServer::set_boot_image_with_stretch` 直接清畫面、貼圖、送出（`drivers/gles3/rasterizer_gles3.cpp:478`）；呼叫的是 `main/main.cpp` 的 `setup_boot_logo`（`:3964`） | `application/boot_splash/show_image` 預設 true（`:3975`）；`image` 空字串就用 Godot 的 Logo（`:3979`、`:4016-4031`）；Godot 的 Logo 在建置時嵌進執行檔（`main/SCsub:22`、`main/main_builders.py:15`） | `image` 填 PNG 路徑（讀不到就印錯誤、改用預設的 Logo，`:3996-4002`）；`bg_color`（`:3374`，預設 `Color(0.14, 0.14, 0.14)`，`main_builders.py:14`）；`show_image` false 時換成 1×1 的透明圖，只剩底色（`:4004-4009`）；`stretch_mode` 預設 Keep（`:3977`），預設的 Logo 不縮放（`:4031`） | `minimum_display_time` 預設 0（`:4220`）：載入（同步讀主場景）多久就顯示多久，有設才在 `Main::start` 的最後等（`:4908-4929`） |
| Bevy | Engine 層沒有 splash。只在範例裡由遊戲自己做：一個 `GameState::Splash` 狀態，放 `branding/icon.png`，計時 1 秒後切到菜單（`examples/showcase/game_menu.rs:51-98`） | — | 遊戲自己的程式 | 範例是固定 1 秒 |
| Bevy 的預設組合 | `DefaultPlugins` 用 feature 決定帶哪些 Plugin：有 `bevy_render` 才帶 `RenderPlugin`、有 `bevy_ui` 才帶 `UiPlugin`（`crates/bevy_internal/src/default_plugins.rs:43`、`:72`）；作者用 `.disable::<X>()` 關掉 | — | — | — |
| Unity | 編輯器的 Player Settings（閉源，沒有原始碼可讀）；免費版強制顯示「Made with Unity」 | — | 有作者自己的 Logo 清單與背景 | 未確認（沒有原始碼，這裡不寫數字） |
| GYO 現況 | Object_PvP 的一個場景 | 只有 Object_PvP 有 | 改 `pvp_splash.json` | 文件排好之後 2 秒，按鍵跳過 |

**和 Godot 不同的地方**（影響第 4.2 節）：Godot 在顯示 Logo 之後才同步讀主場景，所以「載入期間」有幾秒，最短 0 秒也看得到 Logo。GYO 的產品在開視窗之前就讀完自己的資料（Object_PvP：`client_main.cpp:654`、`:660` 讀遊戲資料，`:539` 才 `Product::Build`）；開窗之後的資產是 `AssetStore` 在背景讀的，第一幀馬上就來。照抄「最短 0 秒」的話，Logo 只會閃幾幀，或者根本看不到，達不到使用者說的「就會在」。

## 4. 設計（推薦案；第 5 節列出要使用者決定的地方）

### 4.1 放哪一層：新的 Module `boot_logo`，放在畫圖的 World

- 例子：Object_PvP 有四個 World（Solo、Sim、Frame、Render）。`boot_logo` 只進 Render World（`ClientWorld.cpp:2299-2311`，畫到視窗的那個）。Object_Connect 只有一個 `Main` World，`units` 是空的，所以所有帶著的 Module 都會進 `Main`。
- **為什麼放在畫圖的那一側**（照 Godot：boot image 是渲染核心的功能）：
  - Logo 是「這一幀最後再蓋一層」。`boot_logo` 在 render World 的 `RenderCommands` 最後補一個 2D pass：先把整個主目標清成底色（`Pass2D::clear`，`RenderCommands.hpp:104-107`），再貼 Logo 的圖。
  - 貼圖用 `RenderTextures` 直接從 CPU 像素做（`RenderTextures.hpp:3-10`）。這樣不需要 UI 文件、字型、相機，也不需要資產的 catalog。
  - 作者的 World 照常跑，資產照常在背景讀，只是被蓋住。Logo 結束時，作者的第一個畫面已經準備好了。
- **不放在 Kernel**。也不放在 `sdl_window`、`sdl_gpu`：Plugin 是 SDL 的 adapter，不帶 Engine 的標識（§1.4），`sdl_gpu` 也只讀 render World（SDL-5）。
- **不做成 UI 文件**：如果 Logo 是作者 World 裡的 UI 根，就要相機、字型和 catalog 裡的 Engine 資產；而且 UI 的根之間不互相遮擋（`WorldUiModule.hpp:17-20`），蓋不住作者的 UI。
- 依賴：`render`、`image`、`asset`（都是程式庫）、`render_world`、`world_time`（進 World，用 `RealTime` 計時）、`world_asset`（讀 `AssetOptions::contentRoot`，給作者的圖用）。
- **「預設」怎麼做到**：見決策點 Q1。登記檔現在的規則是「預設帶的 Module 不能依賴不是預設帶的 Module」（`build/cmake/GyoUnitRegistry.cmake:170-176`），而 `render_world` 是 `default=no`，所以 `boot_logo` 照現在的規則**不能**設成預設帶。

### 4.2 什麼時候顯示、顯示多久

- **開始**：render World 第一次跑 frame schedule 的那一幀。依賴順序是：
  1. `Product::Build`：`sdl_window` 開視窗，`sdl_gpu` 建裝置和 swapchain。這時讀作者的圖並解碼，讀不到就是啟動錯誤（4.3）。
  2. 第一幀：作者的 World 照常跑。render World 照常 Extract、排作者的 pass。
  3. 在 `kSubmitStage` 之前（排在所有 2D、3D、UI 的 pass 之後），`boot_logo` 補上 Logo 的 pass。
  4. `GPUSubmit` 畫出來、送到視窗。

  不需要 UI 文件、字型或 catalog 先讀好：Logo 的圖在 `Product::Build` 時就已經在記憶體裡（Engine 的圖在執行檔裡，作者的圖在這一步讀）。
- **結束**：開始後過了 `seconds`（預設 2 秒，見決策點 Q2），就不再補 pass，放掉貼圖。
- **只顯示一次**：狀態放在 Product 的共用處（O-15，`BootLogoStatus`），替換 World 不會重來。例子：Object_PvP 換場景會替換 Frame World，以後如果連 Render World 也一起替換，Logo 也不會再出現一次。
- **Engine 不判斷「作者載入完了沒」**：資產的就緒查詢是「只告訴，等什麼、何時開始是作者的事」（`AssetView.hpp:66-67`）。Logo 也照這條：Engine 只照時間收起來，並用 `BootLogoStatus` 告訴作者 Logo 還在不在。作者要把 Logo 延到自己載完才收、要讓玩家按鍵跳過，這一批都不做（render World 沒有輸入），等有人點名再加（第 6 節）。
- **輸入不會被擋**：Logo 蓋著的時候，作者的 World 照樣收得到按鍵和滑鼠。例子：如果主菜單就在 Logo 下面，玩家在 Logo 還在的時候按一下，可能剛好點到看不見的按鈕。要不要擋是作者的事（不加守衛，§3）。作者讀 `BootLogoStatus` 決定什麼時候放出自己的畫面。Object_PvP 的做法見 Q4。

### 4.3 作者怎麼換圖、換底色、關掉

Product 層的選項 `BootLogoOptions`（放在 `ProductDescription::options`，和 `WindowSettings` 一樣），名字照 N20（Module 的類別名以 Module 結尾）和現有選項的寫法：

| 欄位 | 預設 | 例子 |
|---|---|---|
| `enabled` | `true` | 作者寫 `false`：不讀圖、不補 pass，`BootLogoStatus` 記成「沒有顯示」 |
| `image` | 空字串＝Engine 的 Logo | `"branding/studio_logo.png"`：content root 底下的路徑，照 `asset` 的 `ContentPath` 規則檢查；只收 PNG（`image` 的 `PngLoader` 解碼，和 Godot 一樣只收 PNG） |
| `background` | 黑 `#000000FF`（和現在的 PvP 開始畫面一樣；Godot 是深灰） | `{20, 30, 60, 255}` |
| `seconds` | 2.0（見 Q2） | `1.0` |

- **怎麼縮放**：圖保持比例，整張放進視窗，置中（Godot 自訂圖的預設 Keep）。其他縮放方式現在沒有人要，不加。Engine 的圖會畫成 16:9、四周留透明的邊，所以放滿視窗時大小剛好。
- **作者的圖讀不到或不是 PNG**：當成啟動錯誤回報，`Product::Build` 失敗，訊息寫路徑和原因。Godot 是印錯誤之後改用預設的 Logo；那等於 Engine 替作者決定了怎麼處理（§1.9「誤りは何が起きたかを伝えるだけ」），所以不照抄。
- **建置時拿掉**：產品用 `gyo_product_units(... DISABLE_MODULES boot_logo)`，連程式都不帶（如果 Q1 選甲）。
- `BootLogoStatus`（Product 的共用處）：`Enabled()`、`Showing()`、`Finished()`、`FromAuthor()`（顯示的是作者的圖還是 Engine 的）、圖的寬高。給作者讀，也給測試從外面確認。

### 4.4 Engine 自帶的 Logo 圖檔與授權

- repo 裡現在**沒有** GYO 的 Logo 圖（`git ls-files` 查過：只有 `pvp_splash.json` 的文字「GYO」「ENGINE」）。
- 放哪裡：`engine/modules/boot_logo/assets/gyo_boot_logo.png`（原圖進 git）。建置時把它的位元組嵌進執行檔（同 Godot 的 `boot_splash_png`；GYO 的先例是 `sdl_gpu` 的 `EmbeddedShaders`，`plugins/sdl_gpu/CMakeLists.txt:92`）。這樣不依賴作者的 content root 和 catalog，作者什麼都沒準備也會顯示。
- 授權寫在：`engine/modules/boot_logo/assets/LICENSE.zh-Hant.md`（作者、取得方式、授權），格式照 `assets/object_fps_pvp/LICENSES/SOURCES.zh-Hant.md`。
- 圖本身要使用者決定（Q3）。

### 4.5 和 Object_PvP 自己的開始畫面怎麼排

見 Q4。推薦案：Object_PvP 的開始畫面本來就是 Engine 的標識（「GYO」「ENGINE」），改由 Engine 的 Logo 取代。`Splash` 場景留下，但不再放文件，只在 Logo 下面等；`BootLogoStatus::Finished()`（或 `enabled` 為 false）之後才進主菜單，所以玩家在 Logo 期間按鍵不會點到菜單。`pvp_splash.json` 和 `splashSeconds` 刪掉；`--connect` 直接進遊戲時一樣會蓋 Logo。

## 5. 要使用者決定的事

**Q1. 「預設就有」用哪一種做法？**（影響 O-6 和登記檔的規則，所以要問）

| | 做法 | 作者什麼都不做時 | 代價 |
|---|---|---|---|
| **甲（推薦）** | 獨立的 Module `boot_logo`。登記檔的 `default` 欄加一個值，意思是「它依賴的 Module 都帶了，就預設帶」（照 Bevy `DefaultPlugins` 的 feature 條件）。帶了 `render_world` 的產品就會帶 `boot_logo`，不想要就用 `DISABLE_MODULES` | `units` 空的 World（Object_Connect）自動就有；`units` 有列名單的 World（Object_PvP 的 Render World）要加一行 `"boot_logo"`，和它現在自己列 `window`、`input` 一樣 | 要改登記檔的規則和 `cmake.UnitRegistry` 的測試（O-6 的條文跟著改） |
| 乙 | 做進 `render_world`：凡是會畫圖的 World 都有，不用列；關掉只能靠選項 | 每個畫圖的產品都有，連列都不用列 | `render_world` 多一份和它本來職責無關的工作；現有的 render、`sdl_gpu` 測試（`"render_world"` 在 repo 出現 30 次）前 2 秒都會多一個 pass，要逐一關掉或改期待 |
| 丙 | 獨立 Module，`default=no`，作者自己列 | 沒列就沒有 | 不符合「作者不去換就會在」 |

**Q2. 顯示多久？**
- **甲（推薦）**：最短 2 秒（`seconds` 預設 2.0，和現在的 Object_PvP 一樣），不能跳過。理由見第 3 節最後一段：GYO 開窗之後沒有同步載入，照 Godot 的 0 秒就看不到。
- 乙：照 Godot，預設 0 秒，也就是只顯示第一幀。不會拖慢啟動，但等於看不到。
- 丙：其他秒數。

**Q3. Engine 的 Logo 圖？**
- **甲（推薦）**：先做一張佔位圖：用 repo 裡已有的 BIZ UDPGothic（OFL 1.1，`assets/object_fps_pvp/LICENSES/BIZUDPGothic-OFL.txt`）排出和現在 PvP 開始畫面一樣的白字「GYO」、灰字「ENGINE」，存成 1920×1080 的透明 PNG，產生圖的腳本也進 repo。之後使用者有正式的圖，只換這一個檔。用字型畫出來的圖不是字型本身，不受 OFL 約束，但出處照樣寫進授權檔。
- 乙：使用者提供正式的圖（請告訴我檔案在哪裡、授權是什麼），拿到之前不動工。

**Q4. 和 Object_PvP 的開始畫面怎麼排？**
- **甲（推薦）**：Engine 的 Logo 取代 PvP 的開始畫面（4.5）。理由：PvP 那頁的內容就是 Engine 的標識。代價：現在可以按鍵跳過，改成固定 2 秒不能跳過。
- 乙：先 Engine 的 Logo，再 PvP 自己的 Logo（PvP 的頁改成產品名「Object_FPS_PVP」，仍然可以跳過）。啟動時共看到兩頁，約 4 秒。
- 丙：PvP 用 `BootLogoOptions::image` 把 Engine 的 Logo 換成自己的圖。PvP 就不會出現 Engine 的 Logo，這和使用者要的「Engine 的 Logo 會在」相反，只列出來比較。

## 6. 範圍

- 做：`engine/modules/boot_logo`（Module、選項、狀態、Engine 的圖和授權、嵌入）；登記檔一列（照 Q1）；Object_PvP 的調整（照 Q4）；Module 的頁 `boot-logo.zh-Hant.md`（Design Invariants 和 Replacement Criteria，合併前核准定稿）；`target/v2/README.md` 加一列；dev_log。
- 不做（有人點名再加）：按鍵跳過；延到作者載完才收；淡入淡出；多張 Logo 依序播；Godot 那些縮放方式（Cover、Ignore 等）；直接收 CPU 像素當作者的圖（現在只收路徑）。
- 不碰：Kernel、`sdl_window`、`sdl_gpu`、`ui`、`world_ui`。
- 不改：HANDOFF、批次表（這張卡不改；使用者的決定照四樣格式轉給主會話）。

## 7. 測試（從外面確認）

| 項 | 怎麼測 | 哪裡跑 |
|---|---|---|
| 預設會顯示 | render World 帶 `boot_logo`，不給選項：第一幀 `RenderCommands` 的最後一個 pass 是 Logo 的（清成黑、目標 0、貼圖是 Engine 的圖），貼圖的寬高和像素的 digest 等於嵌入的 PNG 解出來的；`BootLogoStatus::Showing()` 為 true、`FromAuthor()` 為 false | CI 三平台（cpu） |
| 換圖後顯示作者的圖 | `image` 指向測試用的 PNG、`background` 換色：貼圖的像素等於那張 PNG，清成作者的底色；`FromAuthor()` 為 true | CI 三平台（cpu） |
| 關掉後不顯示 | `enabled = false`：每一幀都沒有 Logo 的 pass、沒有建貼圖；`Showing()` 從頭到尾都是 false | CI 三平台（cpu） |
| 時間到就收 | 推 `RealTime`：未滿 `seconds` 時有 pass，滿了之後沒有，貼圖放掉；替換 World 之後不會再出現 | CI 三平台（cpu） |
| 作者的圖讀不到 | 路徑不存在、檔案不是 PNG：`Product::Build` 失敗，訊息有路徑 | CI 三平台（cpu） |
| 預設帶（Q1 甲） | `cmake.UnitRegistry`：帶 `render_world` 的產品就帶 `boot_logo`，沒帶 `render_world` 就不帶，`DISABLE_MODULES` 可以拿掉 | CI 三平台 |
| 真的畫到視窗 | `sdl_gpu` 的顯示視窗：Logo 期間的幀照常送出（`FramesShown` 增加） | Linux（Lavapipe＋Xvfb）、本機 |
| PvP（Q4 甲） | Logo 期間 `Splash` 不換到 `Menu`，Logo 結束之後才換；`enabled = false` 時立刻換 | CI 三平台（cpu） |

**實機**（交使用者看）：Object_PvP 啟動時看到 Engine 的 Logo，2 秒後進主菜單；全屏時 Logo 置中、比例正確。

## 8. 判定（完成條件）

1. 本機 `ctest -L "cpu|gpu"` 全過（本機離線造成的除外，寫明）。
2. 相關的突變全部 killed：新邏輯（預設的圖、換圖、關掉、時間到就收、只顯示一次、讀不到是錯誤、pass 排在最後、Q1 甲的條件式預設帶）各至少一個。
3. 設計審查（Opus／high）的 blocker 處理完；純淨 Code Review（Fable／high）。
4. draft PR（目標 `gyo2`）的 L1 三平台綠（手動 dispatch）。

## 9. 模型與檔位（navigator、effort-gearbox）

- **上段**：Q1 甲會改登記檔的規則（O-6，編排層的 Design Invariant）；新的 Module 也有公開介面（選項、狀態）。
- 實作者：Opus／high。設計審查：Opus／high（和作者同模型，報告會寫明）。純淨 Code Review：Fable／high。
- 突變與 L1 的執行、結果記錄：Opus／medium（上段不用小的模型）。
