# unit 的組成：選擇、建置、連結、註冊進 World（研究，`v8-arch-gyo2-unit-composition-research`）

研究卡，2026-11-27。只研究、只改文件；交出決策點，不寫程式。起因是 #366 的 U6（HANDOFF `v8-arch-gyo2-ink-spike-u6-unit-research`）。這是上段的工作：碰到 Architecture、建置層與編排層的介面，可能推翻現行設計，所以每一題都只給選項與推薦，**由使用者決定**。使用者的決定由主會話記，這份不寫「使用者的決定」一節。

程式的現況照 `origin/gyo2` a66c4135；ink 的列照 #366 的分支（`claude/gyo2-ink-spike-9fa94f`）；模式配置照 #367 的分支（`claude/gyo2-mode-config-declaration` eee820b0）。參考實作的源碼在 `~/Code/GitHome`：Bevy 0.20.0-dev（6a911653f）、Godot 4.8.0 beta（94fc09c）與 godot-cpp（7fd5c4b）、`com.unity.entities` 1.5.0（4630ff5）與 `com.unity.netcode` 7.2.0。

## 0. 前提、用詞、結論先講

### 0.1 已定的前提（這份研究不推翻它們）

| 前提 | 出處 |
|---|---|
| Engine 只給機制，替換得掉的規則由作者選；Kernel 的職責是 ECS；依賴單向；外部組件只出現在 adapter 裡；名字是概念的 tag | AGENTS §1 |
| Module＝我們自己維護、介面是我們的；Plugin＝第三方官方維護的組件的 adapter。Module 不依賴 Plugin | `v8-arch-gyo2-module-plugin-owner-definition`、`v8-arch-gyo2-module-plugin-registers` |
| 兩份登記檔（`config/modules.csv`、`plugins.csv`）是「Engine 有什麼、依賴是什麼」唯一的地方；程式碼不寫 `Requires()`；編排層在 build 時檢查依賴 | `v8-arch-gyo2-module-deps-in-registers`、O-6 |
| 產品可以只選部分 Module（單機不必帶權威） | `v8-arch-gyo2-products-pick-modules` |
| 沒人選的 Plugin 不 configure、它抓的東西不下載（SDL-2） | `v8-arch-gyo2-sdl-p`；`sdl.zh-Hant.md` SDL-2 |
| Module 進 World 和 Plugin 進 World 在程式上沒有差別 | `v8-arch-gyo2-plugin-in-world-rule-removed` |
| 模式配置：玩法組固定登記，跑不跑由模式資料（run condition）決定；模式不決定 Engine 的 Module | #367 第 2.3 節 (R1)、第 2.1 節表的第 4 列 |
| 使用者的原則：**不開特例**（ink 怎麼接，sdl 也要一樣）；Bevy 不分 Module／Plugin、GYO 分，不能整套照搬 | `v8-arch-gyo2-ink-spike-u6-unit-research` |

### 0.2 用詞

使用者的題目是「選擇、建置、連結、註冊」四件事。照程式看，「註冊」其實是兩件，所以這份把它拆成五件：

| 件 | 意思 | 例 |
|---|---|---|
| (1) 選擇 | 這次 configure 要不要加這個目錄 | 不選 `sdl_window` 就不抓 SDL |
| (2) 建置 | 編出它的 target | `gyo_sdl_window` |
| (3) 連結 | 誰 link 它、用它的型別與函式 | 遊戲 link `GYO::Ink` |
| (4) 帶上 | Product 有它的**實例**：跑它的 `Configure`，之後可以放進 World | `std::make_unique<SDLWindowPlugin>()` |
| (5) 放進 World | 某個 World 跑它的 `Build`／`Finish`／`Check` | `WorldDescription::units` 列 `"sdl_window"` |

另外還有第 (6) 件、不屬於本題但要分清楚：**跑不跑**——登記了的 System 這一刻跑不跑（run condition，#367）。

### 0.3 結論（一段話）

使用者覺得怪的第 1 點成立，第 2 點**一半成立**：「放進哪個 World」(5) 今天已經是執行期、遊戲程式碼的事（`WorldDescription::units`），綁在建置層的是 (4)「帶上」——實例由 CMake 產生的程式建立。真正的問題是 **(1)～(4) 塞在同一個 CMake 呼叫裡**，而且同一件事（這個產品用哪些單位）**寫在兩個地方**：CMake 的 `gyo_product_units` 與程式碼的 `units`，要靠人手對齊。推薦 **(C)**：CMake 只負責「用」（(1)～(3)，對所有 Plugin 一樣，不分 `library_only`），程式碼負責「帶上」與「放進 World」（(4)、(5)，同 Bevy 的 `add_plugins`），登記檔照舊是依賴的唯一來源、編排層照舊檢查。執行期的 API 今天就收得下手寫的單位（測試一直這樣用），改動集中在建置層的產生器與各產品的 `main`。遷移的第 1 步（「用」的宣告）可以單獨先做，U6 在那一步就解掉。

---

## 1. 現況（照程式）

### 1.1 五件事今天各在哪

