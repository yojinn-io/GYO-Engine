# 合併前核准：audio 那一側的小批（`v8-arch-gyo2-audio-clip-loader`）

更新：2026-11-23。**狀態：合併前核准進行中（第 1 件已核准；第 2～4 件待定）。合併要使用者另外說「合併」。** 宣告：[audio-clip-loader-declaration.zh-Hant.md](audio-clip-loader-declaration.zh-Hant.md)（修訂 2，使用者 2026-11-23 動工前核准）。dev_log：[2026_11_23_gyo2_audio_clip_loader.zh-Hant.md](../../../dev_logs/2026_11_23_gyo2_audio_clip_loader.zh-Hant.md)。

> **第 1 件核准**（使用者，2026-11-23，side chat 經轉達）：原話「核准」。核准的是第 1 節（被拿掉或改寫的 DI 守衛測試，§14.4 的 1～5；33203a6e 的版本）。比過的選項：核准／不核准。理由：使用者沒有另外說（side chat 的說明：性質還在的由新判定接手、各有突變；隨機制消失的不接手；幾條判定比以前嚴）。

**這一頁請你定四件**：
1. 第 1 節：被拿掉或改寫的 Design Invariants 守衛測試（AGENTS §14.4：DI 的守衛不論有沒有緩的面都要你核准）。
2. 第 2 節：和宣告的偏差。
3. 第 3 節：audio 頁 DI 1、2、6、7、9、11、新 DI 12、RC 1、新 RC 10 與第 6 節的最終文字（[audio.zh-Hant.md](audio.zh-Hant.md)）。
4. **第 4.1 節 A-1（選擇題）**：無效 handle 的 `Stop` 事實上就是「只取空」的入口，牽動你核准的「不加只取空的入口」。

設計審查與檢證的結論在第 4 節，判定的結果在第 5 節。

## 1. 被拿掉或改寫的 DI 守衛測試（§14.4 的 1～5）

**一句話**：舊的守衛守「clip 格」這個機制（卸載、世代、租約計數、格數上限）；機制拿掉之後，那些判定守的性質有兩種去處——(a) 性質還在（「音訊執行緒不釋放」「還得不能太早」「Programmer Error 在計數之前斷言」「帳對得起來」），由新的判定接手；(b) 性質隨機制消失（卸載的帳、世代、格數上限、`NoClipSlot`），不接手。

**怎麼讀下表**：「緩的面」是以前會失敗、之後會通過的情形；「緊的面」是以前會通過、之後會失敗的情形。「接手」寫新的判定與證明它有檢出力的突變。

### 1.1 `ClipSlotTests.cpp` 的 15 案

