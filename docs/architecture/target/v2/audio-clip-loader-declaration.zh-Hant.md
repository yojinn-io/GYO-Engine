# 事前宣告：audio 那一側的小批（clip 的來源、音效 loader）（`v8-arch-gyo2-audio-clip-loader`）

更新：2026-11-23（修訂 0）。Owner：架構（GYO2 主會話開的任務卡 `gyo2-audio-clip-loader`）。**狀態：草稿，交使用者動工前核准。使用者說「開始」之前不改程式。**

**性質**：批次表「6. audio 那一側的小批」一列（HANDOFF `v8-arch-gyo2-world-audio-withdrawn`）的事前宣告。`engine/modules/audio` 是 Engine 的公開 Interface，音效的格式是工具與遊戲之間的 Data Contract，照 AGENTS §2 當**基盤**處理（第 3 節）。

**用詞**：標「**卡片判**」的，是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。別家的事實標出處：本機原始碼寫路徑（`~/Code/GitHome/…`）；**〔知〕**＝憑記憶、這次沒讀到原始碼，不當決勝的根據。

**核心一句**（每一項都拿來對照）：**音效是 asset 載好的不可變值，混音器播它、不另存一份；音訊執行緒從不釋放它。什麼時候載、什麼時候放掉、哪個 World 播，是作者的事。**

---

## 0. 讀的版本

- 程式：`origin/gyo2` 641a1f26 的 `engine/modules/audio`（`Mixer`、`ClipBank`、`VoiceMixer`、`ClipValidation`）、`engine/modules/asset`（`AssetLoader`、`LoadContext`、`AssetTypeName`、`TextLoader`）、`engine/modules/world_asset`（`AssetView::Get` 回 `const T*`）、`tests/common/audio`（7 個檔，`ClipSlotTests.cpp` 14 案）。
- 合約：[audio 頁](audio.zh-Hant.md)（DI 1～11、RC 1～9、第 5 節作者的寫法）；[asset 頁](asset.zh-Hant.md)（AS-2、AS-3、AS-9、AS-10、第 5 節 loader 介面）；[asset 宣告](asset-declaration.zh-Hant.md) 5.10、5.16；[audio 重審宣告](audio-review-declaration.zh-Hant.md) 第 4.2、5 節；`docs/cross-component-rules.md` CCR-6、CCR-9。
- 突變清單：`docs/architecture/plans/audio/scripts/mutations.json`（78 條，其中 22 條的 find 落在 clip 格、`LoadClip`、`UnloadClip`、`ClipBank` 一帶）、`mutations-asan.json`（1）、`mutations-tsan.json`（7，其中 2 條在 clip 格）。
- master（只看為什麼，不拿數據當設計）：`engine/asset/src/loaders/SoundLoader.cpp`（WAV PCM16、不查取樣率、沒有測試）；`docs/architecture/plans/audio/HANDOFF.md:126` `v8-engine-audio-clip-format`（`ClipBank` 複製；不讓 Audio 依賴 Engine 的理由是「離線測試要連整個 Engine」）；`docs/tools/plans/v8/assets.md:53-56`（Data Contract 是「48 kHz PCM16 的 WAV＋catalog 條目」，腳本、手工、audio_editor 都能產生）；`docs/audio_editor/architecture.md` DI 2（輸出照 Engine 公開的格式；驗證是「每個輸出經 `SoundLoader` 載得了、`ClipBank` 收得下」）；`tools/audio_editor/include/gyo/audio_editor/WavFile.hpp`（`EncodePcm16Wav`：標準 44 位元組檔頭、格式碼 1）；`assets/object_fps_pvp/asset_catalog.json:19-22`（4 筆 `"type": "sound"`、`path` 是 `.wav`、不寫 `format`）。
- 別家：
  - **Bevy**（主參考；`~/Code/GitHome/bevy/crates/bevy_audio`）：`audio_source.rs:7-20` `AudioSource { bytes: Arc<[u8]> }` 是 Asset，複製只加參照計數；`audio_source.rs:39-75` `AudioLoader` 在 bevy_audio 裡、整檔讀進來不解碼，認 wav／ogg／flac／mp3（看 feature）；`Cargo.toml:14` bevy_audio 依賴 bevy_asset；`audio_output.rs:84-112` 播放的 System 從 `Assets<Source>` 取值，沒載好就下一幀再試，取到就 `decoder()`（`audio_source.rs:94-100`，把 `AudioSource` 的複製交給 rodio，在 rodio 的混音那一側解碼）。rodio 在哪條執行緒放掉那份複製〔知：混音執行緒〕。
  - **Kira**（Bevy 生態的 bevy_kira_audio 用它〔知〕；`~/Code/GitHome/kira`）：`sound/static_sound/data.rs:23-32` `StaticSoundData { frames: Arc<[Frame]> }`，載入時解碼成取樣、複製只加參照計數；**音訊執行緒不釋放**：用完的資源推進「unused」環（`backend/resources.rs:131-170`，環滿時留在原處、下次再推），遊戲那一側在下一次加入資源（`play`）時取出、drop（`backend/resources.rs:207-222`）。
  - **SoLoud**（`~/Code/GitHome/soloud`）：`Wav` 載入時解碼、`WavInstance` 讀父物件的資料不複製；刪 `AudioSource` 時先 `stop()` 所有實例（`src/core/soloud_audiosource.cpp:208-211`），靠和混音共用的 mutex——GYO 的音訊執行緒不上鎖（DI 7），不採。
  - **miniaudio**（`~/Code/GitHome/miniaudio/miniaudio.h:10324-10465`）：resource manager 以名字共用 data buffer、參照計數；`MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE` 在資源管理那一側先解碼、不在混音執行緒（`:10332`）。
  - **沒有一家有「出聲前登記的庫」或「每個提交口各自載一份」**：共用一份、播放時參照它。