| | Module（`library_only=yes`，例 `json`、`model`） | Module（有單位，例 `transform`、`net`） | Plugin（`library_only=yes`，例 `sdl`、`ink`） | Plugin（有單位，例 `sdl_window`） |
|---|---|---|---|---|
| (1) 選擇 | 一律加（`engine/CMakeLists.txt:20-27` 照 `modules.csv`） | 同左 | **只能由別的 Plugin 的 `depends` 帶進，或建的人用 `GYO_PLUGINS`**（`GyoBuild.cmake:13-14`、`:111-119`；`GyoUnitRegistry.cmake:254-297`） | 遊戲的 `gyo_product_units(PLUGINS …)` 記下要求（`GyoUnitRegistry.cmake:369-374`），或 `GYO_PLUGINS` |
| (2) 建置 | 一律 | 一律 | 被選才建 | 被選才建 |
| (3) 連結 | 遊戲自己 `target_link_libraries`（例 `games/object_fps_pvp/CMakeLists.txt:33-34`、`:53-54`） | 遊戲自己 link＋`gyo_product_units` 也 link 單位的 target（`GyoUnitRegistry.cmake:414`、`:427`） | 遊戲自己 link（例 #366 `plugins/ink/tests/CMakeLists.txt`），但前提是 (1) 有人選了 | `gyo_product_units` link |
| (4) 帶上 | —（沒有單位） | 產生的 `MakeRegisteredUnits()`：`default=yes` 減 `DISABLE_MODULES` 加 `MODULES`（`GyoUnitRegistry.cmake:189-244`、`:404-415`） | —（**`gyo_product_units` 擋下**，`GyoUnitRegistry.cmake:203`） | 產生的 `MakeRegisteredUnits()`，`PLUGINS` 列出的 |
| (5) 放進 World | — | 遊戲程式碼 `WorldDescription::units`（`Product.hpp:39-40`；空＝全部帶上的，`Product.cpp:209-213`） | — | 同左 |

執行期這一側：`Product::Build(ProductDescription, RegisteredUnits)`（`Product.hpp:143-144`）。`RegisteredUnits` 是「兩份登記檔的全部列＋帶上的實例」（`RegisteredUnits.hpp:40-46`），註解寫明「tests write it by hand」（`:13-14`）。`Product::Build` 對手寫的表照樣檢查：一列兩個實例（O-E1）、實例指到不存在或 `library_only` 的列（O-E2）、World 用了沒帶上的單位（O-E6）、依賴缺或排在後面（O-E3）（`Product.cpp:132-246`）。實際上手寫的有十幾處測試，例 `tests/common/camera/CameraTests.cpp:82-92`、`tests/common/authority/AuthorityTestSupport.hpp:87-93`。

### 1.2 使用者覺得怪的兩點，照程式核對

**第 1 點「同一個動作兩種意思」：成立，而且不只 ink。**

- `gyo_product_units(PLUGINS x)` 同時做 (1) 記下要選 x、(3) link x 的 target、(4) 產生 x 的實例。`library_only` 的 Plugin 沒有 (4)，所以整個被擋（`GyoUnitRegistry.cmake:203`），連 (1)、(3) 也拿不到。
- 結果：遊戲沒有地方宣告「我要用 ink」。只剩建的人的 `GYO_PLUGINS`（CI 用 `-DGYO_PLUGINS=ALL`，`.github/workflows/cross-platform.yml:205`），那不是遊戲的依賴宣告。
- SDL 宣告第 3.3 節列的 **`imgui` 也是 `library_only` 的 Plugin**（`sdl-declaration.zh-Hant.md:451`），它回來時會碰到同一個洞。所以這不是 ink 的特例，是「沒有單位的 Plugin」這一類都有的問題。
- Module 這邊沒有這個洞，只因為 Module 一律建；`library_only` 的 Module 也是遊戲自己 link。換句話說，今天 Module 與 Plugin 的 (1)～(3) 走兩條不同的路，`library_only` 又讓 Plugin 內部再分兩條。

**第 2 點「World 的組成綁在建置層」：一半成立。**

- 不成立的一半：**哪個 World 放哪些單位，今天已經是執行期、遊戲程式碼決定的**。例：PvP 的 Client 有 Frame 與 Render 兩個 World，各自列 `units`（`games/object_fps_pvp/src/client/ClientWorld.cpp:2230-2232`、`:2280-2286`）；Match 的 World 也各自列（`games/object_fps_pvp/src/world/PvpWorld.cpp:1218`、`:1245`）。
- 成立的一半：**帶上哪些（實例從哪裡來）由 CMake 決定**。實例在 CMake 產生的 `<target>_units.cpp` 裡 `std::make_unique`（`GyoUnitRegistry.cmake:404-415`、`:418-426`），遊戲的程式碼改不了。
- **真正的毛病是同一件事寫兩處**：PvP 的 Client 在 CMake 寫 `MODULES … PLUGINS sdl_window sdl_audio sdl_gpu`（`games/object_fps_pvp/CMakeLists.txt:89-91`），在程式碼又寫 `"sdl_window"`、`"sdl_gpu"`（`ClientWorld.cpp:2232`、`:2286`）。兩處不一致時，CMake 不會報，執行時才是 O-E6。測試的產品不帶 `sdl_window`／`sdl_gpu`（`tests/object_fps_pvp/CMakeLists.txt:69-70`），所以程式碼得多一個 `bool window`／`bool gpu` 參數來配合（`ClientWorld.hpp:148-149`；呼叫 `tests/object_fps_pvp/client/ClientTests.cpp:213`、`:235`）。這個參數存在的理由，就是 CMake 那一份清單。

### 1.3 第三件相關的事：`depends` 一欄兩種意思（已知的限界 N7）

`depends` 同時是 (3) 的 link 依賴與 (5) 的「同一 World 要先有這個單位」（`orchestration-skeleton-declaration.zh-Hant.md:126`）。為了繞開它，SDL 宣告第 3.3 節規定「每一列只扮一種角色」（函式庫／來源側／渲染側／只做 `Configure`），於是 `ui` 與 `world_ui`、`render` 與 `render_world`、`world_render` 拆成不同列（`sdl-declaration.zh-Hant.md:152-162`）。這和本題的關係在第 4 節；它本身是不是要改，列在決策點 P7。

---

## 2. 現行設計的由來（第 3 題）

