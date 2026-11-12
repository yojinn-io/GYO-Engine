# 動畫事件 → 聲音（`gyo.event_sounds`）：framework 擁有的組合格式

更新：2026-11-04。Owner：framework（`framework/`）。架構卡 `v8-arch-event-sounds-contract`。
**狀態：兩批都完成（2026-11-04），等 PR 合併。**（2026-11-12 圖書管理員補註：後續：PR #145 已合併，`a3f145aa`） 先讀本頁，再讀 [交接](HANDOFF.md)。

## 代號對照

| 代號 | 白話 |
|---|---|
| v8-arch-event-sounds-contract | 整張卡：framework 加「動畫事件 → 聲音」的組合格式、驗證器、共用的排程偏移函式、Design Invariants、守衛與測試 |
| v8-arch-event-sounds-contract-doc | 第 1 批：契約文件、DI、事前宣告（規則清單、突變清單、守衛變更） |
| v8-arch-event-sounds-impl | 第 2 批：核心＋JSON 綁定＋測試＋突變＋守衛 |

## 來源

- 架構會話代為轉達的使用者決定（2026-11-04）：
  1. 「事件 → 聲音（與片段內的對齊點）」是產品與工具共用的 Data Contract，owner 是 framework。Engine 保持純粹：`gyo.anim_events` 第 1 版不動，`GYO::Audio` 不動。
  2. 聲音側走 B：每個動作一個事件、一段聲音。格式要讓 B 自然；A（一個事件、一段拼好的片段）只是 B 的特例。
  3. 依據 PR #142（`claude/arch-layer-formats`）的 `docs/architecture.md`「Layers: admission and format ownership」與 framework DI 3 改寫。
- 工具線的請求：分支 `claude/v8-tool-audio-editor-edit` 的 `docs/tools/plans/v8/anim-event-sound-request.md`。
- 成熟方案的對照（架構會話已做）：Godot 的 Audio 軌、Unreal 的 AnimNotify_PlaySound 把聲音放在動畫資料裡；Source 與 FMOD／Wwise 把名稱事件和聲音對應分開。GYO 選分開：Engine 純粹是既定原則、framework 是這類組合的家；代價是每個模型兩個檔，用事件名 join。
- 基底：#142 在本卡開工時（2026-11-04）尚未合併，本分支以它為基底（`ad348a0c`）。#142 同日合併進 master（`4918b1fa`）；PR 改由 yojinn-io/GYO-Engine#145（base master）承接。

## 現況（master `e15e5e41`）

- PvP 的對應只寫在 `assets/object_fps_pvp/audio/sfx.json` 的 `animation_events` 段（`magazine_in` → cue `reload`，`sync_seconds` 0.33）。讀它的是 `apps/object_fps_pvp/src/Pvp/SoundCues.cpp`（`ParseSoundSettings`、`FindEventSound`）；排程在 `apps/object_fps_pvp/src/Pvp/PvpApplication.cpp` 換彈那段，用 `EventSoundOffsetSeconds`（宣告在 `SoundCues.hpp`）；「對齊點不超過片段長度」的檢查在 `apps/object_fps_pvp/src/Pvp/PvpSound.cpp`。
- 事件資料：`assets/object_fps_pvp/weapons/nailer_v2/viewmodel/viewmodel.anim_events.json`，由 `nailer_v2_viewmodel.json` 的 `animation_events_asset_id` 引用；Engine 解析在 `engine/model/include/engine/model/AnimationEvents.hpp`。
- framework：核心不含 nlohmann，JSON 解析在 `framework/bindings/json/`（先例 `CatalogText.hpp`：結構型別與驗證在核心、文字解析在綁定）。守衛 `tests/framework/framework_dependency_guard.py`（核心規則＋每個綁定一張正向清單）與 `tests/framework/CMakeLists.txt` 的連結檢查。

## 格式（第 1 批定稿，見 [契約](../../../framework/event-sounds.md)）

```json
{
  "schema": "gyo.event_sounds",
  "version": 1,
  "animation_events_asset_id": "<產品的 anim_events 文字資產 id>",
  "events": {
    "<事件名>": { "sound_asset_id": "<type sound 的資產 id>", "sync_seconds": 0.33, "gain": 0.8 }
  }
}
```

一個檔對一個 anim_events 檔。規則比照 anim_events 第 1 版（欄位精確、鍵不重複、version 是整數 1、事件名非空（字元規則見下表）、`sound_asset_id` 非空、`sync_seconds` 有限且 ≥ 0、`gain` 由 `Engine::Audio::IsValidGain` 判定）。沒有選用欄位，一個事件名一段聲音。綁定：每個事件名至少出現在所引用 anim_events 集合的一個 clip；`sync_seconds` 不超過該片段長度（長度由呼叫者回答）。任何違反都是錯誤，不忽略、不夾緊、不修補。

### 對卡片建議形狀的調整（實作者的判斷，理由在括號內）

