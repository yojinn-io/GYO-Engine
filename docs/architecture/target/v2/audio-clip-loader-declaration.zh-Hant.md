# 事前宣告：audio 那一側的小批（clip 的來源、音效 loader）（`v8-arch-gyo2-audio-clip-loader`）

更新：2026-11-23（修訂 1）。Owner：架構（GYO2 主會話開的任務卡 `gyo2-audio-clip-loader`）。**狀態：草稿，交使用者動工前核准。使用者說「開始」之前不改程式。**

**經過**：
- 修訂 0（d97150df）送 Fable 5.2／high 敵對式檢查：blocker 2、should 11、nit 9（主 checkout `build/evidence/gyo2-audio-clip-loader/declaration-r0-adversarial.md`）。全部採用成**修訂 1**，處理對照在第 15 節。
- 最大的改動：(E) 的內部做法改成檢查 N-4 提的形——**音訊執行緒手上只有 view 與整數**，clip 的參照由提交口這一側的一張表握著，回收環上只走表的格號。DI 7 回到結構保證（音訊執行緒沒有擁有記憶體的物件），回收環不可能滿，修訂 0 的「上限恰好取到」（S-2）與「三處覆寫前必須是空的」（S-3）不再存在。

**性質**：批次表「6. audio 那一側的小批」一列（HANDOFF `v8-arch-gyo2-world-audio-withdrawn`）的事前宣告。`engine/modules/audio` 是 Engine 的公開 Interface，音效的格式是工具與遊戲之間的 Data Contract，照 AGENTS §2 當**基盤**處理（第 3 節）。

**用詞**：標「**卡片判**」的，是這張卡照命名規範、別家原始碼與已定的原則自己判的，**不是使用者的話**。標「**卡片的前提**」的，是這張卡自己加的前提。別家的事實標出處：本機原始碼寫路徑（`~/Code/GitHome/…`）；**〔知〕**＝憑記憶、這次沒讀到原始碼，不當決勝的根據。

**核心一句**（每一項都拿來對照）：**音效是 asset 載好的不可變值，混音器播它、不另存一份；音訊執行緒從不釋放它。什麼時候載、什麼時候放掉自己手上的參照、哪個 World 播，是作者的事；混音器保證它自己握的參照最晚在那個提交口下一次被呼叫時放掉。**

---

## 0. 讀的版本

- 程式：`origin/gyo2` 641a1f26 的 `engine/modules/audio`（`Mixer`、`ClipBank`、`VoiceMixer`、`ClipValidation`、`PlayheadBoard`）、`engine/modules/asset`（`AssetLoader`、`LoadContext`、`AssetTypeName`、`TextLoader`、`ReadWholeFile`）、`engine/modules/world_asset`（`AssetView::Get` 回 `const T*`）、`tests/common/audio`（7 個檔；`ClipSlotTests.cpp` 15 案：第 78、103、120、136、156、171、181、215、227、254、273、292、317、338、348 行）。
- 合約：[audio 頁](audio.zh-Hant.md)（DI 1～11、RC 1～9、第 5 節作者的寫法）；[asset 頁](asset.zh-Hant.md)（AS-2、AS-3、AS-9、AS-10、第 3 節 Validation Rule (B)、第 5 節 loader 介面）；[asset 宣告](asset-declaration.zh-Hant.md) 5.10、5.16；[audio 重審宣告](audio-review-declaration.zh-Hant.md) 第 2、4.2、5、14 節；`docs/cross-component-rules.md` CCR-6、CCR-9；HANDOFF `v8-arch-gyo2-world-audio-withdrawn`（`HANDOFF.md:271`）。
- 突變清單：`docs/architecture/plans/audio/scripts/mutations.json`（78 條）、`mutations-asan.json`（1）、`mutations-tsan.json`（7）。這一批會碰到的（`cases` 在 `ClipSlotTests.cpp`、或 find 在要拿掉的程式）：`au-clip-rate-unchecked`、`au-review-handoff-rebound`、`au-review-unload-counts-before-assert`、`au-ctl-clip-handback-ignores-voices`、`au-ctl-clip-lease-not-ended-render`、`au-ctl-clip-lease-not-ended-before-start`、`au-ctl-clip-lease-not-ended-late-stop`、`au-ctl-clip-unload-flag-not-reset`、`au-ctl-clip-reuse-without-check`、`au-ctl-clip-generation-not-bumped`、`au-ctl-unload-uncounted`、`au-ctl-unload-marks-on-full`、`au-ctl-clip-handback-ignores-count`、`au-ctl-ended-not-cleared`、`au-ctl-ended-capacity-one`、`au-ctl-reuse-ended-comes-back`、`au-ctl-alloc-in-render`、ASAN 的 `au-ctl-clip-reuse-without-check-asan`、TSAN 的 `au-ctl-clip-handback-relaxed`、`au-ctl-clip-reclaim-relaxed`。另外 find 落在 `Mixer.cpp` 取命令的迴圈、`Command{…PlayRequest{ClipId{}, at}…}`、`ClipBank` 的設定的，改寫後會失配，實作第 0 步用執行器的「find 不中」逐條列出。
- master（只看為什麼，不拿數據當設計）：`engine/asset/src/loaders/SoundLoader.cpp`（WAV PCM16、不查取樣率、查奇數長度 `:78-80`、沒有測試）；`docs/architecture/plans/audio/HANDOFF.md:126` `v8-engine-audio-clip-format`（`ClipBank` 複製；不讓 Audio 依賴 Engine 的理由是「離線測試要連整個 Engine」）；`docs/tools/plans/v8/assets.md:53-56`（Data Contract 是「48 kHz PCM16 的 WAV＋catalog 條目」）；`docs/audio_editor/architecture.md` DI 2；`tools/audio_editor/src/WavFile.cpp:124-154` `EncodePcm16Wav`（44 位元組檔頭、格式碼 1）；`assets/object_fps_pvp/asset_catalog.json:19-22`（4 筆 `"type": "sound"`、`.wav`、不寫 `format`）。
- 別家：
  - **Bevy**（主參考；`~/Code/GitHome/bevy/crates/bevy_audio`）：`audio_source.rs:7-20` `AudioSource { bytes: Arc<[u8]> }` 是 Asset，複製只加參照計數；`audio_source.rs:39-75` `AudioLoader`（整檔讀進來不解碼，認 wav／ogg／flac／mp3 看 feature）；`lib.rs:97-101` `AudioPlugin::build` 自己 `init_asset_loader::<AudioLoader>()`；`Cargo.toml:13-18` bevy_audio 依賴 bevy_app、bevy_asset、bevy_ecs、bevy_transform——它是**接 ECS 的那一層**，混音的是第三方 rodio，rodio 不認得 bevy_asset；`audio_output.rs:84-112` 播放的 System 從 `Assets<Source>` 取值，沒載好就下一幀再試，取到就 `decoder()`（`audio_source.rs:94-100`，把 `AudioSource` 的複製交給 rodio、在混音那一側解碼）。rodio 在哪條執行緒放掉那份複製〔知：混音執行緒〕。
  - **Kira**（`~/Code/GitHome/kira`）：`sound/static_sound/data.rs:23-32` `StaticSoundData { frames: Arc<[Frame]> }`，載入時解碼、複製只加參照計數；音訊執行緒不釋放：用完的資源推進「unused」環（`backend/resources.rs:131-170`，環滿時留在原處），遊戲那一側在下一次加入資源（`play`）時取出、drop（`:207-222`）；生產端先預約位置、滿了回 `SoundLimitReached`（`:195-205`、`track/main/handle.rs:17-28`）。Bevy 生態的 bevy_kira_audio 用 Kira〔知，本機沒有它的原始碼；不當決勝的根據〕。
  - **SoLoud**（`~/Code/GitHome/soloud`）：`Wav` 載入時解碼、實例讀父物件的資料不複製；刪 `AudioSource` 時先 `stop()` 所有實例（`src/core/soloud_audiosource.cpp:208-211`），靠和混音共用的 mutex——GYO 的音訊執行緒不上鎖（DI 7），不採。
  - **miniaudio**（`~/Code/GitHome/miniaudio/miniaudio.h:10324-10465`）：resource manager 以名字共用 data buffer、參照計數；`MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_DECODE` 在資源管理那一側解碼、不在混音執行緒（`:10332`）。
  - **沒有一家有「出聲前登記的庫」或「每個提交口各自載一份」**：共用一份、播放時參照它。