| 什麼時候、哪一條 | 定了什麼 | 當時的理由 | 現在還成立嗎 |
|---|---|---|---|
| 2026-11-14 `v8-arch-gyo2-module-plugin-registers`（HANDOFF:54） | 兩份登記檔放建置層、只有一處；CMake 照它 link；「執行時的 Registry 只收進 World 的登記單位，**由清單產生，不另外手寫**」 | 卡寫的：(a) 純函式庫能不能用由建置決定，執行時擋不住；(b) 建置與 Registry 各寫一份違反 §9；(c) Registry 的指紋算 World 的內容，不進 World 的不該影響它；(d) 使用者要可見性與依賴明示 | (a) 成立：支持 (1)～(3) 留在建置層，**沒說到 (4)、(5)**。(b) 說的是**登記檔的列**（名字、種類、依賴）不要在程式裡再寫一份；所有選項都保留這點。(c)、(d) 不受任何選項影響 |
| 2026-11-17 `v8-arch-gyo2-module-deps-in-registers`（HANDOFF:143） | 依賴只寫在登記檔；編排層在 build 時檢查；不寫 `Requires()` | 同一事實一處；Bevy 沒有 Plugin 依賴機制（issue #69） | 成立；所有選項都保留 |
| 2026-11-17 `v8-arch-gyo2-products-pick-modules`（HANDOFF:155） | 產品可以只選部分 Module | 「要不然單機遊戲也必須上權威了」 | 成立；只是「在哪裡選」不同 |
| 編排層骨架宣告 3.2（`orchestration-skeleton-declaration.zh-Hant.md:91-131`） | `gyo_product_units` 產生實例；產品選什麼寫在產品自己的 `CMakeLists.txt`（`:117`）；**退掉「產品 `main` 手寫清單」**（`:130`） | 退掉的理由寫的是「11-14『由清單產生，不另外手寫』」；靜態自我登記另外退掉（靜態初始化順序、linker 丟物件檔） | 靜態自我登記的理由成立，沒有選項要它。「`main` 手寫」的理由**要看 11-14 那句怎麼讀**：讀成「列不要手寫」（11-14 卡的理由 (b) 是這個意思），手寫的是實例、列照舊來自登記檔，就不衝突；讀成「產品選什麼也要由清單產生」，(C) 就要推翻它。原話只有卡片的轉述，**哪一個是使用者的意思未確認**——決策點 P3 |
| 2026-11-23 `v8-arch-gyo2-sdl-p`（`sdl.zh-Hant.md` SDL-2） | 只建被選的 Plugin；`GYO_PLUGINS` 給建的人；`library_only` 的 `sdl` 沿依賴帶進 | 不用 SDL 的產品不下載、不建 SDL | 成立。(1) 一定要在 configure 時決定，所以「選擇」必須留在建置層。當時只設想了「有單位的 Plugin 帶著 `library_only` 的 Plugin」，**沒有設想遊戲直接用沒有單位的 Plugin**——這是 U6 的洞 |
| 2026-11-23 `v8-arch-gyo2-plugin-in-world-rule-removed`（HANDOFF:306） | Module／Plugin 進 World 沒有程式上的差別 | 去掉多餘的轉發 | 成立；它也表示「Module 與 Plugin 走不同的註冊路」沒有程式上的理由 |

**小結**：把 (4) 綁在建置層，當時的理由是 C++ 沒有反射、要有人寫 `make_unique`，而卡片選了「產生」而不是「手寫」，根據是 11-14 那句話。理由 (a)～(d) 都沒有要求 (4) 在建置層；它們要求的是「列（名字、種類、依賴）只在登記檔」與「能不能用由建置決定」。這兩點在推薦的 (C) 裡都不變。

---

## 3. 別家怎麼做（第 2 題）

### 3.1 Bevy（主參考）

- **(1)(2) 選擇、建置：Cargo feature，建的人或遊戲的 `Cargo.toml` 決定。** `default = ["2d", "3d", "ui", "audio"]`（`Cargo.toml:139`）；葉子 feature 只往 `bevy_internal` 轉（例 `bevy_audio = ["bevy_internal/bevy_audio"]`，`Cargo.toml:302`）；`bevy_internal` 用 `dep:` 開可選的 crate（`crates/bevy_internal/Cargo.toml:354`、`:575`）。`bevy_math`、`bevy_ecs`、`bevy_transform` 等是不可選的依賴，一律編（`crates/bevy_internal/Cargo.toml:531`、`:537`、`:554`）。
- **(3) 連結：** 遊戲對 `bevy` 一個 crate 的依賴；可選的部分由 `#[cfg(feature)]` 決定有沒有 re-export（`crates/bevy_internal/src/lib.rs:29-30`；`bevy_math` 無條件，`:74`）。
- **(4)(5) 帶上、放進 World：遊戲程式碼 `app.add_plugins(...)`。** `DefaultPlugins` 是 `plugin_group!`（`crates/bevy_internal/src/default_plugins.rs:3-114`），每一項帶 `#[cfg(feature)]`（例 `AudioPlugin`，`:80-81`）：**feature 決定這一項在不在群組的程式碼裡，遊戲決定要不要 `add_plugins` 它**。`PluginGroupBuilder::disable`／`set`（`crates/bevy_app/src/plugin_group.rs:551-558`、`:362-369`）讓遊戲關掉或換掉群組裡的一項（例 `examples/app/without_winit.rs:7`）。`App::add_boxed_plugin` 立刻呼叫 `build`（`crates/bevy_app/src/app.rs:524-567`）。
- **純函式庫 vs 會註冊的單位：看有沒有 `impl Plugin`，不另外標。** `bevy_math`、`bevy_utils`、`bevy_tasks`、`bevy_reflect`、`bevy_ecs` 等沒有任何 `impl Plugin`（`bevy_math` 0 處），也不依賴 `bevy_app`。`bevy_transform` 兩者都是：`Transform` 是純資料（`crates/bevy_transform/src/components/transform.rs:86`），`TransformPlugin` 在 feature `bevy-support` 後面（`crates/bevy_transform/src/lib.rs:24-25`、`Cargo.toml:49`）。**一個 crate 可以有零個或多個 Plugin**（`bevy_render` 25 個）。
- **依賴：沒有正式機制。** `bevy_dev_tools/src/fps_overlay.rs:70` 的 TODO 指 issue #69；各 Plugin 自己 `is_plugin_added` 補（`crates/bevy_sprite/src/lib.rs:68-70`），或缺 Resource 時 panic（`crates/bevy_render/src/lib.rs:427`）。
- **不同 World 不同組成：SubApp。** `SubApp` 有自己的 `World` 與 extract 函式（`crates/bevy_app/src/sub_app.rs:66-82`、`:19`）；`RenderPlugin::build` 用 `ExtractPlugin` 建 `RenderApp`（`crates/bevy_render/src/lib.rs:396-402`；`crates/bevy_extract/src/extract_plugin.rs:57-96`），再 `get_sub_app_mut(RenderApp)` 往裡面加（`crates/bevy_render/src/lib.rs:430-474`）。一個 Plugin 可以同時寫兩個 World（`SpriteRenderPlugin`，`crates/bevy_sprite_render/src/lib.rs:75-107`）。**決定誰進哪個 World 的是 Plugin 自己的程式碼**，不是遊戲。
- **跑不跑：** `run_if`（`crates/bevy_ecs/src/schedule/config.rs:542`）、`in_state`（`crates/bevy_state/src/condition.rs:103-106`）。和 `add_plugins` 分開。
- **動態載入：** `bevy_dynamic_plugin` 因 unsound 已移除（commit d722fef23）。