| # | 舊案（行） | 守的 DI | 1. 守的性質 | 2. 緩的面 | 3. 緊的面 | 4. 性質還有誰守 | 5. 根據 |
|---|---|---|---|---|---|---|---|
| 1 | plays a clip loaded while output runs（78） | DI 7 的前提 | 執行中才來的 clip 能播 | 「兩個 `ClipId` 不同」「兩格回來、能再 `LoadClip` 兩次」隨格與 `ClipId` 拿掉（格號回來由第 3、8 列的案守） | 無 | `ClipTests`「plays a clip made while output runs」（同樣的取樣、聲道、時間點判定） | 宣告 9 第 1 步 |
| 2 | keeps a clip until the last of its voices ends（103） | DI 7 | 兩個播放共用一個 clip 時，第二個還在讀就不釋放 | 「格號回來之前不能再 `LoadClip`」的判定隨格拿掉 | 新判定要求逐樣本正確、而且只能是兩個播放的值 | `RenderAllocTests`「keeps a clip two plays share until the second ends」（檢證後補：第一個的格號回來時不放掉、第二個逐樣本正確）；突變 `au-clip-entry-back-on-start` 也在這一案失敗。（`ClipTests`「plays every queued play fully…」沒有提交口的呼叫、取空不會跑，判不到這件，只判輸出） | DI 7、宣告 10.3 |
| 3 | releases the lease of every voice a block frees（120） | DI 7 | 一個 block 裡被停止釋放的每個 voice 都交還 | 「之後 `LoadClip` 成功」換成「之後的 `Play` 都成功」 | 跑 20 輪（以前 1 輪），每輪兩組 | `ClipTests`「hands back the entry of every voice a block frees」；突變 `au-ctl-stop-no-drain`（表用光、斷言） | DI 7 |
| 4 | LoadClip rejects what ClipBank rejects and reports a missing slot（136） | DI 9（規則）、`NoClipSlot` | 執行中的 clip 照同樣的規則拒絕；格滿回 `NoClipSlot` | `NoClipSlot` 的判定隨機制拿掉 | 無 | 規則：`MixerTests`「Clip rejects clips the mixer cannot play」（兩種 `Make`）；`NoClipSlot`：不接手（機制沒了） | 宣告 4.1 |
| 5 | does not hand back a clip while a voice reads it（156） | DI 7 | voice 還在讀時不交還 | 「格滿時 `LoadClip` 回 `NoClipSlot`」與 `CheckUnloadBalance`（含請求的帳）隨機制拿掉；請求的帳由 DI 6 的「balances…」案守 | 新判定在每一段 `Render` 數釋放（0）、輸出逐樣本相同 | `RenderAllocTests`「lets go of a clip…」(a)、「does not hand a reused entry back early」；突變 `au-clip-entry-back-on-start`（＋ASan 版）、`au-ctl-ended-not-cleared` | DI 7、宣告 10.3 |
| 6 | plays a queued play of an unloaded clip fully（171） | DI 7 | 卸載排在已送出的播放之後，那些照響完 | 「卸載」換成「外面全部放掉」 | 兩個播放、逐樣本 | `RenderAllocTests`「lets go of a clip on the submitter's next call…」（呼叫的人在第一次 `Render` 之前放掉，之後判輸出與 (b)）；`ClipTests`「plays every queued play fully after every other holder let go」（兩個播放、只判輸出）；突變 `au-clip-play-holds-nothing`（檢證後補：`Play` 不在表裡存參照） | DI 7 |
| 7 | hands back a stopped clip once its voice ends（181） | DI 7 | 停止的 voice 結束後才交還 | 無 | 三種停止（開始前、之後的 frame、晚到）各自判釋放數 | `RenderAllocTests`「lets go of a clip…」的 StopBeforeStart、StopLater、LateStop；突變 `au-ctl-clip-lease-not-ended-*` 三條 | DI 7、宣告 10.3 |
| 8 | gives a play dropped for want of a voice no lease（215） | DI 7 | 沒有 voice 的播放不佔租約 | 「不佔租約」換成「格號當場交還」（租約計數沒了） | 判釋放落在下一次呼叫 | `RenderAllocTests`「lets go of a clip…」NoVoice、「takes its table and both rings around twice」；突變 `au-clip-novoice-not-returned` | DI 7 |
| 9 | does not hand back a reused slot early（227） | DI 7 | 重用的格不被前一個 clip 提早交還 | 無 | 判釋放數與逐樣本輸出 | `RenderAllocTests`「does not hand a reused entry back early」；突變 `au-ctl-ended-not-cleared` | DI 7 |
| 10 | may unload again after its queue was full（254） | DI 6 | 環滿丟掉的卸載記數、clip 仍載入中、可再卸 | 卸載的帳隨機制拿掉 | 無 | 不接手（卸載沒了）；環滿丟掉的播放：`RenderAllocTests`「lets go of a play the full queue dropped…」、突變 `au-clip-full-not-returned` | 宣告 4.1 |
| 11 | gives a reloaded slot a new ClipId（273） | DI 11 | 卸載過的 id 不會播下一個 clip | 隨 `ClipId` 拿掉 | 無 | 不接手（`ClipId` 沒了；播放帶 clip 本身） | 宣告 4.1、第 7 節 |
| 12 | treats misused runtime clips as Programmer Errors（292） | DI 11、第 3 節 | 用錯的 clip 是 Programmer Error，在計數之前斷言（`unloadsSubmitted == 1`） | 舊案的六類誤用（別的提交口的 clip 播與卸、卸載過的 clip 播與卸、庫的 clip 卸、`ClipId{}` 卸）與卸載的計數判定隨機制拿掉 | 無 | 「在計數之前斷言」：`ScheduleTests`「treats an empty clip, a bad gain and a moved-from handle as Programmer Errors」——舊案就有 `submitted == 1`，本批把其中的「不在庫裡的 `ClipId{2}`」換成「空的 `Clip{}`」（1.2 另列）；突變 `au-clip-empty-counts-before-assert` | 第 3 節的 Contract |
| 13 | loads on one thread and plays and unloads on another after a hand-over（317） | 判定 2（audio 重審） | 換手後照常使用；舊案只有一格，第二方要等格回來才能 `LoadClip`，所以也判了「交還」 | 「一格、等它回來」的判定隨格拿掉 | 無 | `ClipTests`「plays on one thread and lets go on another after a hand-over」（檢證後補：換手後再用 5 次，表 3 格，取空失效就斷言）；`RenderAllocTests`「lets go, on the thread it was handed to, of a clip played before the hand-over」（第二方的釋放數＝一個 clip）；突變 `au-review-handoff-rebound`（三個換手案） | audio 重審判定 2 |
| 14 | rejects more than 256 clip slots per submitter（338） | 設定的上限 | `clipSlotsPerSubmitter` 0～256 | 隨設定拿掉 | 無 | 不接手 | 宣告 4.1 |
| 15 | loads, plays, stops and unloads while another thread renders（348） | DI 6、DI 7（TSan、ASan） | 並行時沒有資料競爭、不讀已釋放的取樣、帳對得起來 | 「卸載」換成「放掉」 | 每輪生產者放掉自己的 clip | `ClipTests`「plays and lets go of clips while another thread renders」（TSan、ASan）；突變 TSan `-handback-relaxed`、`-reclaim-relaxed`、ASan `au-clip-entry-back-on-start-asan` | DI 7 |

