# 命名規範（目標架構）

> Status: target, not implemented

更新：2026-11-11。Owner：架構（架構師會話）。來源：使用者 2026-11-11 決定（側邊會話轉述，audit 待決第 6 項附帶）。目的（使用者原話的意思）：**從名字看到歸屬與分類，也看出架構核心思想**；參考 Bevy。一個名字同時說出屬誰（1）、是什麼（2）、住哪（3）、進不進判定（4）；第 5 條防止回到舊思路。

## 五條

1. **歸屬＝namespace 與目錄一一對應**：`GYO::Kernel::`、`GYO::Engine::<能力>::`、`GYO::Plugins::<plugin>::`、`<產品>::`。
2. **分類＝詞性**（照 Bevy）：
   - System：動詞片語（`ApplyMembership`、`StepMovement`、`ExtractMeshes`）。
   - Component／Resource：名詞。
   - Event：過去式（`PlayerJoined`、`AssetLoaded`）。
   - Plugin：加 `Plugin` 後綴。
   - Schedule／stage：階段名（`Loading`、`FixedUpdate`；`Ingress`、`Simulate`、`Judge`、`Egress`）。
3. **World＝前綴**（照 Bevy）：渲染 World 的資料 `Gpu*`／`Extracted*`；boundary 輸入帶 `Sample`／`Input`（`HostClockSample`、`InputRing`）；文件／主 World 不加前綴。
4. **決定性區＝位置不是名字**：(c) 的 System 與欄集中在產品的一個 namespace／目錄（例：PvP 的 namespace `fps::pvp::sim`，對應 include 目錄 `RetroFPS/Sim`——`RetroFPS` 是 include 目錄名，不是 namespace；`v8-product-kernel-prep-zones-gate`），(b) 在 `present`、`ingress` 等對應的 namespace／目錄。區仍是宣告並啟動檢查（spec G.1）；目錄讓 J-17 的掃描有對象。這是 GYO 自己的一條，Bevy 沒有區的概念。
5. **禁止舊架構的角色後綴**：`Manager`、`Renderer`、`ViewModel`、`Host`、`Shell`、`Runtime`、`Controller`——它們命名「擁有東西的物件」，正是機制要消掉的形狀。例：`UiRenderer` → `Ui::LayoutText` 類 System 加 `GpuFontAtlas` 類 Resource；`WeaponViewModel` → `Present::WeaponPose` 欄加 `AnimateWeapon` System（使用者確認：這兩個照同一思路改）。

## 適用

- 遷移批的事前宣告要列**改名清單**（舊名 → 新名、歸屬、分類、World、區）。
- 文件裡引用現況程式時仍用舊名（它們還存在）；引用目標架構的東西用新名。
- 決定性區表（determinism.md §4）的條目照歸屬命名：以欄與 System 命名（例 `AnimationState` 欄、`AdvanceAnimation` System），函式庫名只在證據欄（audit §4.5 的處置）。

## Design Invariants

正式：使用者 2026-11-13 核准（kernel HANDOFF `v8-arch-target-di-approved`）。本節的變更是 Architecture Delta（AGENTS §3），要使用者同意。

1. **namespace 與目錄一一對應**，Kernel／Engine／Plugins／產品四個根。對應以「目錄 ↔ namespace」的一張表給定，不要求兩邊拼字相同（例：`RetroFPS/Sim` ↔ `fps::pvp::sim`；產品的根 namespace 由產品定）。守衛：掃描（檔案路徑與 namespace 不符即違規；`v8-arch-kernel-scaffold` 新增，J-9 掃描器擴充）。
2. **System 是動詞片語、資料是名詞、Event 是過去式、Plugin 帶後綴**。守衛：登記時的名稱檢查（Registry 可對 System 名做詞性白名單的弱檢查）＋審查。
3. **第 5 條的後綴在 Kernel、Engine、Plugins 的新程式中不出現**。守衛：掃描（`Manager|Renderer|ViewModel|Host|Shell|Runtime|Controller` 作為型別名尾；白名單只有仍存在的舊程式，隨遷移縮小）。

4. **名字是概念的 tag；Engine 的名字只指 Engine 的概念。** `engine/` 裡出現產品的詞，就是 Engine 裡有了產品的概念，那個東西不屬於 Engine。名字乾淨不等於概念乾淨：對一個中性的值做只有某個產品懂的處理，同樣違反本條。守衛：名字由掃描查（`neutral_names_guard.py`，voxel-mesh 與 map-markers；Kernel 目錄的產品名禁字表，`v8-arch-kernel-scaffold` 新增，J-9）；名字查不到的由審查查。（使用者 2026-11-13：名字是一個概念的極簡 tag，所以這一條既是命名也是分層；分層那邊引用到這裡——`runtime-composition.zh-Hant.md` §4。承接 legacy `docs/architecture.md`〈Layers〉check 1。plugins 層的同一條性質寫在 `docs/plugins/` 的層 DI，`v8-arch-plugins-foundation` 建。）

### Replacement Criteria（候選）

- 某類東西用五條都命不出名（例如同時是 System 又是 Resource 的形狀）→ 不是命名問題，是那個東西的形狀錯了，回到機制判。