### 3.2 Godot

- **engine modules（`modules/*`）：(1)～(4) 全由建 engine 的人決定，產生註冊程式。** 有 `register_types.h`、`SCsub`、`config.py` 的目錄才是 module（`methods.py:301-308`）；每個 module 有 `module_<name>_enabled` 選項（`SConstruct:487-500`），之後 `can_build`、依賴檢查（`SConstruct:1132-1162`；`methods.py:323`、`:336-360`）。`modules/modules_builders.py:15-51` 產生 `register_module_types.gen.cpp`，對每個啟用的 module 呼叫 `initialize_<name>_module(level)`。**這和 GYO 今天的 `MakeRegisteredUnits()` 是同一種形。**
- **純函式庫 vs 會註冊的單位：不分，函式庫也是 module，註冊函式是空的。** `modules/freetype/register_types.h:35-36` 的 `initialize_freetype_module` 是空函式，`modules/freetype/SCsub` 只建 `thirdparty/freetype/`；`text_server_adv` 用 `#ifdef MODULE_FREETYPE_ENABLED` 與 `env.module_list` 判斷它在不在（`modules/text_server_adv/text_server_adv.h:74`、`SCsub:15`）。`thirdparty/` 本身沒有註冊（例 `core/SCsub:39`、`:131`、`:165` 直接編進 core）。**Godot 用「一律有註冊函式，可以是空的」來避免兩種東西走兩條路。**
- **GDExtension：專案決定、執行期載入。** `.gdextension` 檔的解析在 `core/extension/gdextension_library_loader.cpp:279` 起（`entry_symbol`、`compatibility_minimum`、各平台的 `[libraries]`）；編輯器掃專案裡的 `.gdextension`、寫 `res://.godot/extension_list.cfg`（`editor/file_system/editor_file_system.cpp:3699-3704`；`core/extension/gdextension_manager.cpp:372-407`），執行期讀它載入（`gdextension_manager.cpp:329-344`）。初始化等級與 module 相同（`modules/register_module_types.h:34-43`；godot-cpp `include/godot_cpp/godot.hpp:68-74`、`:112-135`）。
- **註冊不等於「在場景裡」。** module／extension 只讓型別進 `ClassDB`（`core/object/class_db.h:237-250`）；真正在執行中的東西由資料決定：主場景（`main/main.cpp:4380-4391`、`:4805-4813`）、`project.godot` 的 autoload（`core/config/project_settings.cpp:351-375`；`main/main.cpp:4543-4611`）。
- **build profile：** `build_profile` JSON 停用類別（`SConstruct:294`、`:660-679`；`core/object/class_db.h:42-58`、`:565-584`），編輯器可以「從專案偵測」產生它（`editor/settings/editor_build_profile.cpp:900`）。

### 3.3 Unity Entities／Netcode for Entities