### 1.2 其他檔的 DI 守衛

| 舊案 | 守的 DI | 改了什麼 | 緩的面 | 根據 |
|---|---|---|---|---|
| `MixerTests`「ClipBank rejects clips the mixer cannot play」 | DI 9 | 改名「Clip rejects clips the mixer cannot play」，兩種 `Make` 都判；拿掉「`Count() == 0`」 | 「庫裡沒有多出 clip」隨庫拿掉 | 突變 `au-clip-rate-unchecked` 照樣 killed |
| `RenderAllocTests`「Render allocates and frees nothing」 | DI 7 | `Render` 與提交口的呼叫分段計數；提交口的呼叫另判 0／0 | 無（分段只是更細） | 突變 `au-ctl-alloc-in-render` killed |
| `RenderAllocTests`「Render allocates and frees nothing for clips loaded while output runs」 | DI 7 | 換成新的 9 案（1.1 第 2、3、5～9、13 列點名的；其中「lets go of a clip…」一案含六種結局） | 「`unloadsTaken == 2`」與「卸載後再 `LoadClip` 兩次成功」隨機制拿掉 | 同上表 |
| DI 6 的帳（「balances…」等） | DI 6 | 卸載的那一句與守衛拿掉；壓力案改名（「plays and lets go of clips…」），它的帳加了停止的那一條 | 卸載的帳 | — |
| `ScheduleTests`「treats a foreign clip, a bad gain and a moved-from handle as Programmer Errors」 | 第 3 節的 Contract | 「不在庫裡的 `ClipId{2}`」換成「空的 `Clip{}`」，改名 | 「不在庫裡的 clip」隨庫拿掉 | 突變 `au-clip-empty-counts-before-assert` |

### 1.3 表上原本沒列、檢證時找出的拿掉的判定