## 1. 先用遊戲講

**FPS 的 Client 開槍。** 進對局前，作者 `Load<Clip>(AssetId("object_fps_pvp.audio.gun_shoot"))`；asset 在背景讀 `audio/gun_shoot.wav`、照 WAV 的規則變成一個不可變的 clip。第 312 幀開槍，表現層的 System 從 `AssetView` 拿到那個 clip，放進要交給混音器的請求；混音器播它，**不另存一份**。兩個 World 播同一個槍聲，記憶體裡也只有一份。

**讀存檔換 World。** 新 World 請求同樣的 id，asset 命中；舊 World 已送出、還在響的槍聲照響完。今天的寫法（audio 頁 5.2）要作者記得「替換前卸掉舊 World 載進提交口的 clip」，否則幾次替換後 `NoClipSlot`；建議的 (E) 之後沒有這件事。

**音效檔換了版本。** 作者用 audio_editor 重新匯出 `gun_shoot.wav`（48 kHz PCM16），catalog 的 id 不變；手寫、腳本產生的同格式檔一樣有效（AGENTS §8）。檔是 44.1 kHz 的話，載入失敗：碼是 `InvalidClip`，訊息說「a clip is 48000 Hz」；World 照跑（asset 不記日誌、不重試，AS-8）。

**audio_editor 預覽。** 工具把剪輯後的取樣做成一個 clip（不經 asset）交給混音器播；換一段再做一個。舊的 clip 在最後一個播放結束、工具放掉它、而且那個提交口又被呼叫過一次之後釋放。

## 2. 前提表

每一項標：**Engine**（只有 Engine 做得到、要在這批定）或**作者**（現有機制就做得到，Engine 只寫清楚）。

| # | 前提 | Engine／作者 | 誰定的 | 掛在它上面的 |
|---|---|---|---|---|
| P1 | 音效是 asset 載好的值：不可變、多執行緒同時讀、任何執行緒解構（AS-10、CCR-9） | Engine（asset） | 使用者 11-22 核准 asset 宣告；#283 合併 | 4.1 共用不複製 |
| P2 | 音訊執行緒不配置、不釋放（DI 7） | Engine（audio） | master 起；#279 照舊 | 4.1 的表與回收環 |
| P3 | 混音器只吃 48 kHz、1～2 聲道的 PCM16，不重新取樣（DI 9） | Engine（audio） | master `v8-engine-audio-clip-format`；#279 照舊 | 第 5 節的格式 |
| P4 | loader 歸擁有那個型別的 Module（asset 5.16、AS-2） | Engine（asset） | 使用者 11-22 | 4.2 位置 |
| P5 | 工具寫、遊戲讀同一個格式 → 放在共通層、是 Data Contract（AGENTS §6、§8） | Engine | AGENTS；`v8-arch-gyo2-world-audio-withdrawn` | 第 5 節 |
| P6 | 把 World 接到混音器（提交口放哪、哪個 System 交、時刻從哪來）是作者的寫法（audio 頁第 5 節） | 作者 | 使用者 11-22「照建議」 | 不在本批；5.2、5.4、5.5 的字隨 4.1 改 |
| P7 | 什麼時候載、等不等載完、什麼時候放掉 Handle 與自己手上的 clip | 作者（AS-7） | asset | 4.1 |
| P8 | 同一個聲音被哪個 World、哪個提交口播、播幾次 | 作者 | **卡片的前提** | 4.1 |
| P9 | **clip 的壽命綁在載它的提交口、替換 World 時要作者卸載**：Engine 的坑，還是作者的事 | 11-22 判**作者的**（`v8-arch-gyo2-world-audio-withdrawn` 第 2、6 件；audio 頁 5.2「clip 格不跟著沿用」、5.5）。**本批提議收回 Engine**（「自己的 Module 不帶作者踩坑」，同一筆的理由） | 使用者 11-22／23 | 第 1 題 |
| P10 | 串流、壓縮格式、重新取樣 | 沒有點名的 Feature | RC 1、DI 9 | 不做 |
| P11 | catalog 的型別名是 `sound` | Engine（Data Contract） | master 的內容與 audio_editor（`SoundExport.hpp` 寫死 `"sound"`）；**卡片判**：照舊 | 第 5 節 |
| P12 | 混音器在 `Configure` 建時 asset 還沒載好，所以 `ClipBank` 填不進遊戲的音效 | **卡片的前提**（共用處不凍結，也可以晚建；但晚建也加不進之後才載的音效） | audio 頁 5.2「通常」 | 4.1 (C) 列 |

## 3. 基盤的根據（AGENTS §2）

**為什麼算基盤**：積在上面的有 SDL 的輸出後端、每個產品的聲音、audio_editor 的預覽與匯出、master 的 4 個音效檔與日後的所有音效內容；clip 的介面與檔案格式一改，這些都要改。

| # | 根據（出處） | 種類 | 要求 |
|---|---|---|---|
| C-1 | ① FPS Client 的槍聲、受擊、換彈、腳步經 asset 載入（asset 宣告 F1：wav 4 個） | 名指的 Feature | `sound` 型別的 loader；載好的值不複製就能播 |
| C-2 | ① 讀檔換 World（O-8） | 名指的 Feature | 已送出的聲音照響完（今天的合約已是如此：`UnloadClip` 排在已送出的播放之後，`Mixer.hpp:153-155`） |
| C-3 | ①② 多個 World 用同一個音效（asset：兩個 World 同一 id 只載一次） | 名指的 Feature | 記憶體一份 |
| C-4 | ③ audio_editor 的匯出與預覽（批次表第 112 行：搬回時照新契約） | 名指的 Feature（工具） | 工具寫的檔遊戲讀得了；工具自己做的取樣也能播 |

**C-1～C-4，第 1 題的 (A) 與 (E) 都滿足。** 選 (E) 的理由是機制少、作者那一側少事、不保守（4.1），不是 Feature 逼的。

**規模**：點名的內容最大 184 KB（`gun_reload.wav`），4 個共 349,800 位元組。

## 4. 判定

### 4.1 clip 的來源（`ClipBank` 該不該存在、共用或複製）——第 1 題