- **(1)～(3)：專案的 package 與 assembly（`.asmdef`）。** `com.unity.entities/package.json:6-21` 的 `dependencies`；`Unity.Entities.asmdef:4-13` 的 `references`；條件編譯的 assembly 例：`Unity.NetCode.Physics.asmdef` 只在裝了 `com.unity.physics` 時定義 `ENABLE_UNITY_NETCODE_PHYSICS`、才編（`com.unity.netcode/Runtime/Physics/Unity.NetCode.Physics.asmdef:23-32`）。（本機沒有遊戲專案的 `Packages/manifest.json`，「專案宣告 package」這一側沒有源碼可引。）
- **(4)(5)：編進來的 System 一律自動加入**，反射或 IL post-processor 找出所有 `ISystem`／`ComponentSystemBase`（`Unity.Entities.CodeGen/StaticTypeRegistry/StaticTypeRegistryPostProcessor.cs:301-319`；`Unity.Entities/Types/TypeManager.cs:3599-3616`），`DefaultWorldInitialization.Initialize` 全部加進預設 World（`Unity.Entities/DefaultWorldInitialization.cs:122-158`、`:241-302`）。也就是**建置決定註冊**，比 GYO 今天更徹底。
- **退出與自訂：** `[DisableAutoCreation]`（可以標在整個 assembly，`Unity.Entities/WorldSystemFiltering.cs:36-39`；檢查在 `TypeManagerSystems.cs:1291-1304`）、`UNITY_DISABLE_AUTOMATIC_SYSTEM_BOOTSTRAP`（`Unity.Entities.Hybrid/Injection/AutomaticWorldBootstrap.cs:1-16`）、`ICustomBootstrap`（`Unity.Entities/World.cs:90-99`；找法 `DefaultWorldInitialization.cs:400-432`）。
- **不同 World 不同組成：System 自己標旗標，bootstrap 依旗標挑。** `WorldSystemFilterFlags`（`WorldSystemFiltering.cs:47-116`）；Netcode 的 `ClientServerBootstrap : ICustomBootstrap`（`com.unity.netcode/Runtime/ClientServerWorld/ClientServerBootstrap.cs:31`）建 Server World 用 `ServerSimulation`（`:446`）、Client World 用 `ClientSimulation | Presentation`（`:341`）。**決定「誰進哪個 World」的是 System 自己的屬性＋遊戲的 bootstrap。**
- **純函式庫 vs 會註冊的單位：看 assembly 有沒有 System 型別。** System 的基底在 `Unity.Entities` 裡，不引用它的 assembly 不可能有 System；IL post-processor 只處理引用它的 assembly（`Unity.Entities.CodeGen/EntitiesILPostProcessor.cs:143-174`）。例：`Unity.Mathematics.Extensions.asmdef` 不引用 Entities、沒有 System；`Unity.Transforms` 有 `TransformSystemGroup`、`LocalToWorldSystem`（`Unity.Transforms/EndFrameTransformSystems.cs:12-19`、`LocalToWorldSystem.cs:27-31`）。
- **跑不跑：** `Enabled`、`RequireForUpdate`（`Unity.Entities/SystemState.cs:180`、`:1089-1120`；判斷 `ShouldRunSystem` `:666-701`）：System 在 World 裡，資料不在就不跑。

### 3.4 對照

| | 選擇、建置、連結 | 帶上、放進 World | 純函式庫怎麼分 | 不同 World 不同組成 | 跑不跑 |
|---|---|---|---|---|---|
| Bevy | 遊戲的 `Cargo.toml`／feature（建置宣告） | **遊戲程式碼** `add_plugins` | 有沒有 `impl Plugin`；一個 crate 可以有多個 Plugin | Plugin 自己往 SubApp 加 | `run_if` |
| Godot modules | 建 engine 的人（SCons 選項） | **產生的程式**，一律初始化 | 不分；空的註冊函式 | （一棵場景樹；不適用） | 場景資料 |
| Godot GDExtension | 專案的檔案 | 執行期載入，一律初始化 | 同上 | 同上 | 同上 |
| Unity | 專案的 package、asmdef | **反射**，一律加入；可退出 | 有沒有 System 型別 | System 的旗標＋bootstrap | `RequireForUpdate` |
| GYO 現在 | Module 一律；Plugin 由 `gyo_product_units` 或 `GYO_PLUGINS`；`library_only` Plugin 遊戲宣告不了 | **產生的程式**（CMake 的清單）＋程式碼 `units` 挑 | 登記檔的 `library_only` 欄；一列最多一個單位 | 遊戲程式碼 `units` | run condition |

讀法：

1. **三家都不會讓「沒有註冊的單位」走另一條宣告的路。** Bevy、Unity 的「用不用」只有一種宣告（依賴），有沒有註冊是程式碼本身的事；Godot 讓每個 module 都有註冊函式（可以是空的）。GYO 的 `gyo_product_units` 把「用」和「註冊」綁在一起，於是沒有註冊的 Plugin 就沒有「用」的路。**這是第 1 點的根。**
2. **「建置決定註冊」本身不怪，Godot modules 與 Unity 都這樣。** 但它們都是「編進來就註冊、一律」，沒有第二份清單；Unity 的不同 World 靠 System 自己的旗標，也不靠遊戲另外列。GYO 是「建置產生一份、程式碼再挑一份」，兩份要對齊。**這是第 2 點裡成立的那一半的根。**
3. **Module／Plugin 的區分在三家都沒有對應**（Bevy 不分；Godot 的 module／GDExtension 分的是「編進 engine／專案載入」；Unity 分的是內建 module／registry package）。GYO 的區分是 owner 與依賴方向，再加上 SDL-2 的「Plugin 沒選不下載」。這些都只影響 (1)：**Module 一律建、Plugin 要被選**。到 (3)～(5)，Module 與 Plugin 沒有程式上的差別（`v8-arch-gyo2-plugin-in-world-rule-removed`）。所以「不開特例」落到設計上就是：**差別只留在 (1)，(3)～(5) 一條路**。
4. 照 Bevy 的部分與偏離（批次表〈每批的設計審查〉第 5 項）：
   - 照：(4)(5) 由遊戲程式碼決定；預設群組可關（`DefaultPlugins`＋`disable`）；純函式庫不必有註冊。
   - 偏離 1：**依賴有正式的表與檢查**（Bevy 沒有，issue #69）。衝突點：Bevy 的 `add_plugins` 是型別，沒有名字；GYO 的檢查要把實例對到登記檔的列。膠水：一個「實例↔列」的配對（今天的 `UnitEntry{row, unit}` 就是；或產生的型別對照，見 P5）。
   - 偏離 2：**一列最多一個單位**（Bevy 一個 crate 可以有多個 Plugin）。這次不動（第 1.3 節、P7）。
   - 偏離 3：**Plugin 的「選擇」在 configure 時、由遊戲或建的人宣告**（Bevy 的 feature 也是建置時，但 Cargo 自己解依賴；GYO 用 CMake，要自己收集要求）。膠水：今天已有的 `GYO_PLUGINS_REQUESTED`＋`gyo_plugin_selection`。