1. `MixerTests`「VoiceMixer treats a foreign clip and a bad gain as Programmer Errors」的 `Schedule({ClipId{2}})` 斷言：隨庫拿掉；`ClipView{}`（沒有 frame）的斷言留著。
2. `MixerTests`「ClipBank copies the samples and names clips from one」的 id 順序、`Contains`、`View(ClipId{3})` 斷言：隨庫拿掉；「複製一次、之後共用」由改寫的同一案判。
3. `static_assert(NoClipSlot == 5)`：隨 `NoClipSlot` 拿掉；「5 不重用」只剩 `AudioError.hpp` 的註解（4 也是這樣處理的）；`ClipTests.cpp` 改釘 `TooManySubmitters == 3`。
4. 第 1、5、13、15 列的附帶判定（例：舊壓力案的 `loaded == kRounds`——新案改成「每輪重試到 `Play` 成功」，不再有載入失敗的出口）。
5. 舊 `RenderAllocTests` 的 `unloadsTaken == 2` 與之後的兩次 `LoadClip`（1.2 第 3 列）。

### 1.4 證明（§14.4：緩的面有的，先做突變與別的檢證者的檢證）

- 突變：改之前的清單 77/78（未改的程式）；改過之後 85/86（第 5 節）。存活的一條兩邊相同，和本批無關。
- 別的檢證者：Opus 5.5／high（作者以外的 Agent），用實際的原始碼與突變結果核對 1.1、1.2 的 1～5（第 4.2 節）。上面的表已照它的結論改寫。

## 2. 和宣告的偏差

1. 宣告第 10 節的突變「`Clip` 的複製改成深複製」沒立：`Clip` 的複製是 `shared_ptr` 的預設複製，沒有可以改的一行。改成兩條各自可改的：`au-clip-play-copies-samples`（`Play` 把取樣複製進表，預期「五種呼叫」案的配置數判定失敗）、`au-clip-make-vector-copies`（`Make(vector&&)` 複製，預期 `data()` 相等的判定失敗）。
2. 宣告 10.4 的「表與回收環繞過兩圈」用 12 輪（表 3 格）。另加「does not hand a reused entry back early」一案：第一輪完整突變時 `au-ctl-ended-not-cleared` 存活，才發現缺「重用的格號不提早回來」（舊的「does not hand back a reused slot early」的對應）。
3. 宣告 10.6 的 master 範例只放 `player_footstep.wav` 一個（照窄複查 N-3）；4 個全載的核對在敵對式檢查時做過（證據 `declaration-r0-adversarial.md`「確認成立的」）。
4. 突變清單的 `cases` 不能含逗號（doctest 把逗號當分隔）：第一輪有 3 條因此一案都沒跑、記成存活；改成不含逗號的片段後 killed。記在 dev_log。
5. `Clip` 的內部形：第一版實作成「`shared_ptr<const vector>`＋快取的 `ClipView`、`View()` 回參照」，和宣告 4.1 的「`shared_ptr<const ClipData>`、`View()` 傳值」不同，而且被搬走的 `Clip` 的 view 會指著不歸它管的取樣（設計審查 S-1）。已改回宣告的形（`Detail::ClipData` 裝取樣與聲道數，`View()` 每次現算），加一個「被搬走的 clip 的 view 是空的」判定。
6. `ClipValidation.hpp` 併進 `Clip.cpp`（只剩它用；設計審查 N-1）。
7. `Submitter` 的搬動賦值先取空（被覆蓋的那個提交口算一次呼叫，和解構子一樣；設計審查 N-2）。audio 頁第 3 節的執行緒契約與 `Mixer.hpp` 的註解寫進去。
8. 檢證後補的測試：「keeps a clip two plays share until the second ends」「lets go, on the thread it was handed to, of a clip played before the hand-over」、換手案再用 5 次；突變 `au-clip-play-holds-nothing`（清單 86 → 87）。

## 3. Design Invariants 與 Replacement Criteria 的最終文字

照宣告第 7 節的方向（使用者 2026-11-23 動工前核准），寫在 [audio.zh-Hant.md](audio.zh-Hant.md)：