**判定（建議 (E)）**：拿掉 `ClipBank` 與提交口的 clip 格；clip 是一個**共用取樣的不可變值**，**每次播放帶著它**（Bevy、Kira 的資料模型）。釋放：混音器握的那份參照放在**提交口這一側的一張表**裡，音訊執行緒只拿 view 與表的格號，用完把格號經回收環還回去，提交口下一次被呼叫時清掉那一格。

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

- clip 的規則在 `Clip::Make`（`ClipValidation` 的四條：空的、聲道數不是 1 或 2、樣本數不是整數個 frame、取樣率不是 48000，都是 `InvalidClip`）；**加一條**：frame 數不超過 2^32 − 1（`ClipView::frames` 與 voice 的位置是 32 位元，今天沒查，`ClipBank.hpp:25`、`VoiceMixer.hpp:127`）。位元組層的規則在 loader（第 5 節）。
- `Mixer::Create(const MixerConfig&)`：不再收 `ClipBank`。`MixerConfig::clipSlotsPerSubmitter` 拿掉。
- 拿掉：`ClipBank`、`ClipId`、`Submitter::LoadClip`、`Submitter::UnloadClip`、`AudioErrorCode::NoClipSlot`（5 不重編，照 4 的先例）、`MixerStats` 的 `unloads*` 三個計數、`ClipSlot`（格、世代、卸載）。
- 留：`ClipView`（搬到 `Clip.hpp`）、`SoundHandle`、`Stop`、`StopAll`、`PlayheadOf`、其餘計數、時間點換算、`VoiceMixer` 的混音與它的「租約標記」管道（`ScheduledPlay::view`／`lease`、`Ended()`，標記改指表的格）。

**做法**（每個提交口；全部在建混音器時配好）：
1. **參照表**：`queueCapacity + maxVoices + 1` 格的 `Clip`，加一個空格的堆疊。只有使用提交口的一方碰它（和今天提交口私有的欄位一樣，換手照第 3 節的執行緒契約）。
2. **`Play`**：先取空回收環（第 4 點），再從堆疊拿一個空格、把請求的 clip 複製進去（參照計數加一，在使用提交口的一方），命令裡只放那一格的 view 指標與格號。堆疊空了（照下面的界不會發生）就和環滿一樣丟掉並記數（`droppedQueueFull`）、回無效的 handle——在生產端，安全地處理，不靠推導。
3. **音訊執行緒**：照今天的租約管道——拿到 voice 的播放，格號跟著 voice；沒有 voice 的、voice 結束的（播完、停止、還沒開始就被停），把格號推進那個提交口的**回收環**（`uint32` 的 SPSC 環，release／acquire 同命令環）。音訊執行緒手上只有 view 與整數，**沒有擁有記憶體的物件**。
4. **取空**：使用提交口的一方在**每個成員呼叫**（`Play`、`Stop`——含無效 handle、`StopAll`）開頭、與 `~Submitter` 裡，把回收環取空：每個格號的那一格清掉（最後一份參照可能在這裡放掉）、格號放回堆疊。
5. **界**：回收環與參照表一樣大。一個格號同時只在一處（堆疊、命令環、voice、回收環之一），所以回收環**不可能滿**——結構保證，不需要「恰好取到」的推導。表不會用光：每次 `Play` 先取空，之後佔用的格只在命令環（≤ `queueCapacity`）或先前 block 的 voice（≤ `maxVoices`），所以至少剩一格（修訂 0 檢查「確認成立的」第 1 項的推導；堆疊空時第 2 點的處理是保險，不是正確性的前提）。
6. **限界**（寫進 Contract）：混音器握的參照最晚在那個提交口下一次被呼叫時放掉；不再被呼叫的提交口，它的格留到混音器的狀態解構（上限：表的大小）。沒有 `Render`（後端沒開）時，排著的請求的格留在環裡，直到混音器的狀態解構。`Play`、`Stop` 可能在呼叫的執行緒上釋放大塊記憶體（表現層 System 的那一幀）。

**比過的選項**（audio 重審宣告第 5 節的 A～D，加本卡的 E）：

| 選項 | 內容 | 後果 | 和 Bevy |
|---|---|---|---|
| **(E) 建議** | 拿掉 `ClipBank` 與 clip 格；播放帶共用的 clip；混音器的參照放在提交口這一側的表，音訊執行緒只還格號 | 一條路、記憶體一份；**不必逐提交口載入與卸載、沒有格數要配、沒有 `NoClipSlot`**；audio 頁 5.2「clip 格不跟著沿用」、5.4 的 `LoadClip` 值、5.5「先送出緩衝再卸載」三件作者要記的事消失。DI 7 仍是結構保證（音訊執行緒手上只有 view 與整數）。**代價**：改寫 clip 那一半（`Mixer.cpp`、7 個測試檔的設定、`ClipSlotTests.cpp` 換成 clip 壽命的測試、上面 20 條突變退役或改寫）；**作者沒有主動放掉的入口**——放掉自己手上的 clip 之後，實際釋放等那個提交口下一次被呼叫（第 6 點；要不要給一個只取空的入口，見第 12 節第 1 題）；釋放落在使用提交口的執行緒；`PlayRequest` 不再是可平凡複製的型別 | **資料模型相同**（Bevy `Arc<[u8]>`／Kira `Arc<[Frame]>`）。偏離：Bevy 把參照交給 rodio、在混音那一側放掉〔知〕；Kira 經 unused 環把整個資源還回來。GYO 還的是格號、參照留在生產端。**衝突點**：DI 7。**膠水**：每個提交口一張參照表、一個空格堆疊、一個整數回收環、呼叫時取空的迴圈；拿掉的格、世代、卸載、卸載計數比它多 |
| (A) | 拿掉 `ClipBank`；`LoadClip` 收共用的 `Clip`、不複製；格與卸載照舊 | 記憶體一份；任何 World 也都能播，但要各自 `LoadClip`（兩個 World 播同一音效各佔一格）；`clipSlotsPerSubmitter` 預設 0 要改（照 asset 非同步載入，第一次 `LoadClip` 就 `NoClipSlot`；建議改 32，和 `maxVoices` 預設同）；audio 頁 5.2、5.5 的坑留著；沒有 `Render` 就不回收格。作者有 `UnloadClip` 這個明確的放掉動作。改動比 (E) 小（約一半） | 偏離：Bevy／Kira 沒有「每個提交口載一份」。**膠水**：格、租約、卸載、世代、`NoClipSlot`，作者還要寫替換時的卸載（5.2） |
| (B) | 拿掉 `ClipBank`；`LoadClip` 照舊複製 | 同 (A) 的格的後果；每個音效在 asset 與混音器各一份、兩套有效期 | 偏離更多（複製） |
| (C) | 兩條都留 | 同一件事兩條路；`ClipBank` 填不進之後才載的音效（P12） | 偏離（Bevy 沒有出聲前的庫）；不合「能拿掉的就拿掉」 |
| (D) | 混音器只借 asset 的記憶體（view），asset 在音訊執行緒還在讀時不得釋放 | asset 要知道混音器的租約（跨 Component 的狀態，AGENTS §3 State Authority） | 偏離。(E) 用共用所有權解掉同一件事；「clip 的記憶體必須在音訊執行緒還在讀時被收回、或以位元組數設上限」本來就是 RC 1，照舊記在那裡 |