## 1. 先用遊戲講

**FPS 的 Client 開槍。** 進對局前，作者 `Load<Clip>(AssetId("object_fps_pvp.audio.gun_shoot"))`；asset 在背景讀 `audio/gun_shoot.wav`、照 WAV 的規則解成 48 kHz 的取樣，變成一個不可變的 clip。第 312 幀開槍，表現層的 System 從 `AssetView` 拿到那個 clip，放進要交給混音器的請求；混音器播它，**不另存一份**。兩個 World（例：畫面 World 與重播 World）播同一個槍聲，記憶體裡也只有一份。

**讀存檔換 World。** 新 World 請求同樣的 id，asset 命中；舊 World 已送出、還在響的槍聲照響完——播放自己握著那份 clip。今天的寫法（audio 頁 5.2）要作者記得「替換前卸掉舊 World 載進提交口的 clip」，否則幾次替換後 `NoClipSlot`；這一批之後沒有這件事。

**音效檔換了版本。** 作者用 audio_editor 重新匯出 `gun_shoot.wav`（48 kHz PCM16），catalog 的 id 不變；手寫、腳本產生的同格式檔一樣有效（AGENTS §8）。檔是 44.1 kHz 的話，載入失敗、碼說「取樣率不是 48000」，World 照跑（asset 不記日誌、不重試，AS-8）。

**audio_editor 預覽。** 工具把剪輯後的取樣做成一個 clip（不經 asset）交給混音器播；換一段再做一個。舊的 clip 在最後一個播放結束、工具放掉它之後釋放。

## 2. 前提表

每一項標：**Engine**（只有 Engine 做得到、要在這批定）或**作者**（現有機制就做得到，Engine 只寫清楚）。

| # | 前提 | Engine／作者 | 誰定的 | 掛在它上面的 |
|---|---|---|---|---|
| P1 | 音效是 asset 載好的值：不可變、多執行緒同時讀、任何執行緒解構（AS-10、CCR-9） | Engine（asset） | 使用者 11-22 核准 asset 宣告；#283 合併 | 4.1 共用不複製 |
| P2 | 音訊執行緒不配置、不釋放（DI 7） | Engine（audio） | master 起；#279 照舊 | 4.1 釋放走回收環 |
| P3 | 混音器只吃 48 kHz、1～2 聲道的 PCM16，不重新取樣（DI 9） | Engine（audio） | master `v8-engine-audio-clip-format`；#279 照舊 | 5 的格式 |
| P4 | loader 歸擁有那個型別的 Module（asset 5.16、AS-2） | Engine（asset） | 使用者 11-22 | 4.2 位置 |
| P5 | 工具寫、遊戲讀同一個格式 → 放在共通層、是 Data Contract（AGENTS §6、§8） | Engine | AGENTS；`v8-arch-gyo2-world-audio-withdrawn` 的判法 | 5 |
| P6 | 把 World 接到混音器（提交口放哪、哪個 System 交、時刻從哪來）是作者的寫法（audio 頁第 5 節） | 作者 | 使用者 11-22「照建議」 | 不在本批；5.2、5.4、5.5 的字隨 4.1 改 |
| P7 | 什麼時候載、等不等載完、什麼時候放掉 Handle | 作者（AS-7） | asset | 4.1：clip 活多久由持有它的人決定 |
| P8 | 同一個聲音被哪個 World、哪個提交口播、播幾次 | 作者 | — | 4.1：任何提交口都能播任何 clip |
| P9 | 串流、壓縮格式、重新取樣 | 沒有點名的 Feature | RC 1、DI 9 | 不做 |
| P10 | catalog 的型別名是 `sound` | Engine（Data Contract） | master 的內容與 audio_editor（`SoundExport.hpp` 寫死 `"sound"`）；**卡片判**：照舊 | 5 |

**沒有要作者多做的事**：音效的載入照一般 asset（`Load<Clip>`、`AssetView::Get`）；播放照 audio 頁第 5 節。

## 3. 基盤的根據（AGENTS §2）

**為什麼算基盤**：積在上面的有 SDL 的輸出後端、每個產品的聲音、audio_editor 的預覽與匯出、master 的 4 個音效檔與日後的所有音效內容；clip 的介面與檔案格式一改，這些都要改。