- **DI 2**：連結恰好是 `GYO::Base`、`GYO::Time`、`GYO::Asset`（守衛 `gyo_expect_audio_links`，突變 `au-link-engine`）。
- **DI 6**：拿掉卸載的帳。
- **DI 7**：參照表、音訊執行緒不握參照、回收環與取空（全文見頁）；守衛換成 1.1 的新案。
- **DI 9**：守衛名改「Clip rejects clips the mixer cannot play」。
- **DI 11**：拿掉 `ClipId` 的世代一句與兩個守衛。
- **新 DI 12**：音效的 Data Contract（第 6 節）。
- **RC 1**：拿掉「由別的提交口載入、卸載或播放」，補「混音器只借 asset 的記憶體」。
- **新 RC 10**：「最晚在下一次呼叫時放掉」超出預算，或需要不經 `Play`／`Stop`／`StopAll` 的放掉入口。
- 第 3 節的執行緒契約加解構子；第 6 節（格式、Version、Validation Rule、範例）。

## 4. 設計審查與檢證

### 4.1 設計審查（Fable 5.2／high，唯讀）

報告：主 checkout `build/evidence/gyo2-audio-clip-loader/design-review.md`。結論：**設計合理，可以交合併前核准。blocker 0、should 5、nit 10；牽動已核准決定的 4 條。**

| 條 | 內容 | 處理 |
|---|---|---|
| S-1 | `Clip` 快取的 view 在搬動後懸空，而且和宣告的形不同 | 改回宣告的形（第 2 節第 5 項） |
| S-2 | 「哪個 asset 失敗」由每個 loader 各拼一次（`TextLoader`、`WavLoader`），store 又加一次 | owner 是 asset；不在本批。轉主會話排（第三個 loader 之前定） |
| S-3 | `Mixer.cpp` 裡同一個 SPSC 協定手寫了兩份（命令環、回收環） | 不擋合併；轉主會話，建議排在 SDL 後端接 `Render` 之前 |
| S-4 | DI 7 把做法寫進了條文 | 改成本質三件 (a)(b)(c)，做法指第 2、3 節（第 3 節） |
| S-5 | audio 頁兩處舊說法（5.3「要卸」、第 3 節「無效 handle 什麼都不做」） | 已改 |
| A-1 | 無效 handle 的 `Stop` 事實上是「只取空」的入口 | **交你選**，見下 |
| A-2 | 參照留在生產端（E-2）對 Kira 的形（E-1）：實作後的帳和宣告相符 | 交代，不建議推翻；不改 |
| A-3 | 兩種 `Make`：`Make(vector&&)` 把 `std::vector` 釘成「不複製」的交接型別 | 交代；不改 |
| A-4 | WAV 規則的先後整個寫成合約 | 交代；卡片判、已核准，不改 |
| N-1、N-2 | `ClipValidation.hpp` 只剩一處用；搬動賦值不取空 | 已改（第 2 節第 6、7 項） |
| N-3 | 堆疊與回收環可合成一個環；但測試的檢出力靠「空格是堆疊」的順序 | 不改（收益小）；記在 dev_log |
| N-4 | 兩個測不到的靜默分支；`VoiceMixer` 的結束清單是 `Mixer` 與它之間沒寫在型別上的約定 | 不改；下次動 `VoiceMixer` 時考慮（dev_log） |
| N-5～N-8 | 釋放數的基準用 10 frame 的 clip；65536 那一案與 `au-clip-empty-counts-before-assert` 是日後退役的候選；`InvalidClip` 收五種理由 | 不改；記在 dev_log |
| N-9 | `WavLoader.hpp` 的 enum 註解、DI 1 的措辭、DI 7 的守衛清單漏三案 | 已改 |
| N-10 | dev_log 漏 `Clip` 的內部形；測試 CMake 的過時註解 | 已改 |