**(E) 內部的兩種做法**（使用者不必選，卡片判；寫出來照 AGENTS §4 的系譜）：
- (E-1，修訂 0)：命令環與回收環上走 `shared_ptr`，音訊執行緒搬動參照。退掉：音訊執行緒手上有擁有記憶體的物件，DI 7 變成「voice 重用、回收環的一格、暫存物件三處覆寫前一定是空的」的紀律；回收環的容量是恰好取到的上限，滿了音訊執行緒沒有安全的處理（修訂 0 檢查 S-1 第 4 點、S-2、S-3）。
- **(E-2，採用)**：上面的做法（檢查 N-4 提的形）。DI 7 照今天一樣靠結構；回收環不會滿；表用光時在生產端處理。

**為什麼建議 (E)**：
1. (A)(E) 都做到「播它、不另存一份」「任何 World 都能播」「記憶體一份」；差別是 (E) 不必逐提交口載入與卸載、沒有格數。
2. 不堆屎山：(E) 拿掉的機制（格、世代、卸載、`NoClipSlot`、卸載計數）比加上的（參照表、整數回收環）多，作者那一側少三件要記的事。
3. 主參考：資料模型和 Bevy 相同；唯一的偏離（參照留在生產端、只還格號）是 DI 7 要求的。
4. `gyo2-mechanism-not-conservative`：(A) 是保守的小改，格的模型會被上面的產品與 audio_editor 積上去，之後不會有人回頭拿掉。

**推翻的舊決定**（要使用者同意，AGENTS §3）：
- audio 重審宣告第 2 節「clip 的租約與交還：留，DI 7 的核心」——性質（音訊執行緒不釋放）與租約管道留；交還的單位從「clip 格」換成「播放的參照格」。
- master `v8-engine-audio-control` 的 `LoadClip`／`UnloadClip`（為 audio_editor 預覽做的）——預覽的需求（C-4）由 (E) 直接滿足。
- audio 頁 5.2「clip 格不跟著沿用」、5.4 的 `LoadClip`／`NoClipSlot`、5.5「先送出緩衝再卸載」（`v8-arch-gyo2-world-audio-withdrawn` 第 2、6 件判成作者的；P9）——(E) 把這兩件收回 Engine，三處文字拿掉或改寫。

**RC 的狀態**（AGENTS §3：成立時交使用者）：RC 1「clip 必須由擁有它的提交口以外的提交口……播放」——(E) 等於主動走進這一條，第 1 題就是「修還是重建」的答案；RC 6「每個新的播放功能都要重新驗證……clip 的回收、即時安全」——這一批的 clip 回收與即時安全全部重驗（第 10 節）。

**`clipSlotsPerSubmitter` 預設 0**：(E) 下這個設定拿掉，問題消失；選 (A) 時改成 32。

### 4.2 音效 loader 放在哪裡——第 2 題

**判定（建議 (i)）**：loader 與 `AssetTypeName<Clip>` 放在 `GYO::Audio` 裡，`GYO::Audio` 連 `GYO::Asset`（登記檔 `audio` 列 `depends=asset`，列移到 `asset` 之後；O-6）。

| 選項 | 內容 | 後果 | 和 Bevy |
|---|---|---|---|
| **(i) 建議** | `GYO::Audio` 連 `GYO::Asset`；`Clip.hpp` 帶型別特性，`WavLoader.hpp` 是 loader | 一個 Module 擁有型別、規則與 loader（asset 頁第 1 節「擁有型別的 Module：loader 與型別特性的函式庫只依賴 asset」）。改 DI 2 的連結（Base、Time → Base、Time、Asset）與 audio 頁第 1 節「不認得 asset」。用混音器的程式都會連到 asset（連帶私有的 io、json、Threads，`asset/CMakeLists.txt:30-31`）；用混音器而不用 asset 的使用者今天沒有（audio_editor 兩者都用） | **Module 層級相同**：第一方的 audio 單位持有型別與 loader、依賴 asset（bevy_audio，`Cargo.toml:14`、`audio_source.rs:39`）。**不同**：Bevy 的混音核心（rodio）不認得 asset，bevy_audio 是接 ECS 的那一層；GYO 沒有那一層（11-22 撤掉 world_audio），所以混音器本體連上 asset。**衝突點**：沒有中間層可放。**膠水**：無；代價是混音器的使用者都連到 asset |
| (ii) | 另開一個小函式庫（例 `engine/modules/audio_asset`，`GYO::AudioAsset`），連 Audio 與 Asset；`Clip` 在 audio、型別特性與 loader 在它 | DI 2 不變；多一個 Module 目錄、登記檔一列；`Clip` 與它的型別特性分在兩個 Module（只引 `Clip.hpp` 時 `Load<Clip>` 編譯錯誤，要記得引另一個標頭） | crate 的切法較像 Bevy（混音器不認得 asset，另一層連兩者），但 Bevy 把型別也放在那一層。**膠水**：一個目錄、一列登記、一份 CMake，作者多記一個標頭 |

master 當年不讓 Audio 依賴 Engine 的理由（「離線測試要連整個 Engine」，`v8-engine-audio-clip-format` B）在 gyo2 不成立：asset 是不認得 ECS 的小函式庫（`library_only`），audio 的測試照樣離線、不開裝置。

### 4.3 和 Bevy 不同的地方——第 3 題（一起確認）

| # | 和 Bevy 不同 | GYO | 理由 | 碰到哪條 |
|---|---|---|---|---|
| 1 | Bevy 的 asset 存**編碼過的位元組**，播放時在混音那一側解碼 | 載入時變成取樣（Kira、SoLoud、miniaudio 的 DECODE 同） | 音訊執行緒不配置（DI 7）；解碼在混音那一側要配置 | DI 7 |
| 2 | Bevy 認 wav／ogg／flac／mp3（看 feature）、任何取樣率 | 只認 WAV PCM16、48 kHz、1～2 聲道；其他格式由工具事先轉 | 不重新取樣（DI 9）；壓縮格式要第三方解碼器，沒有點名的 Feature（RC 1；audio_editor 的 RC「Formats other than WAV must be read」） | DI 9、RC 1 |
| 3 | Bevy 由 `AudioPlayer` 元件與播放的 System 等載好再播 | 作者在表現層 System 用 `AssetView` 取 clip 再交（audio 頁第 5 節） | 使用者 11-22 不做 Bevy 式便利層（`v8-arch-gyo2-world-audio-withdrawn` 第 2 件） | — |
| 4 | **Bevy 的 `AudioPlugin` 自己登記 loader**（`lib.rs:97-101`） | **產品自己登記一行** `registry.Register<WavLoader>()`（audio 是 `library_only`、沒有登記單位）。**後果**：忘了登記不是啟動錯誤，是請求時的 `Failed(NoLoader)`（asset 頁第 3 節 (B) 第 3 列「啟動時不報」） | 給 audio 一個登記單位就是 world_audio 那一層，11-22 撤掉；SDL 那批建混音器的登記單位（audio 頁 5.2、第 4 節）出現時，可以由它順手登記 | — |