| # | 根據（出處） | 種類 | 要求 | 本批 |
|---|---|---|---|---|
| C-1 | ① FPS Client 的槍聲、受擊、換彈、腳步經 asset 載入（asset 宣告 F1：wav 4 個） | 名指的 Feature | `sound` 型別的 loader；載好的值能直接播 | 做 |
| C-2 | ① 讀檔換 World（O-8）時聲音不斷、不必搬 clip | 名指的 Feature | 播放自己握著 clip；不綁提交口 | 做（4.1） |
| C-3 | ①② 多個 World 用同一個音效（asset：兩個 World 同一 id 只載一次） | 名指的 Feature | 記憶體一份 | 做（4.1） |
| C-4 | ③ audio_editor 的匯出與預覽（master control 層；batch plan 第 112 行搬回時照新契約） | 名指的 Feature（工具） | 工具寫的檔遊戲讀得了；工具自己做的取樣也能播 | 做（5、4.1） |
| C-5 | 規模：master 的 PvP 4 個 wav 共 350 KB；audio_editor 讀檔上限 64 MB（`assets.md:192`） | 規模 | 預設上限容得下工具寫的最大檔 | 5.4 |

## 4. 判定

### 4.1 clip 的來源（`ClipBank` 該不該存在、共用或複製）——第 1 題

**判定（建議 (E)）**：拿掉 `ClipBank` 與提交口的 clip 格；clip 是一個**共用取樣的不可變值**，**每次播放帶著它**。這就是 Bevy、Kira 的資料模型；釋放那一半照 Kira：音訊執行緒把用完的 clip 經回收環還給提交口，下一次使用提交口時才放掉。

**新的形狀**（名字卡片判，照 N15、N16）：

```cpp
// engine/audio/Clip.hpp
class Clip final {                       // 共用取樣的不可變值；複製只加參照計數
public:
    Clip() = default;                    // 空的；拿去播是 Programmer Error
    // 複製一次取樣（工具、程式產生的聲音）
    static Base::Result<Clip, AudioError> Make(Base::ConstSpan<std::int16_t> interleaved,
                                               std::uint16_t channels, std::uint32_t sampleRate);
    // 接收取樣、不複製（loader 用）
    static Base::Result<Clip, AudioError> Make(std::vector<std::int16_t>&& interleaved,
                                               std::uint16_t channels, std::uint32_t sampleRate);
    ClipView View() const noexcept;      // 取樣、聲道數、frame 數
    explicit operator bool() const noexcept;
private:
    std::shared_ptr<const Detail::ClipData> data_;
};
template <> struct Asset::AssetTypeName<Audio::Clip> { static constexpr std::string_view value = "sound"; };

struct PlayRequest final { Clip clip; Time::TimePoint at{}; float gainLeft = 1.0f; float gainRight = 1.0f; };
```

- 規則照 `ClipValidation`（InvalidClip：空的、聲道數不是 1 或 2、樣本數不是整數個 frame、取樣率不是 48000），只在 `Clip::Make` 一處判。
- `Mixer::Create(const MixerConfig&)`：不再收 `ClipBank`。`MixerConfig::clipSlotsPerSubmitter` 拿掉。
- 拿掉：`ClipBank`、`ClipId`、`Submitter::LoadClip`、`Submitter::UnloadClip`、`AudioErrorCode::NoClipSlot`（5 不重編，照 4 的先例）、`MixerStats` 的 `unloads*` 三個計數、`ClipSlot`、租約。
- 留：`ClipView`（搬到 `Clip.hpp`）、`SoundHandle`、`Stop`、`StopAll`、`PlayheadOf`、計數的其餘部分、時間點換算、`VoiceMixer` 的混音。

**音訊執行緒不釋放（DI 7）怎麼守**：
1. 提交口的 `Play` 把請求（含 clip 的一份參照）複製進環——參照計數加一在使用提交口的一方。
2. 音訊執行緒取到播放：拿到 voice 就把 clip **搬進** voice（指標搬動，不碰參照計數）；沒有 voice（`droppedNoVoice`）就直接搬進那個提交口的**回收環**。
3. voice 結束（播完、停止、還沒開始就被停）時，音訊執行緒把 clip 搬進播它的提交口的回收環（提交口由 handle 的高 7 位元得知）。
4. 使用提交口的一方每次呼叫 `Play`、`Stop`、`StopAll` 時，先把回收環取空、放掉那些參照。最後一份參照在這裡、在 asset 的 store（維護時）或在作者手上放掉——**永遠不在音訊執行緒**。
5. 回收環在建混音器時配好，每個提交口 `queueCapacity + maxVoices + 1` 格。**不會滿**：每次呼叫先取空再送最多一個請求，所以任何時刻屬於一個提交口的 clip 參照（環裡的、voice 裡的、回收環裡的）不超過 `queueCapacity + maxVoices + 1`（推導寫進 `Mixer.cpp` 的註解，壓力測試釘住）。Kira 的環滿時「留在原處、下次再推」不需要。
6. 停用的提交口（解構、或作者不再呼叫）不取它的回收環：那些 clip 留到下一次呼叫或混音器的狀態解構（上限同上）。沒有 `Render`（後端沒開）時，排著的請求連同 clip 留在環裡，直到混音器的狀態解構。這和今天「銷毀的提交口不還它的格」同類，寫進 Contract。

**比過的選項**（audio 重審宣告第 5 節的 A～D，加本卡的 E）：