---

## 4. 五件事各該在哪一層、由誰決定（第 1 題）

| 件 | 前提 | 推薦的層與決定的人 | 理由 |
|---|---|---|---|
| (1) 選擇 | **Engine 要解決**（configure 時決定加不加目錄、抓不抓外部組件，只有建置層做得到） | 建置層。Module：一律（Engine 自己的）。Plugin：**遊戲的建置宣告**（主要）＋建的人（`GYO_PLUGINS`，開發與 CI 用） | SDL-2；三家的「用不用」都在建置宣告 |
| (2) 建置 | **Engine 要解決**（建置層的機制） | 建置層，自動：被選的就建 | — |
| (3) 連結 | **作者能處理**（`target_link_libraries`） | 遊戲的建置宣告。**對 `library_only` 與有單位的 Plugin 同一個動作**（建議由 (1) 的同一個宣告順便 link） | 不開特例；Module 今天已經是遊戲自己 link |
| (4) 帶上 | **作者能處理，但要 Engine 給機制**：實例只有程式碼建得出來，作者寫一行 `Add<T>()` 就行；Engine 要給的是預設群組與檢查（O-E1～O-E3、O-E6，已有） | **遊戲程式碼**（推薦，選項 C）；或照舊由建置產生（選項 A、B） | 第 1.2、3.4 節；一處 |
| (5) 放進 World | **作者能處理**（已經是） | 遊戲程式碼 `WorldDescription::units`（照舊） | — |
| (6) 跑不跑 | **作者能處理**（run condition 是 Kernel 的機制，條件是遊戲的） | 資料＋遊戲的條件（#367） | 第 6 節 |

**純函式庫 vs 會註冊的單位怎麼分**：(C) 之下由程式碼本身決定——有單位類別的才能 `Add`，沒有的根本沒有東西可加（Bevy、Unity 的分法）。登記檔的 `library_only` 欄照舊留著當「看得到」的標記（11-14 (2)「不進 World 的函式庫也登記，但要標明」）與 O-E2 的檢查，但**不再影響 CMake 的宣告**。

---

## 5. 同一個 Product 的不同 World 組成不同（第 4 題）

**查證結果：今天就能表達，而且在用。** `WorldDescription::units` 列名字，空＝帶上的全部（`Product.hpp:39-40`、`Product.cpp:197-213`）。PvP 的 Client：Frame World 列 `entity_ids`…`window_camera`＋`sdl_window`，Render World 列 `entity_ids`、`render_world`、`camera_render`、`sprite_render`、`mesh_render`、`ui_render`＋`sdl_gpu`（`ClientWorld.cpp:2230-2232`、`:2280-2286`）。Match 的 World 列 `entity_ids`、`world_time`、`world_asset`、`rollback`、`authority`、`net`、`transform`、`physics_3d`（`PvpWorld.cpp:1218`）。`sdl_audio` 帶上但不放進任何 World，只跑 `Configure`（`config/plugins.csv` 第 3 列；SDL 宣告的「只做 `Configure` 的單位」，`sdl-declaration.zh-Hant.md:161`）。

**今天的問題**（不是表達不了，是表達的方式）：

1. 帶上的清單（CMake）與各 World 的清單（程式碼）是兩份，要人手對齊（第 1.2 節）。
2. `depends` 兩義，逼出「每一列只扮一種角色」的規則（第 1.3 節）。
3. `units` 空＝全部，在多個 World 的產品裡容易漏列（SDL 宣告因此規定多 World 的產品明列，`sdl-declaration.zh-Hant.md:162`）。

**別家**：Bevy 由 Plugin 自己往 SubApp 加（遊戲只 `add_plugins` 一次，Plugin 知道自己哪一半進 RenderApp）；Unity 由 System 的 `WorldSystemFilter` 旗標＋bootstrap 挑；Godot 不適用。GYO 讓**遊戲**逐 World 列，這是 GYO 自己的選擇（單位不知道產品有幾個 World、叫什麼名字），本研究不推翻它。

**(C) 之下**：帶上的集合＝「各 World 的 `units` 的聯集＋只做 `Configure` 的」，由遊戲程式碼一處寫出；第 1 點消失，`bool window`／`bool gpu` 這類參數可以改成「呼叫端傳要不要加 `SDLWindowPlugin`」，或測試直接不 `Add` 它。第 2、3 點不受 (C) 影響（P7）。

---

## 6. 和模式配置（#367）的分工（第 5 題）

| 層 | 決定什麼 | 什麼時候定、能不能變 | 誰決定 | 機制 |
|---|---|---|---|---|
| 用（(1)～(3)） | 程式裡**有沒有**這段碼 | 建置時；要重建 | 遊戲的建置宣告（＋建的人） | CMake |
| 組成（(4)(5)） | World 裡**登記了**哪些型別、System、Resource | Product 建起來時；一個 World 的一生不變，要換就 `ReplaceWorld` | 遊戲程式碼 | `RegisteredUnits`、`WorldDescription::units` |
| 啟用（(6)） | 登記了的 System **這一刻跑不跑** | 每一步；資料決定 | 模式資料＋遊戲的條件 | run condition、帶條件的 set（#367） |