| 項 | 調整 | 理由 |
|---|---|---|
| 片段長度 | 綁定接一個回呼 `std::optional<double>(std::string_view soundAssetId)`；回答 `nullopt` 的 id 是錯誤 | 呼叫者（PvP、Model Viewer）都先經 Engine 載入片段，手上就有長度；回呼不必先組一張表。`nullopt` 當錯誤，才符合 Layers 檢查 4「引用的 id 要存在」 |
| 錯誤型別 | `EventSoundsError`，兩個 code：`InvalidEventSounds`（文件規則）、`UnboundEventSounds`（綁定）；detail 是出錯值的路徑（如 `events."magazine_in".sync_seconds`） | 工具會分開顯示「檔案壞了」和「和這個模型對不上」；Engine 的 `InvalidAnimationEvents` 兩者同碼，路徑仍照它的形式 |
| 分工 | 核心 `gyo/framework/EventSounds.hpp`：型別 `EventSoundSet`、`CheckEventSoundSet`（結構規則）、`BindEventSounds`、`BoundEventSounds`、`EventSoundOffsetSeconds`。JSON 綁定 `gyo/framework/json/EventSoundsText.hpp`：`ParseEventSounds(text)`（文字層規則：欄位精確、鍵不重複、型別），解析後呼叫核心的 `CheckEventSoundSet` | 比照 catalog（結構在核心、文字在綁定，DI 2／5）。換 JSON 函式庫只換綁定 |
| 綁定結果 | 驗證過的對應表 `BoundEventSounds`，`Find(eventName)` 回傳該事件的聲音或空。沒有對應的事件靜默，不是錯誤 | 兩個呼叫者都已從 Engine 拿到每個 clip 的事件（PvP 的 `GetActionEvents`、Model Viewer 的 `BoundAnimationEvents`），只需要逐名查聲音（PvP 今天的 `FindEventSound` 就是這樣）；依 clip 再列一份等於複製 Engine 的資料。對應是單向的：聲音引用事件，事件不必都有聲音（事件也可能是特效或玩法提示） |
| 事件名的字元規則 | 解析時只檢查非空；1～64 字元 `[a-z0-9_.]` 由綁定的「名字必須出現在所引用集合裡」間接保證 | Engine 的 `ValidEventName` 沒有公開；framework 自己再寫一份會碰到 framework 的 Replacement Criteria（複製沒有共用 fixture 檢查的 Engine 規則）。使用者 2026-11-04 核准（宣告 D-es-1） |
| `animation_events_asset_id` 的核對 | framework 不知道 id：呼叫者確認傳入的集合就是它點名的那個 | 比照 Engine（`BindAnimationEvents` 不知道 id，PvP 自己核對 `model_asset_id`）。framework 沒有 catalog 可查 |
| `EventSoundOffsetSeconds` | 搬進核心（含負偏移與 `nullopt` 條件），附測試；另外收緊為「結果不是有限值就沒有值」 | 兩個產品共用的排程算法，DI 5 的組合。收緊：有限的輸入也會溢位成 inf，審査者指出、使用者 2026-11-04 核准（PvP 的版本沒有這條） |

## 批次

第 2 批的判定、突變與守衛變更：[事前宣告](contract-declaration.md)（使用者 2026-11-04 核准）。

```text
v8-arch-event-sounds-contract
├── v8-arch-event-sounds-contract-doc   high｜契約文件、DI、事前宣告；使用者核准宣告後才進第 2 批
└── v8-arch-event-sounds-impl           high｜核心＋JSON 綁定＋測試＋突變＋守衛
```

上段（新 Data Contract、Design Invariants、守衛正向清單）。模型與檔位見 [交接](HANDOFF.md)。一條 PR；commit 與 PR 說明用日語；契約文件英文，計畫與 dev_log 繁體中文。

## 不做

- 不改 Engine（含 anim_events、`GYO::Audio`、`GYO::Model`）。
- 不改 PvP 的程式與資料；不改任何工具。
- 不做反射式／schema 驅動的通用編輯。
- 不為單一例外加任意條件的設定語言。

## 給下游的交接（本卡合併後，各線在自己的線上接）

- **PvP 遷移（產品會話做）**：`sfx.json` 升第 3 版、刪 `animation_events` 段與只被它用到的 `reload` cue；`nailer_v2_viewmodel.json` 加 `event_sounds_asset_id`；新檔手寫、登錄為 `text`；`SoundCues`／`PvpSound` 改呼叫 framework（`ParseEventSounds` → `BindEventSounds`，片段長度由 `PvpSound` 回答），刪自己的 `EventSoundOffsetSeconds`（framework 的版本對溢位的結果回傳沒有值，PvP 的實際輸入不受影響）；`SoundCuesTests.cpp` 的「event sounds name events of the weapon's gyo.anim_events」由 framework 的綁定取代；玩家聽到的不變。B 需要 anim_events 多幾個事件（`magazine_out`、`magazine_in`、`bolt_pull`）與 audio_editor 匯出的多段片段。
- **Model Viewer（anim-events-line）**：讀兩個格式（`gyo.anim_events` 經 Engine、`gyo.event_sounds` 經 framework 的 JSON 綁定）、用同一個 `EventSoundOffsetSeconds` 預覽；之後編輯 event_sounds（framework 不寫檔，工具自己經 `engine/io` 寫）。
- **audio_editor（工具線）**：不知道事件，不寫這個檔。
- **之後的候選（本卡不做）**：嚴格 JSON 的輔助函式（欄位精確、重複鍵的偵測）在 Engine 的 `AnimationEvents.cpp` 與 framework 的 `EventSoundsText.cpp` 各有一份（第二次出現，審査者記為 code smell）；寫入函式（序列化）等 Model Viewer 要編輯時另開一批。

## 完成

| 批 | 日期 | dev_log |
|---|---|---|
| v8-arch-event-sounds-contract-doc | 2026-11-04 | [契約與事前宣告](../../../dev_logs/2026_11_04_framework_v8_event_sounds_contract_doc.zh-Hant.md) |
| v8-arch-event-sounds-impl | 2026-11-04 | [核心、JSON 綁定、測試與守衛](../../../dev_logs/2026_11_04_framework_v8_event_sounds_impl.zh-Hant.md) |