| 選項 | 內容 | 後果 | 和 Bevy |
|---|---|---|---|
| **(E) 建議** | 拿掉 `ClipBank` 與 clip 格；播放帶共用的 clip；回收環 | 一條路、記憶體一份；任何提交口能播任何 clip；沒有格數要配、沒有 `NoClipSlot`、沒有卸載；audio 頁 5.2「clip 格不跟著沿用」、5.4 的 `LoadClip` 值、5.5「先送出緩衝再卸載」三件作者要記的事消失。代價：改寫 clip 那一半（`Mixer.cpp`、`VoiceMixer`、7 個測試檔的設定、`ClipSlotTests.cpp` 整檔換成 clip 壽命的測試、22＋2＋1 條突變退役或改寫）；`PlayRequest` 不再是可平凡複製的型別（環的一格多一個 `shared_ptr`） | **資料模型相同**（Bevy `Arc<[u8]>`／Kira `Arc<[Frame]>`）。偏離：Bevy 把參照交給 rodio、在混音那一側放掉〔知〕；GYO 照 Kira 走回收環。**衝突點**：DI 7。**膠水**：每個提交口一個 SPSC 回收環＋呼叫時取空的迴圈；拿掉的格、租約、卸載、世代比它多 |
| (A) | 拿掉 `ClipBank`；`LoadClip` 收共用的 `Clip`、不複製；格與卸載照舊 | 記憶體一份；clip 只能由載它的提交口播，兩個 World 播同一音效各佔一格；`clipSlotsPerSubmitter` 預設 0 要改（照 asset 非同步載入，第一次 `LoadClip` 就 `NoClipSlot`；建議改 32，和 `maxVoices` 預設同）；audio 頁 5.2、5.5 的坑留著；沒有 `Render` 就不回收格。改動比 (E) 小（約一半） | 偏離：Bevy／Kira 沒有「每個提交口載一份」。**膠水**：格、租約、卸載、世代、`NoClipSlot`，作者還要寫替換時的卸載（5.2） |
| (B) | 拿掉 `ClipBank`；`LoadClip` 照舊複製 | 同 (A) 的格的後果；每個音效在 asset 與混音器各一份、兩套有效期 | 偏離更多（複製） |
| (C) | 兩條都留 | 同一件事兩條路；`ClipBank` 在 asset 非同步載入下填不進遊戲的音效（混音器在 `Configure` 建時 asset 還沒載好） | 偏離（Bevy 沒有出聲前的庫）；不合「能拿掉的就拿掉」 |
| (D) | 混音器只借 asset 的記憶體（view），asset 在音訊執行緒還在讀時不得釋放 | asset 要知道混音器的租約（跨 Component 的狀態，AGENTS §3 State Authority）；或另一套交還協定 | 偏離。(E) 用共用所有權解掉同一件事；「clip 的記憶體必須在音訊執行緒還在讀時被收回、或以位元組數設上限」本來就是 RC 1，照舊記在那裡，不另立 RC |

**為什麼建議 (E)**：
1. 核心一句：「播它、不另存一份」——(A)(E) 都做到；「任何 World 都能播」只有 (E)。
2. 不堆屎山：(E) 拿掉的機制（格、租約、卸載、世代、`NoClipSlot`、卸載計數）比加上的（回收環）多，作者那一側少三件要記的事。
3. 主參考：資料模型和 Bevy 相同，唯一的偏離（回收環）來自 Bevy 生態用的 Kira，而且是 DI 7 要求的。
4. `gyo2-mechanism-not-conservative`：(A) 是保守的小改，格的模型會被上面的產品與 audio_editor 積上去，之後不會有人回頭拿掉。

**推翻的舊決定**（要使用者同意，AGENTS §3）：audio 重審宣告第 2 節「clip 的租約與交還：留，DI 7 的核心」——性質（音訊執行緒不釋放）留，機制從「格＋租約」換成「回收環」；master `v8-engine-audio-control` 的 `LoadClip`／`UnloadClip`（為 audio_editor 預覽做的）——預覽的需求（C-4）由 (E) 直接滿足。

**`clipSlotsPerSubmitter` 預設 0**：(E) 下這個設定拿掉，問題消失；選 (A) 時改成 32（上一列）。

### 4.2 音效 loader 放在哪裡——第 2 題

**判定（建議 (i)）**：loader 與 `AssetTypeName<Clip>` 放在 `GYO::Audio` 裡，`GYO::Audio` 連 `GYO::Asset`（登記檔 `audio` 列 `depends=asset`，列移到 `asset` 之後；O-6）。

| 選項 | 內容 | 後果 | 和 Bevy |
|---|---|---|---|
| **(i) 建議** | `GYO::Audio` 連 `GYO::Asset`；`Clip.hpp` 帶型別特性，`WavLoader.hpp` 是 loader | 一個 Module 擁有型別、規則與 loader（asset 頁第 1 節「擁有型別的 Module：loader 與型別特性的函式庫只依賴 asset」，這裡就是 audio 本身）。改 DI 2 的連結（Base、Time → Base、Time、Asset）與 audio 頁第 1 節「不認得 asset」。用混音器的程式會連到 asset（＋私有的 io、json）；用混音器而不用 asset 的使用者今天沒有（audio_editor 兩者都用） | **相同**：bevy_audio 依賴 bevy_asset，`AudioLoader` 在 bevy_audio（`Cargo.toml:14`、`audio_source.rs:39`） |
| (ii) | 另開一個小函式庫（例 `engine/modules/audio_asset`，`GYO::AudioAsset`），連 Audio 與 Asset；`Clip` 在 audio、型別特性與 loader 在它 | DI 2 不變；多一個 Module 目錄、登記檔一列；`Clip` 與它的型別特性分在兩個 Module（只引 `Clip.hpp` 時 `Load<Clip>` 編譯錯誤，要記得引另一個標頭） | 偏離：Bevy 沒有拆。**膠水**：一個目錄、一列登記、一份 CMake，作者多記一個標頭 |

