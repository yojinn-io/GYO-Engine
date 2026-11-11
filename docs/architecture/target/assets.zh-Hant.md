# 資產（目標架構）

> Status: target, not implemented

更新：2026-11-11。Owner：架構（架構師會話）。來源：audit §3.6 第 4 項的使用者決定（側邊會話轉述）；參考 Bevy 的資產模型（`AssetServer`、`Assets<T>`、`RenderAssets<T>`）。
本文件只寫使用者已裁決的部分。

## 1. 今天的 `AssetManager` 不選 World，而是拆開（使用者 2026-11-11 決定）

使用者原則：不用舊思路處理新架構；參考 Bevy。審查原本問「`AssetManager` 屬 Frame World 還是 Render World」，這個問題本身沿用了「一個總管物件」的形狀，不採。

1. **請求與狀態 Resource**（Bevy `AssetServer` 的形狀）：要載什麼、載到哪了、發 Handle。在文件／Frame World。
2. **每種資產型別一個 `Assets<T>` Resource**（不是一個總管物件）；Handle 是帶引用計數的 id。在文件／Frame World。
3. **loader 由各 plugin 登記**（例：model-assets plugin 登記 ufbx loader）；ufbx 本身是函式庫，不是 plugin 也不是 System，被 plugin 的載入 System 呼叫。
4. **載入是外部服務**：IO 執行緒加交換點，與 Link 同形（spec I.1、I.2）；不是 v1 先同步讀檔。完成的資產在同步點**依請求順序**套用進 `Assets<T>`，不依完成序（決定性）。GYO 多一條 Bevy 沒有的約束：**內容身分是 (c)**（determinism.md §4.9），進 Startup 的資產全部載完才凍結——今天「ready 前全載」的原意（blueprint §5 A 列）。
5. **GPU 資源**（貼圖、mesh buffer）是渲染 World 的 Resource（Bevy `RenderAssets<T>`）；Extract 搬「哪些資產變了」，渲染 World 的 System 上傳。

後果：今天的 `AssetManager` 物件消失；`UiRenderer`、`WeaponViewModel` 跨 World 持 `AssetManager*` 的問題隨之消失（它們各自改讀自己 World 的 `Assets<T>` 與渲染 World 的 GPU 資源）。audit 的 asset「包（先收攏）」判定不變，但「收攏成一個 Resource」改為「拆成請求狀態、`Assets<T>`、GPU 資源三類」。

## 2. 命名（待辦，規範本身待定）

使用者 2026-11-11 要求：`UiRenderer`、`WeaponViewModel` 這類名字都要改——它們在新架構裡不再是持指標的物件，而是 System 與 Resource；遷移批時照新架構的命名規範重命名。**命名規範本身待定**（候選：System 以動詞或「動詞＋對象」命名、Resource 以名詞、plugin 以 kebab-case 目錄名；由計畫批提案、使用者定）。

## Design Invariants（候選；核准後成為正式條文）

1. **沒有資產總管物件**：請求狀態、`Assets<T>`、GPU 資源是三類 Resource，各在它們的 World；System 以句柄存取，不持指標。守衛：掃描（無 `AssetManager*`／`&` 成員）；spec J-5。
2. **載入是外部服務，完成依請求順序在同步點套用**：IO 執行緒不碰 World；套用順序與完成時序無關。守衛：等價測試（打亂完成順序，`Assets<T>` 的內容與 Handle 序列相同）。
3. **內容身分是 (c)；Startup 的資產全載完才凍結**。守衛：determinism.md §4.9 的既有 digest；啟動時「資產未就緒即 Running」是啟動錯誤。

### Replacement Criteria（候選）

- 出現必須在 Running 期間串流載入並進 (c) 的資產 → 本文件第 1 節第 4 點的「Startup 全載」要重審（lightyear／Bevy 的串流模型），不是在 `Assets<T>` 上加特例。
