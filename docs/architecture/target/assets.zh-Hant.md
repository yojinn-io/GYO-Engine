# 資產（目標架構）

> Status: target, not implemented

更新：2026-11-11。Owner：架構（架構師會話）。來源：audit §3.6 第 4 項的使用者決定（側邊會話轉述）；參考 Bevy 的資產模型（`AssetServer`、`Assets<T>`、`RenderAssets<T>`）。
本文件只寫使用者已裁決的部分。

## 1. 今天的 `AssetManager` 不選 World，而是拆開（使用者 2026-11-11 決定）

使用者原則：不用舊思路處理新架構；參考 Bevy。審查原本問「`AssetManager` 屬 Frame World 還是 Render World」，這個問題本身沿用了「一個總管物件」的形狀，不採。

1. **請求與狀態 Resource**（Bevy `AssetServer` 的形狀）：要載什麼、載到哪了、發 Handle。世界內容的資產在 sim World（Loading schedule；spec H.2b）；純呈現用的在 Frame World。
2. **每種資產型別一個 `Assets<T>` Resource**（不是一個總管物件）；Handle 是帶引用計數的 id。放在使用它的 World（sim 或 Frame）。
3. **loader 由各 plugin 登記**（例：model-assets plugin 登記 ufbx loader）；ufbx 本身是函式庫，不是 plugin 也不是 System，被 plugin 的載入 System 呼叫。
4. **載入是外部服務**：IO 執行緒加交換點，與 Link 同形（spec I.1、I.2）；不是 v1 先同步讀檔。完成的資產由 Host 在**交換點**寫進 external 的 `AssetInbox` Resource；Loading 的 Ingress stage 的 `StageAssets`（(b)，Z3）把它搬進 role=Input 的 (c) 暫存 `AssetStaging`（以請求序號為鍵）；`資產` stage 的 (c) System `ApplyStagedAssets`（`AssetCursor` 的唯一寫者）只套用**連續完成的前綴**進 `Assets<T>`，缺一筆就等下一批，不依完成序（決定性；spec H.2b 第 2～3 點，F.2 不加階段；2026-11-12 N-2／B-3／N4 同步）。GYO 多一條 Bevy 沒有的約束：**內容身分是 (c)**（determinism.md §4.9），世界內容的資產在 Loading 宣告 ready 前全部套用完——今天「ready 前全載」的原意（blueprint §5 A 列）。
5. **GPU 資源**（貼圖、mesh buffer）是渲染 World 的 Resource（Bevy `RenderAssets<T>`）；Extract 搬「哪些資產變了」，渲染 World 的 System 上傳。

後果：今天的 `AssetManager` 物件消失；`UiRenderer`、`WeaponViewModel` 跨 World 持 `AssetManager*` 的問題隨之消失（它們各自改讀自己 World 的 `Assets<T>` 與渲染 World 的 GPU 資源）。audit 的 asset「包（先收攏）」判定不變，但「收攏成一個 Resource」改為「拆成請求狀態、`Assets<T>`、GPU 資源三類」。

## 2. 命名（待辦，規範本身待定）

使用者 2026-11-11 要求：`UiRenderer`、`WeaponViewModel` 這類名字都要改——它們在新架構裡不再是持指標的物件，而是 System 與 Resource；遷移批時照新架構的命名規範重命名。命名規範已定（使用者 2026-11-11）：見 `naming.zh-Hant.md` 五條；`UiRenderer` → `Ui::LayoutText` 類 System 加 `GpuFontAtlas` 類 Resource，`WeaponViewModel` → `Present::WeaponPose` 欄加 `AnimateWeapon` System。

## Design Invariants（候選；核准後成為正式條文）

1. **沒有資產總管物件**：請求狀態、`Assets<T>`、GPU 資源是三類 Resource，各在它們的 World；System 以句柄存取，不持指標。守衛：掃描（無 `AssetManager*`／`&` 成員）；spec J-5。
2. **載入是外部服務，完成的資產經 Loading 的 Ingress stage 進暫存、由 (c) System 依請求序號的連續前綴套用**：IO 執行緒不碰 World；套用順序與批次切分都與完成時序無關。守衛：等價測試（打亂完成順序，`Assets<T>` 的內容與 Handle 序列相同）。
3. **內容身分是 (c)，由 (c) System 從暫存的位元組算；世界內容的資產在 Loading 宣告 ready 前載完**（Startup 不載內容；runtime-composition §3a）。守衛：determinism.md §4.9 的既有 digest；「資產未就緒即 ready」是 Loading 的 System 以 Err 回傳的 Programmer Error。

### Replacement Criteria（候選）

- 出現必須在 Running 期間串流載入並進 (c) 的資產 → 本文件第 1 節第 4 點的「ready 前載完」要重審（lightyear／Bevy 的串流模型），不是在 `Assets<T>` 上加特例。