**第 4 處的選項**：(a) 產品登記，SDL 那批若有建混音器的單位再改由它登記（建議）；(b) 現在就給 audio 一個只登記 loader 的單位（等於重開一小塊 world_audio）。

### 4.4 順手補：audio 頁 §5.2 引 O-2 新補的半句

§5.2「根據」一段，O-2 那一句改成引原文：「登記單位以 World 名為鍵記的資料在 `Build` 時重設（不含 O-15 共用處裡跨 World 的物件——它們比每個 World 活得久，例：照 World 名沿用的提交口）」（`orchestration.zh-Hant.md` O-2，`v8-arch-gyo2-o2-shared-note`），刪掉「主會話 2026-11-23 確認」那半句的推論。HANDOFF `v8-arch-gyo2-audio-o2-cite-deferred`。

## 5. 音效的 Data Contract（`sound`／`wav`）——卡片判（照 master 與 AGENTS §8）

**誰產生、誰讀**：audio_editor 寫（搬回時照這裡）、Script（master 的 `convert_wav.py` 類）、手工（任何能輸出 48 kHz PCM16 WAV 的軟體），一樣有效；遊戲經 asset 讀，執行期讀、不需要 build 的產物；工具要驗證自己的輸出就呼叫同一個 loader，並比大小（5.2 規則 8：大小由 store 判，直接呼叫 loader 驗不到）。

### 5.1 catalog 的項目

```json
{"id": "object_fps_pvp.audio.gun_shoot", "type": "sound", "path": "audio/gun_shoot.wav"}
```

- `type`：`sound`（`AssetTypeName<Audio::Clip>`）。
- `format`：`wav`（`path` 的副檔名推得出，可省；asset 頁第 3 節）。
- `settings`：沒有；寫了照收、忽略（照文字 loader）。

### 5.2 檔案格式（`wav` 第 1 版）

RIFF/WAVE，小端序。**照下表的順序判，回第一條違反的碼**（loader 是純函式，先後是合約的一部分；同時違反多條時回編號最小的）：

| # | 規則 | 碼（`WavLoaderCode`） |
|---|---|---|
| 1 | 至少 12 位元組，開頭是 `RIFF`、第 8～11 位元組是 `WAVE` | `NotWave` |
| 2 | 從第 12 位元組起是一串 chunk（4 位元組 id＋4 位元組長度＋內容，長度是奇數時補 1 位元組）。每個 chunk 的內容都在檔案內；最後一個 chunk 的補位可以缺；最後剩不到 8 位元組的尾巴忽略（照 master）。RIFF 檔頭的長度欄不查（寫錯的軟體常見） | `Truncated` |
| 3 | `fmt ` 與 `data` 各恰好一個（先後不限）；其他 chunk（`LIST` 等）跳過 | `MissingChunk`、`DuplicateChunk` |
| 4 | `fmt ` 至少 16 位元組 | `BadFormatChunk` |
| 5 | 格式碼 1（整數 PCM）、每樣本 16 位元 | `NotPcm16` |
| 6 | 聲道數 1 或 2；block align 等於聲道數 × 2 | `BadFormatChunk`（聲道數不對時 detail 寫聲道數） |
| 7 | `data` 的長度是 2 的倍數（master `SoundLoader.cpp:78-80`；奇數時最後一個位元組不能變成樣本） | `PartialSample` |
| 8 | 檔案不超過上限：預設 64 MiB，產品可用 asset 的 `byteLimits` 依（`sound`、`wav`）覆寫（由 store 讀檔時判，碼是 asset 的 `TooLarge`） | asset `TooLarge` |
| 9 | `Clip::Make` 的規則（取樣率 48000、`data` 非空、樣本數是聲道數的整數倍、frame 數 ≤ 2^32 − 1），失敗原樣帶出（detail 是 `Clip::Make` 的訊息，例「a clip is 48000 Hz」） | `InvalidClip` |

- 取樣是有號 16 位元、小端序、聲道交錯，照位元組原樣變成 clip（不抖動、不換算）；逐樣本組合（不依賴主機的位元組序）。`fmt ` 的 byte rate 不查。
- **64 MiB 的根據**：卡片判，沒有 Feature 推得出（點名的內容最大 184 KB）。48 kHz 立體聲 PCM16 約 5.8 分鐘，容得下一般的音效與短的音樂；產品要更大或更小就覆寫。工具匯出時自己比這個數字（audio_editor 的讀檔上限是**來源檔**的上限，和輸出無關）。
- **Version**：WAV 沒有 GYO 的版本欄。`wav` 的版本記在 audio 頁的格式節（今天第 1 版）。**放寬**（多收：例 `WAVE_FORMAT_EXTENSIBLE` 裡的 PCM16）時版本加一、`format` 名不變：舊執行檔拒收用到新功能的檔（例 `NotPcm16`），內容與執行檔一起出貨，不另設協商；工具預設只寫第 1 版的寫法。**收窄**、或改變已收內容的意思，用新的 `format` 名登記另一個 loader（AS-9；例：日後的引擎格式）。
- **標準寫法**（工具、腳本照這個寫）：44 位元組檔頭（`RIFF`、`WAVE`、16 位元組的 `fmt `、`data`），即 master `EncodePcm16Wav` 的輸出。

### 5.3 loader

```cpp
class WavLoader final {                  // engine/audio/WavLoader.hpp
public:
    using Output = Clip; using Settings = WavSettings; using Code = WavLoaderCode;
    static constexpr std::string_view kTypeName = "sound";
    static constexpr std::array<std::string_view, 1> kFormats{"wav"};
    static constexpr std::size_t kDefaultMaxBytes = std::size_t{64} * 1024u * 1024u;
    // ParseSettings：任何 settings 照收、忽略。Load：照 5.2；位元組→取樣複製一次，交給 Clip::Make(vector&&)、不再複製；不宣告引用
};
```

- **登記**：產品在 `Configure` 經 `AssetLoaderRegistry::Register<WavLoader>()`（第 3 題第 4 處）。
- 純函式（AS-3）：沒有 World、沒有時鐘、沒有全域可變狀態；同樣的位元組得到同樣的 clip。`Clip` 合 AS-10 (c)／CCR-9（沒有 `mutable`、參照計數是原子的、解構不綁執行緒）；「不在音訊執行緒解構」由混音器自己守（4.1）。

## 6. Architecture Delta（AGENTS §3 的七項）