- #367 第 2.2、2.3 節已經說明：**登記會進 protocol hash 與 consistency fingerprint**，依模式改登記（R2）會讓模式不同的兩端在握手就被拒。所以 #367 選 (R1)「登記固定、跑不跑由模式決定」，並明寫「模式不決定 Engine Module」（第 2.1 節表的第 4 列）。
- 由此推得：**unit 的組成屬於「程式的結構」，不屬於「這一場的資料」**。它可以在程式碼裡依情況選（例：有沒有視窗、Client 還是 Match），但不該由一場的模式檔決定。這是第 7 節不推薦選項 (D) 的主要理由。
- 分工一句話：**組成決定「有什麼」，模式決定「什麼在動」**。兩者不重疊：#367 不需要改 unit 的組成，本研究的任何選項也不改 #367 的設計。

---

## 7. 選項（第 6 題）

| | 做法 | 第 1 點 | 第 2 點 | 好處 | 代價 | 要改多少 | 推翻的決定 |
|---|---|---|---|---|---|---|---|
| (A) 最小修補 | `gyo_product_units` 多一個關鍵字（例 `LIBRARIES ink`）或讓 `PLUGINS` 收 `library_only`：只選、只 link | 關鍵字版：解（CMake 上看得出差別）；`PLUGINS` 收版：**不解**（還是同一個字兩種意思） | 不解 | 改動最小；U6 當天解 | Module 與 Plugin 的「用」仍是兩條路；兩份清單照舊 | `GyoUnitRegistry.cmake` 一個函式＋`cmake.UnitRegistry` 測試 | 無 |
| (B) 拆 CMake 動作 | 新的「用」宣告（例 `gyo_use_plugins(<target> <名字>…)`：選、建、link，對所有 Plugin 一樣）；`gyo_product_units` 只管「帶上」（產生實例），不再選、不再 link | 解 | 不解 | CMake 上「用」與「帶上」分開，各一個意思 | 兩份清單照舊（甚至變成三個地方：用、帶上、`units`） | (A) ＋各產品的 CMake 多一行 | 無 |
| **(C) 程式碼組成（Bevy 形）** | CMake 只有 (B) 的「用」宣告；**`gyo_product_units` 拿掉**。建置層只產生「登記檔的全部列」（不依產品的選擇）與「預設 Module 群組」；遊戲在程式碼裡建 `RegisteredUnits`：預設群組（可關）＋`Add<SDLWindowPlugin>()`；各 World 照舊列 `units` | 解 | 解（帶上與放進 World 都在程式碼、一處） | 一個「用」的動作、對 ink 與 sdl 一樣；同一事實一處；編譯器檢查型別（沒 link 就 `Add` 不了）；執行期的檢查已經有 | 各產品的 `main`、測試改寫；要定「實例↔列」的配對（P5）與預設群組的形（P6） | 建置層 `GyoUnitRegistry.cmake` 的產生器、`RegisteredUnits` 的組法；呼叫 `gyo_product_units` 的 12 處（9 個 CMakeLists）、用 `MakeRegisteredUnits()` 的 15 個程式檔（games 3、examples 2、plugins 的測試 2、tests 8）；`cmake.UnitRegistry`（`tests/common/cmake/UnitRegistryTests.cmake`）、`build.ci` 的 `test_plugin_removal.py`（合成的 `gyo_product_units` 呼叫）、`orchestration.units` 守衛；O-6、骨架宣告 3.2 的條文 | 骨架宣告 3.2 的「退掉 `main` 手寫清單」；11-14「由清單產生」**依 P3 的讀法**可能要推翻 |
| (D) 資料組成（Godot 場景／`project.godot` 形） | 產品的組成寫在資料檔，執行時讀；建置層產生「所有被 link 的單位的工廠表」 | 解 | 解（而且不必重編） | 換組成不必重編 | 多一種資料格式與它的驗證；組成進了資料，就和 #367「登記固定」衝突（第 6 節）；C++ 靜態連結下，沒編進來的照樣用不了，好處有限 | 最大：(C) 的全部＋資料格式、載入、錯誤 | (C) 的全部＋#367 第 2.1 節的前提 |

另外考慮過、不列為選項的：

- **從 `target_link_libraries` 自動推「選了哪些 Plugin」**（掃 target 的 link 名字）：CMake 要在加 Plugin 目錄之前就知道，得在所有產品 configure 完後掃全部 target 的屬性，而 link 名字可以經由中間的函式庫間接帶進，掃不完整；而且「link 一個名字就觸發下載」是隱含的副作用。三家的「用」都是明寫的宣告。不推薦。
- **讓每個 Plugin 都有單位（照 Godot，空的也算）、刪掉 `library_only`**：可以讓 `gyo_product_units` 只剩一個意思，但 ink 得帶一個空的單位、被放進 Product，只為了配合建置層；這是用新的空殼去補結構（#366 的使用者「空殼刪掉」），不推薦。Godot 這樣做是因為它的 module 一律初始化、沒有「帶上」這一步。

### 7.1 推薦：(C)

理由：

1. 兩點都解，而且是從根解：「用」與「註冊」分開（第 3.4 節讀法 1），同一事實一處（讀法 2）。
2. 合使用者的「不開特例」：(1) 之後 Module 與 Plugin、有沒有單位都走同一條路；差別只剩 GYO 定義本身要求的「Plugin 要被選」與依賴方向。
3. 守住所有仍成立的理由（第 2 節）：列只在登記檔、依賴不寫在程式碼、編排層檢查、沒選的 Plugin 不下載、不用靜態自我登記。
4. 照主參考 Bevy 的 `add_plugins`／`DefaultPlugins`／`disable`，偏離的地方（依賴表、一列一單位、configure 時收集要求）都已寫明衝突點與膠水（第 3.4 節讀法 4）。
5. 執行期幾乎不用動：`Product::Build` 已經收手寫的 `RegisteredUnits`、已經有 O-E1～O-E3、O-E6；十幾處測試早就這樣寫。