master 當年不讓 Audio 依賴 Engine 的理由（「離線測試要連整個 Engine」，`v8-engine-audio-clip-format` B）在 gyo2 不成立：asset 是不認得 ECS 的小函式庫（`library_only`），audio 的測試照樣離線、不開裝置。

### 4.3 和 Bevy 不同、照舊決定的——第 3 題（一起確認）

| 和 Bevy 不同 | GYO | 為什麼照舊 | 碰到哪條 |
|---|---|---|---|
| Bevy 的 asset 存**編碼過的位元組**，播放時在混音那一側解碼 | 載入時解成取樣（Kira、SoLoud、miniaudio 的 DECODE 同） | 音訊執行緒不配置（DI 7）；解碼在混音那一側要配置 | DI 7 |
| Bevy 認 wav／ogg／flac／mp3（看 feature）、任何取樣率 | 只認 WAV PCM16、48 kHz、1～2 聲道；其他格式由工具事先轉 | 不重新取樣（DI 9）；壓縮格式要第三方解碼器，沒有點名的 Feature（RC 1；audio_editor RC「Formats other than WAV must be read」） | DI 9、RC 1 |
| Bevy 由 `AudioPlayer` 元件與播放的 System 等載好再播 | 作者在表現層 System 用 `AssetView` 取 clip 再交（audio 頁第 5 節） | 使用者 11-22 不做 Bevy 式便利層（`v8-arch-gyo2-world-audio-withdrawn` 第 2 件） | — |

### 4.4 順手補：audio 頁 §5.2 引 O-2 新補的半句

§5.2「根據」一段，O-2 那一句改成引原文：「登記單位以 World 名為鍵記的資料在 `Build` 時重設（不含 O-15 共用處裡跨 World 的物件——它們比每個 World 活得久，例：照 World 名沿用的提交口）」（`orchestration.zh-Hant.md` O-2，`v8-arch-gyo2-o2-shared-note`），刪掉「主會話 2026-11-23 確認」那半句的推論。HANDOFF `v8-arch-gyo2-audio-o2-cite-deferred`。

## 5. 音效的 Data Contract（`sound`／`wav`）——卡片判（照 master 與 AGENTS §8）

**誰產生、誰讀**：audio_editor 寫（搬回時照這裡）、Script（master 的 `convert_wav.py` 類）、手工（任何能輸出 48 kHz PCM16 WAV 的軟體），一樣有效；遊戲經 asset 讀，執行期讀、不需要 build 的產物；工具要驗證自己的輸出就呼叫同一個 loader（asset 宣告 5.10 第 3 點）。

### 5.1 catalog 的項目

```json
{"id": "object_fps_pvp.audio.gun_shoot", "type": "sound", "path": "audio/gun_shoot.wav"}
```

- `type`：`sound`（`AssetTypeName<Audio::Clip>`）。
- `format`：`wav`（`path` 的副檔名推得出，可省；asset 頁第 3 節）。
- `settings`：沒有；寫了照收、忽略（照文字 loader）。

### 5.2 檔案格式（Version 1）

RIFF/WAVE，小端序：

| # | 規則 | 碼（`WavLoaderCode`） |
|---|---|---|
| 1 | 至少 12 位元組，開頭是 `RIFF`、第 8～11 位元組是 `WAVE` | `NotWave` |
| 2 | 從第 12 位元組起是一串 chunk（4 位元組 id＋4 位元組長度＋內容，長度是奇數時補 1 位元組）；每個 chunk 的內容都在檔案內（最後一個 chunk 的補位可以缺）。RIFF 檔頭的長度欄不查（寫錯的軟體常見） | `Truncated` |
| 3 | `fmt ` 與 `data` 各恰好一個（先後不限）；其他 chunk（`LIST` 等）跳過 | `MissingChunk`、`DuplicateChunk` |
| 4 | `fmt ` 至少 16 位元組；格式碼 1（整數 PCM）、每樣本 16 位元 | `NotPcm16` |
| 5 | `fmt ` 的 block align 等於聲道數 × 2 | `BadFormatChunk` |
| 6 | 聲道數 1 或 2、取樣率 48000、`data` 非空、`data` 的長度是 block align 的整數倍——即 `Clip::Make` 的規則（`ClipValidation`），失敗原樣帶出（detail 寫哪一條） | `InvalidClip` |