1. **為什麼要變**：批次表「audio 那一側的小批」；Feature C-1～C-4。
2. **現在的問題**：(a) `ClipBank` 填不進之後才載的音效（P12）；(b) `LoadClip` 再複製 asset 已有的不可變值；(c) clip 綁在載它的提交口，作者要在替換 World 時卸載（audio 頁 5.2）；(d) `clipSlotsPerSubmitter` 預設 0，第一次 `LoadClip` 就 `NoClipSlot`；(e) 沒有 `sound` 的 loader，工具與遊戲之間沒有寫下來的格式。
3. **變的邊界**：`GYO::Audio` 的公開介面（4.1 的拿掉與新增）；DI 2 的連結（第 2 題 (i)）；新的 Data Contract（第 5 節）。
4. **影響**：audio 與它的測試、突變清單；audio 頁；asset 頁第 1 節「不在範圍：…音效」與第 9 節；`config/modules.csv`（audio 列的依賴、順序與 description "The audio mixer"）；`engine/modules/audio/CMakeLists.txt:1-3` 與 `tests/common/audio/CMakeLists.txt:33-36` 的註解（"no asset"）；`tests/common/audio/CMakeLists.txt` 的連結檢查；`docs/cross-component-rules.md` CCR-9 的實作欄（`Clip` 是第二個 loader 值；AGENTS §1 要求引用並更新，登記頁的 owner 是圖書管理員——本批在同一個 PR 改那一欄並在回報裡告知，或交圖書管理員，照主會話的指示）。gyo2 上沒有別的程式用 audio：`git grep -l "engine/audio/" -- engine examples tests ':!engine/modules/audio' ':!tests/common/audio'` 沒有命中（終了碼 1）；拿掉兩個排除時命中 `engine/modules/audio` 的 7 個檔（陽性對照）。641a1f26，2026-11-23；實作前再跑一次。
5. **依賴方向**：新增 `GYO::Audio → GYO::Asset`（第 2 題 (i)）；都是 Engine 的 Module、往下，沒有循環（asset 不認得 audio）。
6. **Ownership**：`sound` 型別、它的格式與 loader 歸 audio（asset 5.16）。clip 的取樣由所有持有者共有；混音器握的參照由使用提交口的一方放掉，音訊執行緒不握參照。
7. **為什麼不能更小**：(A) 是更小的一步（第 1 題），但留下格的模型與作者那一側的三件事；只加 loader 不動 `ClipBank`，遊戲的音效還是要複製進格、還是預設 0 格。

## 7. Design Invariants 與 Replacement Criteria 的改動（草案；最終文字在合併前核准）

- **DI 2**（第 2 題 (i)）：連結恰好是 `GYO::Base`、`GYO::Time`、`GYO::Asset`；守衛 `gyo_expect_audio_links` 跟著改。「不含 SDL」不變。第 1 節「不認得 SDL、asset、World、產品」改成「不認得 SDL、World、產品；認得 asset 的 loader 介面」。
- **DI 6**：拿掉卸載的帳（`unloadsSubmitted` 一句與守衛「may unload again after its queue was full」）。
- **DI 7**：「一個 clip 的樣本在 voice 或排著的播放可能讀它時不變」改成：clip 的取樣不可變（`Clip` 沒有改它的操作）；混音器為每個排著或播放中的請求在提交口這一側的表裡握一份參照，**音訊執行緒不握參照**（只有 view 與格號），用完經回收環還格號；使用提交口的一方在下一次呼叫或 `~Submitter` 時放掉那份參照，或隨混音器的狀態放掉。守衛見第 10 節第 2～5 項。
- **DI 9**：守衛名從「ClipBank rejects…」改成「Clip rejects clips the mixer cannot play」。
- **DI 11**：拿掉「執行中的 `ClipId` 指一格的一個世代」一句與它的兩個守衛（`ClipId` 不在了）。
- **新增 DI 12（Data Contract）**：`sound`／`wav` 的格式、Version 與 Validation Rule 在 audio 頁新的第 6 節（照本頁第 5 節）；手寫、腳本、工具產生的一樣有效；loader 是純函式、只連 `GYO::Audio`。守衛：第 10 節第 6 項。
- **RC 1**：拿掉「clip 必須由擁有它的提交口以外的提交口載入、卸載或播放」（(E) 下任何提交口都能播）；留「需要串流或壓縮格式；或 clip 的記憶體必須在音訊執行緒不跑時回收，或以位元組數設上限」，補「（原 (D)：混音器只借 asset 的記憶體）」。
- **新增 RC 10**：「最晚在下一次呼叫時放掉」讓記憶體留得太久而超出預算，或作者需要不經 `Play`／`Stop` 也能讓混音器放掉。

選 (A) 時只改 DI 7 的措辭（「釋放」→「放掉參照」）、DI 9 的守衛名、RC 1 補 (D)；DI 6、DI 11 不動。

## 8. 系譜、退掉的選項、決勝理由（AGENTS §4）

| 決定 | 系譜 | 退掉的與理由 | 要它的 Feature |
|---|---|---|---|
| 共用的 clip 值、播放帶參照 | Bevy `AudioSource`、Kira `StaticSoundData` | (A)～(D)，第 4.1 節表。C-1～C-4 兩者都滿足，決勝是機制少與不保守 | C-1、C-3、C-4（(A) 也滿足） |
| 參照留在生產端的表、回收環只走格號 | 今天的租約管道（`VoiceMixer` 的 `lease`／`Ended()`）；檢查 N-4；Kira 的「遊戲那一側才 drop」 | (E-1) 環上走 `shared_ptr`：音訊執行緒手上有擁有物，DI 7 變紀律、回收環容量恰好取到；在音訊執行緒放掉（Bevy／rodio〔知〕）、SoLoud 的 mutex：破 DI 7 | C-2（替換時聲音照響） |
| loader 在 audio、audio 連 asset | bevy_audio 的 Module 層級（混音核心不同，4.2） | (ii) 另開函式庫：多一個 Module、型別與特性分兩處 | C-1、C-4 |
| WAV PCM16 48 kHz、載入時變成取樣 | master `SoundLoader`＋audio_editor 的輸出；Kira／miniaudio DECODE | Bevy 存編碼位元組、播時解碼：破 DI 7；收任何取樣率：破 DI 9 | C-1、C-4 |

**決勝的不是**：既有測試與突變要不要改（是費用，AGENTS §4）。(E) 要改的測試與突變比 (A) 多，寫在費用裡。

## 9. 實作的步驟（使用者說「開始」之後）

0. **基準**：改之前跑 audio 的 3 個 CTest（本機 Intel Mac）與第 0 節列的突變；在 ThreadSanitizer、AddressSanitizer 的手動組態各跑一次 audio 的 CTest（同 #279 的組態）；跑一次完整突變清單、記下哪些 find 會在改寫後失配。log 放主 checkout 的 `build/evidence/gyo2-audio-clip-loader/`。
1. **clip（第 1 題）**：`Clip`、`PlayRequest` 帶 clip、參照表與回收環、拿掉 `ClipBank`／`ClipId`／`LoadClip`／`UnloadClip`／clip 格。測試照下面處理：
   - 設定從 `ClipBank::Add` 換成 `Clip::Make`、判定不變：`MixerTests`、`ScheduleTests`、`StopTests`、`PlayheadTests`、`CharacterizationTests`（**雜湊常數不改**——輸出逐位元相同是 (E) 不改混音的證據）。
   - `ClipSlotTests.cpp` 換成 `ClipTests.cpp`：**15 案逐判定**，每個判定寫 AGENTS §14.4 的 1～5（守的性質、緩的面、緊的面、性質由哪個判定接手、根據）。它們是 DI 6、DI 7、DI 9、DI 11 的守衛，**緩的面有無都要使用者核准**（§14.4）：1～5 進合併前核准頁，先做突變與別的檢證者（Opus 5.5／high，作者以外的 Agent，照領航員規則「計畫沒有指派的 §14.4 檢證者照實作者的指派」）的檢證。隨機制拿掉的判定（格數上限、`NoClipSlot`、世代、卸載的帳、卸載後的 Programmer Error）寫明拿掉、不接手。
   - 第 317 行的換手案（audio 重審判定 2 的守衛之一，突變 `au-review-handoff-rebound`）不拿掉：改寫成「A 播、`join`、B 播、B 取空 A 送出的播放的參照」，ThreadSanitizer 下跑。
   - `RenderAllocTests` 的兩案照第 10 節第 3 項改成分段計數（`Render` 一段、提交口的呼叫一段）。