代價要講清楚：(C) 推翻骨架宣告 3.2 的一條退掉理由，可能也推翻 11-14 的一句話（看 P3）；遊戲作者多寫幾行程式（預設群組一行＋每個 Plugin 一行）。

如果使用者不想現在動 (4)：**(B)** 是次選——第 1 點解掉、不推翻任何決定；它也是 (C) 的第 1 步，不白做。

### 7.2 遷移順序（照推薦 (C)）

每一步可以單獨合併、單獨回退；第 1 步之後 U6 就解了。

1. **「用」的宣告**（建置層）：新增一個 CMake 函式（名字照命名規範，例 `gyo_use_plugins`），把名字記進 `GYO_PLUGINS_REQUESTED`、在 Plugin 目錄加入後 link 它的 target；收任何 Plugin 列，不看 `library_only`。`gyo_product_units(PLUGINS …)` 暫時照舊（內部改成呼叫它）。ink 的測試、之後的 imgui 用它。守衛：`cmake.UnitRegistry` 加案例、`cmake.PluginSelection` 照舊。——到這裡等於 (B) 的一半。
2. **產生的東西拆開**：產生「登記檔的全部列」（例 `MakeRegisterRows()`）與「預設 Module 群組」兩樣，不依產品的選擇；`MakeRegisteredUnits()` 暫時保留、改成用它們組。不改任何產品。
3. **產品改成程式碼組成**：games（PvP 的 Client、Match，Object_Connect）、examples、plugins 的測試、tests 逐個改；每改一個就拿掉它的 `gyo_product_units` 呼叫。PvP 的 `bool window`／`bool gpu` 改成由呼叫端決定要不要 `Add`。
4. **收尾**：拿掉 `gyo_product_units` 與 `MakeRegisteredUnits()`；改 O-6、骨架宣告 3.2（加一條修訂）、`RegisteredUnits.hpp` 的註解；RC-O6 的判斷寫進 orchestration 頁。
5. （另案，P7）`depends` 一欄兩義。

### 7.3 要使用者決定的點

| # | 問題 | 選項 | 推薦 |
|---|---|---|---|
| P1 | 五件事的層別（第 4 節的表）：(1)(2) 建置層、(3) 遊戲的建置宣告、(4)(5) 遊戲程式碼、(6) 資料 | 照表／改某一列 | 照表 |
| P2 | 走哪個選項 | (A)／(B)／(C)／(D) | **(C)**；不想現在動 (4) 就 (B) |
| P3 | 11-14「執行時的 Registry 只收進 World 的登記單位，由清單產生，不另外手寫」的意思（本研究找不到原話以外的說明；理由 (b) 說的是登記檔的列） | (i) 指「列（名字、種類、依賴）不在程式裡再寫一份」——(C) 不衝突／(ii) 也包括「產品帶哪些單位」——選 (C) 就是推翻它 | 請使用者說是哪一個；(C) 在兩種讀法下做法相同，只差記錄上是不是推翻 |
| P4 | 「用」的宣告的形 | (a) 新的 CMake 函式，明寫名字（對 ink、sdl 一樣）／(b) 從 `target_link_libraries` 自動推 | (a)（第 7 節「不列為選項」第 1 點） |
| P5 | (C) 之下，實例怎麼對到登記檔的列 | (a) 照今天的 `UnitEntry{row, unit}`：程式碼寫名字字串，`Product::Build` 照舊用 O-E2 檢查／(b) 建置層替每個有單位的列產生型別對照（`Add<SDLWindowPlugin>()` 自己知道它是 `sdl_window`），名字仍只來自登記檔 | (b)：名字只寫一次、打錯是編譯錯誤；代價是每個單位的 target 多一個產生的檔。(a) 改動最小，可以先用 (a)、再換 (b) |
| P6 | 預設 Module（`default=yes`）怎麼表達 | (a) 產生的預設群組＋可關（`DefaultPlugins`＋`disable` 形）／(b) 每個產品自己全列 | (a)：照 Bevy，也合骨架宣告 3.2 第 4 點「Module 預設全帶、產品可以關」的現況 |
| P7 | `depends` 一欄兩義（N7；SDL 宣告「每一列只扮一種角色」的起因）這次動不動 | (a) 不動，另案／(b) 這次一起拆成 link 依賴與 World 依賴 | (a)：和本題獨立，(C) 不需要它；混在一起會讓一批碰到登記檔的格式 |
| P8 | U6 先行 | (a) 第 7.2 節第 1 步單獨先做、先解 ink 與之後的 imgui／(b) 等整條線一起 | (a)：第 1 步在 (B)、(C) 都要做，不白做 |

---

## 8. 沒查到、未確認的

- Unity 的遊戲專案 `Packages/manifest.json`：本機沒有，「專案宣告 package」只引了 package 自己的 `package.json` 與 asmdef。
- Unity.Mathematics、Collections、Burst 的源碼本機沒有；「它們沒有 System」是從依賴方向推的（System 的基底在 Entities 裡、它們不引用 Entities），沒有直接 grep 到。
- 11-14 那句話的意思（P3）：HANDOFF 只有卡片的轉述與卡片寫的理由。
- 第 7 節「要改多少」的檔數是 `git grep` 的計數（`gyo_product_units` 的呼叫、`MakeRegisteredUnits()` 的使用，不含 `docs/` 與 `build/cmake/` 本身），沒有逐檔估工時。
- O3DE、Unreal：骨架宣告 3.2 引過（「由建置工具照清單產生接上的程式」），本研究沒有重查源碼（本機沒有 Unreal 官方源碼，O3DE 不在 `~/Code/GitHome`）。