- 取樣是有號 16 位元、小端序、聲道交錯，照位元組原樣變成 clip（不抖動、不換算）；`fmt ` 的 byte rate 不查（由 block align 與取樣率決定）。
- **Version**：WAV 本身沒有 GYO 的版本欄，Version 由 catalog 的（`type`、`format`）表示（AS-9）。放寬（多收：例 `WAVE_FORMAT_EXTENSIBLE` 裡的 PCM16）不改名；收窄、或改變已收內容的意思，用新的 `format` 名登記另一個 loader（例：日後的引擎格式）。
- 標準寫法（工具、腳本照這個寫）：44 位元組檔頭（`RIFF`、`WAVE`、16 位元組的 `fmt `、`data`），即 master `EncodePcm16Wav` 的輸出。

### 5.3 loader

```cpp
class WavLoader final {                  // engine/audio/WavLoader.hpp
    using Output = Clip; using Settings = WavSettings; using Code = WavLoaderCode;
    static constexpr std::string_view kTypeName = "sound";
    static constexpr std::array<std::string_view, 1> kFormats{"wav"};
    static constexpr std::size_t kDefaultMaxBytes = std::size_t{64} * 1024u * 1024u;
    // ParseSettings：任何 settings 照收、忽略。Load：照 5.2，取樣只複製一次（位元組→取樣），不宣告引用
};
```

- **登記**：world_asset 不認得音效（AS-2），由產品或擁有它的單位在 `Configure` 經 `AssetLoaderRegistry::Register<WavLoader>()` 登記。今天 audio 是 `library_only`、沒有登記單位，所以是產品自己登記（一行）；要不要給 audio 一個 World 端的登記單位（像 world_asset），等點名的 Feature（使用者 11-22 不做便利層）。**卡片判**。
- **預設上限 64 MiB**：audio_editor 讀檔上限（`assets.md:192`），48 kHz 立體聲 PCM16 約 5.8 分鐘；產品可用 `byteLimits` 覆寫。
- 純函式（AS-3）：沒有 World、沒有時鐘、沒有全域可變狀態；同樣的位元組得到同樣的 clip。`Clip` 合 AS-10 (c)／CCR-9（沒有 `mutable`、參照計數是原子的、解構不綁執行緒；「不在音訊執行緒解構」由混音器自己守，4.1）。

## 6. Architecture Delta（AGENTS §3 的七項）

1. **為什麼要變**：批次表「audio 那一側的小批」；Feature C-1～C-4。
2. **現在的問題**：(a) `ClipBank` 在 asset 非同步載入下填不進遊戲的音效；(b) `LoadClip` 再複製 asset 已有的不可變值；(c) clip 綁在載它的提交口，作者要在替換 World 時搬卸（audio 頁 5.2）；(d) `clipSlotsPerSubmitter` 預設 0，第一次 `LoadClip` 就 `NoClipSlot`；(e) 沒有 `sound` 的 loader，工具與遊戲之間沒有寫下來的格式。
3. **變的邊界**：`GYO::Audio` 的公開介面（4.1 的拿掉與新增）；DI 2 的連結（第 2 題 (i)）；新的 Data Contract（第 5 節）。
4. **影響**：audio 與它的測試、突變清單；audio 頁；asset 頁第 1 節「不在範圍：…音效」與第 9 節；登記檔 `config/modules.csv`（audio 列）；`tests/common/audio/CMakeLists.txt` 的連結檢查。gyo2 上沒有別的程式用 audio：`git grep -l "engine/audio/" -- engine examples tests ':!engine/modules/audio' ':!tests/common/audio'` 沒有命中（終了碼 1）；拿掉兩個排除時命中 `engine/modules/audio` 的 7 個檔（陽性對照）。641a1f26，2026-11-23；實作前再跑一次。
5. **依賴方向**：新增 `GYO::Audio → GYO::Asset`（第 2 題 (i)）；都是 Engine 的 Module、往下，沒有循環（asset 不認得 audio）。
6. **Ownership**：`sound` 型別、它的格式與 loader 歸 audio（asset 5.16）。clip 的取樣由所有持有者共有；混音器只保證最後一份不在音訊執行緒放掉。
7. **為什麼不能更小**：(A) 是更小的一步（第 1 題），但留下格的模型與作者那一側的三件事；只加 loader 不動 `ClipBank`，遊戲的音效還是要複製進格、還是預設 0 格。

## 7. Design Invariants 與 Replacement Criteria 的改動（草案；最終文字在合併前核准）