2. **loader（第 5 節）**：`WavLoader`、`WavLoaderCode`、`AssetTypeName<Clip>`；登記檔與 CMake（第 2 題）。
3. **文件**：audio 頁（第 1～3、5 節、新的第 6 節、DI／RC，第 7 節）；asset 頁第 1、9 節；`target/v2/README.md` 加本頁；§5.2 的 O-2 引用（4.4）；第 6 節第 4 項的註解、description 與 CCR-9。
4. **突變**（第 10 節）。
5. dev_log、設計審查（Fable／high，唯讀）、合併前核准頁 `audio-clip-loader-merge-approval.zh-Hant.md`、PR（base `gyo2`），等「合併」。

**時間估計**：(E)：程式 6～9 h、loader 3～4 h、文件 3～4 h、突變與 sanitizer 3～4 h、§14.4 的 1～5 與檢證 2～3 h、dev_log 與核准頁 1～2 h，共 18～26 h。(A)：共 11～16 h。**不含**宣告的再修訂、突變清單的敵對式檢查與設計審查之後的修正回合。超過 1.5 倍停下回報。

**模型／檔位**：上段（公開介面、Design Invariants、Data Contract）。實作者 Opus 5.5／high（本卡）；宣告的敵對式檢查、突變清單的敵對式檢查、合併前設計審查 Fable 5.2／high（主會話的 brief 指定）；§14.4 的檢證者 Opus 5.5／high（作者以外的 Agent）。

## 10. 測試、判定（先宣告；結果出來之後不放寬）

1. **既有行為不變**：第 9 節第 1 步第一項的測試判定不變、全部通過；`CharacterizationTests` 的雜湊常數不改。
2. **不複製**（新）：
   - `Clip` 的複製、`Play` 的呼叫（回收環是空的時）在呼叫的執行緒上配置數為 0（`gyo_audio_alloc_tests`）。
   - `Clip::Make(vector&&)` 不複製：`View().samples.data()` 等於傳進去的 vector 的 `data()`。
   - 經 `AssetStore` 載好的 clip（`Read<Clip>`）與它的複製，`View().samples.data()` 相等。
3. **音訊執行緒不放掉**（新，DI 7；分段計數：`Render` 一段、提交口的呼叫一段、各在自己的執行緒）：外面的持有者在播放中放掉最後一份參照之後，每一段 `Render` 的配置數與釋放數都是 0，直到播放結束之後；提交口的下一次呼叫那一段的**釋放數等於單獨解構一個同樣大小的 clip 的釋放數**（在測試裡先量好）。結局逐一一案：準時播完、晚到播完、沒有 voice、停在開始前（`Stop` 裡當場釋放 voice）、停在之後的 frame（在混音裡結束）、晚到的停止（在 block 開頭結束）。「環滿」不經過音訊執行緒（`Play` 當場失敗），另一案判「格當場放回、釋放落在這次呼叫」。
4. **表與回收環繞過兩圈**（新）：`queueCapacity = 1`、`maxVoices = 1`（表 3 格），播放、結束、取空跑到格號與兩個環都繞過兩圈以上，每輪外面的持有者先放掉；每輪都照第 3 項判，最後每個格都空、堆疊滿。
5. **ThreadSanitizer、AddressSanitizer**（本機手動組態）：「plays and drops clips while another thread renders」（生產者每輪放掉自己手上的 clip，ASan 抓音訊執行緒讀已釋放的取樣）、換手案、既有壓力測試沒有報告；和第 0 步比。
6. **loader**（新，DI 12）：
   - 第 5.2 節規則 1～7、9 各一案、碼對得上；同時違反兩條的三案（例：22050 Hz 的 24 位元 → `NotPcm16`；缺 `data` 而 `fmt ` 不足 16 → `MissingChunk`；聲道 3 而 `data` 奇數 → `BadFormatChunk`），判先後。
   - 規則 8：經 `AssetStore`、`byteLimits` 設得比檔小 → `TooLarge`。
   - master 的 `player_footstep.wav`（34 KB；從 origin/master 3b411e21 複製進 fixture，git 物件相同）原樣載得了：frame 數、聲道數、取樣與檔案位元組相同。4 個檔全載的結果記在證據（4 個檔頭同形，放 1 個就夠，§14.5）。
   - `LIST` chunk 跳過、奇數長度 chunk 的補位、最後一個 chunk 缺補位、不到 8 位元組的尾巴。
   - 同樣的位元組兩次同結果、兩條執行緒各 1000 次同結果（AS-3）。
   - 經 `AssetStore` 以測試執行器 `Request<Clip>` → `Loaded`，取到的 clip 交給混音器播出預期的取樣。
7. **連結**：`gyo_expect_audio_links` 照第 2 題的結論。

**突變**（AGENTS §14.5，只跑相關的；預期失敗在立清單時逐條寫案名）：
- **改之前**跑第 0 節列的突變當基準。
- **退役**：第 0 節列的 20 條逐條判，寫「隨哪個機制拿掉」或「守的性質由哪條新突變接手」。TSAN 的 `-handback-relaxed`、`-reclaim-relaxed` 守的是交還的記憶體順序，改寫到回收環兩端（release／acquire 放寬成 relaxed，預期第 5 項的 ThreadSanitizer 案失敗）。
- **新增**（每條寫只有它捕的誤；`guards` 指 DI 7、DI 12 的條文）：
  - 取空時不清那一格（預期第 3 項的「下一次呼叫的釋放數」案失敗）；
  - 沒有 voice 的播放不還格號（預期第 4 項的「堆疊滿」案失敗）；
  - voice 結束不還格號（同上，另一案）；
  - `Play` 拿格時不先取空（預期第 4 項在表 3 格時用光、回無效 handle）；
  - `Clip` 的複製改成深複製（預期第 2 項的 `data()` 相等案失敗）；
  - loader：取樣率寫死 48000、不查位元數、不查格式碼、不查 block align、`data` 奇數照收、接受越界的 chunk、接受重複的 chunk、不跳過補位（各預期第 6 項對應的案）。
- 碰到的突變清單受一次別的模型（Fable／high）的敵對式檢查（§14.2）；合併前在背景跑一次完整清單（`mutation-rerun-scope`）。

## 11. 本卡判的（不是使用者的話）

- C++ 型別名 `Clip`（混音器的用詞：`ClipView`）、catalog 型別名照舊 `sound`（P11）；`WavLoader`、`WavLoaderCode`、`WavSettings` 的名字。
- 第 5 節的格式規則、判的先後與碼、Version 的表示法、64 MiB 的預設上限。
- (E) 內部採 (E-2)（4.1）；參照表的容量、取空的時點。
- frame 數上限加進 `Clip::Make` 的規則。

## 12. 要使用者定的

