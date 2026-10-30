# 音效（GYO::Audio）：分批計畫與進度

更新：2026-10-30。Owner：Engine（新增 `engine/audio` → `GYO::Audio`、`engine/audio/backend/sdl` → `GYO::AudioBackendSDL`）。
**狀態：`v8-engine-audio-plan` 完成（2026-10-30 使用者核准）；`v8-engine-audio-core` 完成（2026-10-30，本機 L1）；schedule、sdl、record 未開始。**本文件與 [PLAN](PLAN.md)、[HANDOFF](HANDOFF.md) 於 2026-10-30 由提出需求的消費端在它的規劃批次中建立（消費端 v7 D28：audio 計畫夾在音效批開始時建立）。

Engine 目前沒有音效輸出：

- `engine/asset` 有 `SoundLoader`（WAV PCM16 → `SoundAsset`），但沒有任何程式或測試使用它，也沒有混音器與輸出裝置。
- SDL 只有 VIDEO 子系統由 `SdlPlatform` 初始化；沒有任何 Engine 模組開啟音效裝置。
- 消費端要在「事件真正發生的時刻」發出聲音：事件帶著 Engine 時間戳（輸入事件、固定步的 Tick），而畫面在 30 FPS 時每 33 ms 才處理一次。若在處理到事件的那一幀才「立刻播放」，起音會落在幀格點上，連射的間隔會變成 33 ms 的倍數。

本計畫補上：一個不帶產品知識的混音器，以 Engine 時間戳排程每個聲音的起點（換算成自己的樣本位置），加上 SDL 的輸出後端。要播放什麼、何時觸發、音量與左右聲道的比例，由消費端決定。

先讀 [批次計畫](PLAN.md)（範圍、公開介面草案、Design Invariants、分批、驗收）和 [交接](HANDOFF.md)（等使用者決定的事、研究事實、壓力摘要）。

## 來源

- 提出需求的消費端（一個 4 人連線 FPS）在新版本的規劃中，把音效排為第一優先；它的原計畫（混音器規格、L2 的構想）以文字寫在 [HANDOFF](HANDOFF.md)。刪除該消費端後，本計畫仍可獨立理解。
- 依照其他 Engine 計畫夾的慣例（時間、執行緒與 Trace 計畫的 D12），本夾不連結、不點名消費端；程式檔案路徑與證據路徑作為資料保留。

## 依賴

| 依賴 | 狀態 | 用在哪裡 |
|---|---|---|
| `GYO::Time`（[time.md](../../time.md)） | 完成（TT-1） | 請求的時間戳、輸出回呼的錨點 |
| SDL 隔離（[platform-sdl.md](../../platform-sdl.md)） | 完成（IP-3、IP-4） | 後端遵守中立／native 分離、PRIVATE 連結、子系統各自計數初始化；G1～G3 守衛 |
| `GYO::Base` | 完成 | `Result`／`Error`、`GYO_ASSERT`、`Span` |

```text
GYO::Base ◄── GYO::Time ◄── GYO::Audio ◄── GYO::AudioBackendSDL ──(PRIVATE)──► SDL3
                                 ▲                   ▲
                               消費端（只連 Audio 與後端的中立標頭）
```

`GYO::Audio` 不依賴 SDL、不依賴 `GYO::Engine`（資產）；只在需要聲音的程序連結。伺服器端程序不連結任何音效 target。

## 進度

| 批次 | 建議檔位｜引擎 | 估時 | 狀態 | 交付邊界 |
|---|---|---|---|---|
| `v8-engine-audio-plan` | high｜Opus 5.5（主對話） | 3～5 h | 完成（2026-10-30 核准；[dev_log](../../../dev_logs/2026_10_30_engine_audio_plan.zh-Hant.md)） | 本夾三份文件 |
| `v8-engine-audio-core` | high｜繼承 | 3～4 h | 完成（2026-10-30；本機 core 35／35、突變 9／9；[dev_log](../../../dev_logs/2026_10_30_engine_audio_core.zh-Hant.md)） | 片段庫、聲部混音、離線 `Render`、統計與收支；`docs/architecture/audio.md`（契約與 Design Invariants） |
| `v8-engine-audio-schedule` | high；時間換算、遲到、SPSC 局部 xhigh（`xhigh-reviewer`，引擎繼承） | 4～5 h | 未開始 | 提交端（有界 SPSC）、時間戳 → 樣本位置、遲到的規則 |
| `v8-engine-audio-sdl` | high｜繼承 | 3～5 h | 未開始 | `GYO::AudioBackendSDL`：SDL 音效串流的回呼、AUDIO 子系統計數、G1～G3 擴充、`dummy`／`disk` 驅動的 L1 |
| `v8-engine-audio-record` | medium（執行與紀錄）；分析程式 high｜繼承 | 2～4 h | 未開始 | Engine 自己的 L2（`disk` 驅動、只記錄）、相關突變、dev_log |

`v8-engine-audio` 合計 12～18 h（與消費端計畫的估時相同）。檔位與引擎的說明見 [PLAN](PLAN.md)「分批」。

## 執行規則

- 每次只執行使用者指定的批次；commit 與 PR 用日語，程式註解用英文。
- 每批開始、里程碑、停止時，在同一變更中更新本表、[HANDOFF](HANDOFF.md) 與 dev_log（`docs/dev_logs/YYYY_MM_DD_engine_audio_<主題>.zh-Hant.md`），然後停止。
- Engine 子系統與它的第一個消費端放在同一個 PR（時間、執行緒與 Trace 計畫的慣例）：`v8-engine-audio` 的四批與消費端的第一個音效批同一條 PR 線。
- Engine 的驗收只驗 Engine 的目標（Engine 測試、Engine target 的依賴閉包），不以特定產品的存在為前提（AGENTS §7）。
- 計時相關的 L1 不設比結構條件更嚴的時間上界；精度只在 L2 以「只記錄」量（時間、執行緒與 Trace 計畫的規則）。L2 若要當判定，門檻在看結果前宣告並由使用者核准（AGENTS §14.1）。
- 主對話的檔位與引擎由使用者決定；ultracode 與高於主對話的檔位，在批次開始時說明並徵求同意（`agents/effort-gearbox.md`）。
- 出現非預期回歸、範圍擴大、超過估時 1.5 倍，或需要新的第三方依賴或新的 SDL 邊時，停下回報並重新規劃。