- **DI 2**（第 2 題 (i)）：連結恰好是 `GYO::Base`、`GYO::Time`、`GYO::Asset`；守衛 `gyo_expect_audio_links` 跟著改。「不含 SDL」不變。第 1 節「不認得 SDL、asset、World、產品」改成「不認得 SDL、World、產品；認得 asset 的 loader 介面」。
- **DI 6**：拿掉卸載的帳（`unloadsSubmitted` 一句與守衛「may unload again after its queue was full」）。
- **DI 7**：「一個 clip 的樣本在 voice 或排著的播放可能讀它時不變」改成：clip 的取樣不可變（`Clip` 沒有改它的操作）；播放與 voice 各握一份參照；音訊執行緒只搬動參照、從不放掉——用完的經提交口的回收環交回，由使用提交口的一方在下一次呼叫時放掉，或隨混音器的狀態放掉。守衛換成：「Render allocates and frees nothing while the last other holder lets go of a playing clip」、「every clip a play held is back with its submitter after the submitter's next call」、ThreadSanitizer 與 AddressSanitizer 下的「plays and drops clips while another thread renders」。
- **DI 9**：守衛名從「ClipBank rejects…」改成「Clip rejects clips the mixer cannot play」。
- **DI 11**：拿掉「執行中的 `ClipId` 指一格的一個世代」一句與它的兩個守衛（`ClipId` 不在了）。
- **新增 DI 12（Data Contract）**：`sound`／`wav` 的格式、Version 與 Validation Rule 在 audio 頁新的第 6 節（照本頁第 5 節）；手寫、腳本、工具產生的一樣有效；loader 是純函式、只連 `GYO::Audio`。守衛：每條規則各一案（碼對得上）；master 的 4 個 wav 原樣載得了；標準檔頭寫出再讀回相等。
- **RC 1**：拿掉「clip 必須由擁有它的提交口以外的提交口載入、卸載或播放」（(E) 下任何提交口都能播）；留「需要串流或壓縮格式；或 clip 的記憶體必須在音訊執行緒不跑時回收，或以位元組數設上限」，並補「（原 (D)：混音器只借 asset 的記憶體）」。
- **新增 RC 10**：回收環的取空時點（下一次呼叫）讓記憶體留得太久而超出預算，或需要不經提交口也能收回。

選 (A) 時只改 DI 7 的措辭（「釋放」→「放掉參照」）、DI 9 的守衛名、RC 1 補 (D)；DI 6、DI 11 不動。

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

| 決定 | 系譜 | 退掉的與理由 | 要它的 Feature |
|---|---|---|---|
| 共用的 clip 值、播放帶參照 | Bevy `AudioSource`、Kira `StaticSoundData` | (A)～(D)，第 4.1 節表 | C-1～C-4 |
| 回收環（音訊執行緒不放掉） | Kira unused 環 | 在音訊執行緒放掉（Bevy／rodio〔知〕）：破 DI 7；SoLoud 的 mutex：破 DI 7 | C-2（替換時聲音照響） |
| loader 在 audio、audio 連 asset | Bevy bevy_audio | (ii) 另開函式庫：多一個 Module、型別與特性分兩處 | C-1、C-4 |
| WAV PCM16 48 kHz、載入時解碼 | master `SoundLoader`＋audio_editor 的輸出；Kira／miniaudio DECODE | Bevy 存編碼位元組、播時解碼：破 DI 7；收任何取樣率：破 DI 9 | C-1、C-4 |

**決勝的不是**：既有測試與突變要不要改（是費用，AGENTS §4）。(E) 要改的測試與突變比 (A) 多，寫在費用裡。

## 9. 實作的步驟（使用者說「開始」之後）

0. **基準**：改之前跑 audio 的 3 個 CTest（本機 Intel Mac）與本批會碰到的突變；在 ThreadSanitizer、AddressSanitizer 的手動組態各跑一次 audio 的 CTest（同 #279 的組態）。log 放主 checkout 的 `build/evidence/gyo2-audio-clip-loader/`。
1. **clip（第 1 題）**：`Clip`、`PlayRequest` 帶 clip、回收環、拿掉 `ClipBank`／`ClipId`／`LoadClip`／`UnloadClip`／clip 格。7 個測試檔照下表處理（§14.4）：
   - 設定從 `ClipBank::Add` 換成 `Clip::Make`、判定不變：`MixerTests`、`ScheduleTests`、`StopTests`、`PlayheadTests`、`CharacterizationTests`（**雜湊常數不改**——輸出逐位元相同是 (E) 不改混音的證據）、`RenderAllocTests` 的第一案。
   - `ClipSlotTests.cpp` 換成 `ClipTests.cpp`：14 案逐判定對照（每個判定守的性質留、移或隨機制拿掉，表寫進 dev_log）；隨機制拿掉的：格數上限、`NoClipSlot`、世代、卸載的帳、卸載後的 Programmer Error。
2. **loader（第 5 節）**：`WavLoader`、`WavLoaderCode`、`AssetTypeName<Clip>`；登記檔與 CMake（第 2 題）。
3. **文件**：audio 頁（第 1～3、5 節與 DI／RC，第 7 節）、asset 頁第 1、9 節、`target/v2/README.md` 加本頁；§5.2 的 O-2 引用（4.4）。
4. **突變**（第 10 節）。
5. dev_log、設計審查（Fable／high，唯讀）、合併前核准頁 `audio-clip-loader-merge-approval.zh-Hant.md`、PR（base `gyo2`），等「合併」。

**時間估計**：(E)：程式 6～9 h、loader 3～4 h、文件 3～4 h、突變與 sanitizer 3～4 h、dev_log 與核准頁 1～2 h，共 16～23 h。(A)：共 10～14 h。超過 1.5 倍停下回報。

**模型／檔位**：上段（公開介面、Design Invariants、Data Contract）。實作者 Opus 5.5／high（本卡）；宣告的敵對式檢查、突變清單的敵對式檢查、合併前設計審查 Fable 5.2／high（主會話的 brief 指定）。

## 10. 測試、判定（先宣告；結果出來之後不放寬）