**A-1：要你選。** 例子：FPS 打完一場回主選單，對戰 World 的提交口（照 5.2 節用名字 "match" 沿用）最後響著一段 20 秒的結算音樂（約 3.8 MB）。World 換掉、Handle 放掉，主選單用別的提交口。"match" 沒人再呼叫，那段取樣留到混音器的狀態解構（整個行程）。作者要放掉，得在音樂結束後對它再呼叫一次成員；最沒有副作用的寫法就是 `Stop(SoundHandle{}, at)`。你核准過「不加只取空的入口」，可是同一份宣告 4.1 第 4 點又要「`Stop`——含無效 handle」取空，測試也把它釘成契約：入口存在，只是讀起來像「停一個不存在的聲音」。前提：作者能處理（現有 API 做得到），問題在名字與文件。

- **(a) 照現狀，寫成作者可用的寫法**（建議；審查者也傾向）：audio 頁 5.2 補一句「不再用的提交口，聲音結束後再呼叫一次任一成員（例：`Stop(SoundHandle{}, at)`）就放掉」，RC 10 的後半（「不經 `Play`／`Stop`／`StopAll` 的入口」）改寫成「這個寫法不夠用時」。程式不改。
- (b) 給它名字（`Submitter::Collect()`，同一個迴圈），無效 handle 的 `Stop` 回到真的什麼都不做。**推翻「不加」的決定。**
- (c) 無效 handle 的取空不進契約（不寫、不測）。測試的探針要換寫法。

### 4.2 §14.4 的檢證（Opus 5.5／high）

報告：主 checkout `build/evidence/gyo2-audio-clip-loader/verifier-14-4.md`。19 列：**CONFIRMED 13、PARTLY 6（1.1 第 1、2、5、6、12、13 列）、WRONG 0。** 處理：

- 第 1、5、12 列：「緩的面」補上漏寫的判定（已改寫表）。
- 第 2 列：引用的 `ClipTests` 案沒有提交口的呼叫、判不到；補「keeps a clip two plays share until the second ends」。
- 第 6 列：補引用 `RenderAllocTests` 的案與突變 `au-clip-play-holds-nothing`。
- 第 13 列：「緊的面」其實判不到（3 格、3 次 `Play`）；換手案改成再用 5 次，另加 `RenderAllocTests` 的換手版（第二方的釋放數）。
- 表上沒列的判定：補第 1.3 節。DI 7 的守衛清單補齊。`mut-after-rerun.log` 跑在哪個樹上：2d7db6c0＋工作樹（內容即 03b6ea0c），記在 dev_log。

## 5. 判定的結果

證據都在主 checkout 的 `build/evidence/gyo2-audio-clip-loader/`（本機 macOS Intel）。下面是審查修正之後的一輪（`r2-*`，03b6ea0c＋審查修正，內容即本頁所在的 commit）；之前那一輪（03b6ea0c）的結果在 dev_log。

| 判定（宣告第 10 節） | 結果 | 證據 |
|---|---|---|
| 1 既有行為不變（雜湊常數不改） | 通過；`CharacterizationTests` 的常數未改 | `r2-full-ctest.log` |
| 2 不複製 | 通過（`data()` 相等三處、`Play` 配置數 0） | 同上 |
| 3 音訊執行緒不放掉、提交口不早放 | 通過（六種結局、五種呼叫、兩個播放共用、重用、環滿、換手） | 同上 |
| 4 表與回收環繞過兩圈、格號 65536 以上 | 通過 | 同上 |
| 5 TSan、ASan | 各 3/3、沒有報告；突變 TSan 7/7、ASan 1/1 | `r2-thread-ctest.log`、`r2-address-ctest.log`、`r2-mut-tsan.log`、`r2-mut-asan.log` |
| 6 loader | 通過 | `r2-full-ctest.log` |
| 7 連結 | 通過（設定時的檢查） | 同上 |
| 突變（完整清單 87 條） | 86/87；存活 `au-ctl-stress-reassign-every-block`（未改的程式上也存活，`mut-base-full.log`；上一批已記為時序偶發） | `r2-mut-full.log` |
| 全量 `ctest -L "cpu|shader"` | 182/182 | `r2-full-ctest.log` |