**第 1 題：clip 的來源**（推翻 audio 重審宣告第 2 節「clip 格的租約與交還」的單位、master `v8-engine-audio-control` 的 `LoadClip`／`UnloadClip`，與 11-22 判成作者的「替換時卸載」（P9）；RC 1、RC 6 成立的處理）
- **(E) 建議**：拿掉 `ClipBank` 與 clip 格；播放帶共用的 clip；混音器的參照留在提交口這一側，音訊執行緒只還格號。
- (A)：拿掉 `ClipBank`；`LoadClip` 收共用的 clip、不複製；格與卸載留，`clipSlotsPerSubmitter` 預設改 32。
- (B) 照舊複製、(C) 兩條都留、(D) 借 asset 的記憶體：不建議（第 4.1 節表）。
- 取捨：兩者都滿足 C-1～C-4。(E) 作者那一側少三件事、不必逐提交口載入，費用是改寫 clip 那一半（估 18～26 h），而且作者沒有主動讓混音器放掉的入口（最晚等那個提交口下一次被呼叫）；(A) 改動約一半、有 `UnloadClip`，留下格的模型與替換 World 時的卸載。
- **附帶（選 (E) 時）**：要不要加一個只取空、不送命令的入口（例 `Submitter::Collect()`，同一個迴圈，沒有新機制）。卡片建議**不加**（`~Submitter` 與每個成員呼叫已取空；沒有點名的 Feature 需要；需要時是 RC 10）。

**第 2 題：loader 的位置**（改 DI 2）
- **(i) 建議**：`GYO::Audio` 連 `GYO::Asset`，loader 與型別特性在 audio（Bevy 的 Module 層級；混音器本體連上 asset，4.2）。
- (ii)：另開小函式庫，DI 2 不變，多一個 Module、作者多記一個標頭。

**第 3 題：和 Bevy 不同的四處**（第 4.3 節）：1～3 照舊（載入時變成取樣、只認 WAV PCM16 48 kHz、不做 `AudioPlayer` 式便利層）；第 4 處 loader 的登記選 (a) 產品登記、SDL 那批有建混音器的單位時改由它（建議），或 (b) 現在給 audio 一個登記單位。

## 13. 範圍的限界（AGENTS §14）

- **ThreadSanitizer、AddressSanitizer 不在 L1**（gyo2 的 CI 與 preset 都沒有；audio 重審宣告第 14 節已記）：DI 7 的資料競爭與釋放後讀取只在本機手動組態跑，PR 的 CI 不檢查。配置計數（`gyo_audio_alloc_tests`）在 L1。
- **gyo2 沒有輸出後端**：沒有真的音訊執行緒，`Render` 只由測試的執行緒呼叫。「音訊執行緒」的性質由測試的混音執行緒代表；後端的時序（block 間隔、真的裝置）要等 SDL 那批。現在做而不併進 SDL 那批的理由：使用者另開這張卡（主會話告知 SDL 卡）；clip 的來源與格式和後端無關，先定可讓 SDL 那批只接後端。
- **整合的範圍**：最長的路是 `AssetStore`（測試執行器）→ `Read<Clip>` → 提交口 → `Render`。經 Product 的 World → `AssetView::Get` → 提交口 → `Render`、與 C-2（替換 World 時聲音照響、之後放掉）的 scenario，**延到 SDL 那批或第一個搬回的產品**（把 World 接到混音器是作者的寫法，audio 頁第 5 節；那時才有真的接法可測）。混音器那一側的 C-2 性質（外面全部放掉之後已送出的播放照響完、之後放掉）由第 10 節第 3 項守。
- **基準只在本機 Intel Mac**；L1 三平台由 PR 的 CI 跑。

## 14. 停止條件

- 實作段的測試、判定卡住：AGENTS §14 冒頭的三條。
- 第 10 節第 1 項的雜湊常數必須改才通過（代表 (E) 改了混音，不是只改 clip 的來源）。
- 參照表用光的保險（4.1 第 2 點）在任何測試裡被觸發（代表第 5 點的界不成立）。
- 發現 gyo2 上有 audio 的其他使用者（第 6 節第 4 項的掃描有命中）。

## 15. 修訂 0 檢查的處理（全部採用）

| 檢查 | 處理 |
|---|---|
| B-1 決勝理由第 1 點不成立；C-2、C-3 推過頭 | 4.1「為什麼建議 (E)」第 1 點改成事實；第 3 節 C-2 改成今天的合約；加「C-1～C-4 兩者都滿足」；第 8 節同步 |
| B-2 登記與「(i) 和 Bevy 相同」 | 4.2 的 (i) 改成「Module 層級相同、混音核心不同」並寫衝突點與膠水；登記移到 4.3 第 4 處與第 3 題 |
| S-1 (E) 的代價漏列 | 進 4.1 表與第 12 節；主動放掉的入口交第 12 節第 1 題附帶；`~Submitter` 與 `Stop(無效 handle)` 也取空（4.1 第 4 點）；核心一句改寫 |
| S-2 回收環上限恰好取到、滿了怎麼辦 | 改採 (E-2)：回收環與表一樣大、一個格號只在一處，結構上不會滿；表用光在生產端處理（4.1 第 2、5 點）；停止條件改成看得到的判法 |
| S-3 環繞回、三處覆寫 | (E-2) 下音訊執行緒不握參照，三處不存在；仍加繞過兩圈的案（10.4）與對應突變 |
| S-4 觀測法 | 10.2、10.3 寫死觀測法（`data()` 相等、分段計數、釋放數等於單獨解構的數） |
| S-5 §14.4、15 案、換手案 | 第 0 節、第 9 節改 15 案；1～5 進合併前核准頁、使用者核准、Opus／high 檢證；換手案改寫保留 |
| S-6 範圍的限界 | 新增第 13 節 |
| S-7 奇數長度、聲道 0、突變 | 5.2 加規則 7、判的先後、聲道數在 block align 之前；4.1 那句改成「clip 的規則在 `Clip::Make`，位元組層的在 loader」；突變加取樣率寫死、奇數照收 |
| S-8 64 MiB | 改成卡片判並寫實際的根據；大小寫成 5.2 規則 8；C-5 拿掉 |
| S-9 前提表 | 加 P9（11-22 判作者的兩件、本批提議收回）、P12（`ClipBank` 填不進是卡片的前提）；P8 標卡片的前提；「推翻的舊決定」加 audio 頁 5.2、5.4、5.5 |
| S-10 規則的模糊處 | 尾巴忽略、判的先後、`fmt ` 不足 16 改 `BadFormatChunk`、Version 的放寬寫法；第 1 節改成「訊息說」 |
| S-11 CCR-9、註解、RC | 6.4 與第 9 節第 3 步加；4.1 與第 12 節寫 RC 1、RC 6 的狀態 |
| N-1 | 第 0 節列出 20 條的 id；失配的另由第 0 步列 |
| N-2 | Kira 與 Bevy 生態的關係標〔知〕、拿出決勝理由 |
| N-3 | fixture 只放 `player_footstep.wav`，4 個全載記在證據 |
| N-4 | 採用為 (E-2) |
| N-5 | frame 數上限加進 `Clip::Make` |
| N-6 | 逐樣本組合；示意加 `public:` |
| N-7 | 時間估計寫明不含的回合 |
| N-8 | 10.3 分兩種停止、環滿另判 |
| N-9 | 預期失敗在立清單時寫案名；TSAN 的兩條改寫到回收環兩端 |