1. **既有行為不變**：第 9 節第 1 步第一項的測試判定不變、全部通過；`CharacterizationTests` 的雜湊常數不改。
2. **不複製**（新）：`Clip` 的複製、`Play` 的呼叫不配置（數配置的配置器，`gyo_audio_alloc_tests`）；asset 載好的 clip 與 voice 讀的取樣是同一塊（`View().samples.data()` 相等）。
3. **音訊執行緒不放掉**（新，DI 7）：外面的持有者在 voice 播放中放掉最後一份參照，`Render` 播到結束都不配置、不釋放；之後提交口的下一次呼叫釋放它（那條執行緒上的釋放數 ≥ 1）。播放的每一種結局（準時、晚、沒有 voice、環滿、停在開始前、停在播放中）各一案。
4. **回收環不會滿**（新）：一個提交口送滿環、voice 全佔、全部結束而提交口不呼叫，再呼叫一次——每一份參照都回來（clip 的共有數回到 1），沒有遺失。
5. **ThreadSanitizer、AddressSanitizer**（本機手動組態）：「plays and drops clips while another thread renders」與既有壓力測試沒有報告；和第 0 步比。
6. **loader**（新，DI 12）：第 5.2 節每條規則各一案、碼對得上；master 的 4 個 wav 原樣載得了（frame 數、聲道數、取樣與檔案位元組相同；檔從 origin/master 3b411e21 複製進測試 fixture，git 物件相同、不增加 repo 大小）；`LIST` chunk 跳過、奇數長度的補位；同樣的位元組兩次同結果、兩條執行緒各 1000 次同結果（AS-3）；經 `AssetStore` 以測試執行器 `Request<Clip>` → `Loaded`，取到的 clip 交給混音器播出預期的取樣。
7. **連結**：`gyo_expect_audio_links` 照第 2 題的結論。

**突變**（AGENTS §14.5，只跑相關的）：
- **改之前**跑本批會碰到的突變當基準。
- **退役**：find 落在拿掉的機制的（22＋2＋1 條裡逐條判；每條寫「隨哪個機制拿掉」），守的性質還在的改寫到新程式。
- **新增**（每條寫只有它捕的誤：`guards` 指 DI 7、DI 12 的條文）：音訊執行緒在 voice 結束時就地放掉 clip（預期第 3 項的配置測試失敗）；沒有 voice 的播放就地放掉（同）；提交口不取回收環（預期第 4 項失敗）；`Clip` 的複製改成深複製（預期第 2 項失敗）；loader：不查位元數、不查格式碼、不查 block align、接受越界的 chunk、接受重複的 chunk、不跳過補位（各預期第 6 項對應的案）。
- 碰到的突變清單受一次別的模型（Fable／high）的敵對式檢查（§14.2）；合併前在背景跑一次完整清單（`mutation-rerun-scope`）。

## 11. 本卡判的（不是使用者的話）

- C++ 型別名 `Clip`（混音器的用詞：`ClipView`）、catalog 型別名照舊 `sound`（P10）；`WavLoader`、`WavLoaderCode`、`WavSettings` 的名字。
- 第 5 節的格式規則與碼、Version 的表示法、64 MiB 的預設上限。
- loader 由產品登記、audio 不另設登記單位。
- 回收環的容量與取空時點（4.1 第 4、5 點）。

## 12. 要使用者定的

**第 1 題：clip 的來源**（推翻 audio 重審宣告第 2 節「clip 的租約與交還：留」的機制，與 master `v8-engine-audio-control` 的 `LoadClip`／`UnloadClip`）
- **(E) 建議**：拿掉 `ClipBank` 與 clip 格；播放帶共用的 clip；音訊執行緒用完經回收環交還（Bevy 的資料模型＋Kira 的釋放）。
- (A)：拿掉 `ClipBank`；`LoadClip` 收共用的 clip、不複製；格與卸載留，`clipSlotsPerSubmitter` 預設改 32。
- (B) 照舊複製、(C) 兩條都留、(D) 借 asset 的記憶體：不建議（第 4.1 節表）。
- 取捨：(E) 作者那一側少三件事、任何 World 都能播，費用是改寫 clip 那一半（估 16～23 h）；(A) 改動約一半，留下格的模型與替換 World 時的卸載。

**第 2 題：loader 的位置**（改 DI 2）
- **(i) 建議**：`GYO::Audio` 連 `GYO::Asset`，loader 與型別特性在 audio（同 Bevy）。
- (ii)：另開小函式庫，DI 2 不變，多一個 Module、作者多記一個標頭。

**第 3 題：和 Bevy 不同的三處照舊**（第 4.3 節）：載入時解碼、只認 WAV PCM16 48 kHz、不做 `AudioPlayer` 式便利層。同意照舊，或要改哪一處。

## 13. 停止條件

- 實作段的測試、判定卡住：AGENTS §14 冒頭的三條。
- 回收環的上限推導在實作時被推翻（壓力測試看到參照遺失或要放掉在音訊執行緒）。
- 第 10 節第 1 項的雜湊常數必須改才通過（代表 (E) 改了混音，不是只改 clip 的來源）。
- 發現 gyo2 上有 audio 的其他使用者（第 6 節第 4 項的掃描有命中）。
