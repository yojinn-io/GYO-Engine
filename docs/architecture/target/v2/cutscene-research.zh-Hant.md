# 過場動畫（即時渲染）：成熟方案比較（`v8-arch-gyo2-cutscene-research`）

更新：2026-11-27（新頁）。Owner：研究卡（由 GYO2 重建主會話開出）。**狀態：研究，不實作。第 8 節的決策點等使用者決定；這份文件不改任何已定的決定。**

查的版本：`gyo2` 85df84da。Godot（`94fc09c`）、Bevy（`6a911653f`）用 `~/Code/GitHome` 的本地源碼（路徑從各 repo 的根算起）。Unity Timeline、Unreal Sequencer、Bevy 生態的第三方 crate 不在本機（本機的 `com.unity.entities`／`com.unity.netcode` 裡搜不到 `PlayableDirector`），只讀官方網頁與 README（附 URL），沒有下載或 clone。

**查證等級**：〔源〕＝讀了源碼、附檔名與行號；〔文〕＝讀了官方文件或 README 的那一頁；〔知〕＝既有知識、這次沒有找到原文；〔未確認〕＝查了但沒找到。

## 0. 問題與判斷依據

使用者問：「我要如何加過場動畫（實機渲染）」。兩個例子：

1. **撤離成功**：鏡頭沿一條路徑飛過直升機，角色播登機動畫，配音效與字幕，最後切到結算畫面。
2. **開場**：現在是 `Scene::Splash` 加寫死的 `splashSeconds`（`games/object_fps_pvp/include/object_fps_pvp/client/Scenes.hpp:100`，預設 2.0 秒）；`LeaveSplash`（`games/object_fps_pvp/src/client/Scenes.cpp:228-256`）讀 `RealTime`，文件排好後計時，按任意鍵或時間到就切主選單。之前提過「資料驅動的開場步驟＋UI tween」，沒有定案（gyo2 的文件與程式裡 `git grep -i tween` 沒有結果）。

判斷依據是 AGENTS.md §1：

- §1-1：Engine 只提供機制，替換得了的規則由作者選。
- §1-6：名字是概念的 tag。「過場」「撤離」「直升機」是產品的詞，不進 Engine。
- §1-7：Tool 與 Game 只經資料格式交接；Game 沒有 Tool 也能跑。
- §1-10：預設方便，不綁作者。
- ECS 相關以 Bevy 為主參考；偏離時寫衝突點與膠水量（批次表〈每批的設計審查〉第 5 項）。

每一題開頭先寫前提：「這是 Engine 要解決的（只有 Engine 做得到，或多個產品都要、且作者各寫一份會分歧），還是作者能處理的」。

**主會話的初步方向**（本文拿成熟方案驗證）：timeline 是一份資料，裡面有多條軌道（鏡頭路徑、角色動畫片段、音效、字幕、事件），每條放關鍵幀；照時鐘推進、各軌取當下的值；過場期間鎖輸入、相機由 timeline 接管；播放器放 Engine、編輯工具是 Tool；多人時表演性的過場只在 Client 本地播。**結論先講**：方向大致成立；要修正三處——(1)「播放器」要拆成「取樣機制」與「把值寫進 World 的 binder」兩層，因為 GYO 的 Kernel 沒有反射（第 2.2 節）；(2) 鏡頭路徑最便宜的來源是 DCC（Blender）匯出的相機動畫，不一定要先有編輯工具（第 6.2 節）；(3)「鎖輸入」「相機接管」不是 timeline 的職責，是遊戲用現有機制做（run 條件、`Camera::active`），Engine 不另加旗標（第 3.1 節）。

## 1. 四家怎麼做

### 1.1 Unity Timeline（Playables）〔文〕

- **資料與實例分開**：Timeline asset「存軌道、片段、錄下的動畫，不連到特定 GameObject」；連到場景物件的叫 bindings，存在場景裡，一個 asset 可以有多個實例（<https://docs.unity3d.com/Packages/com.unity.timeline@1.8/manual/tl-overview.html>）。`PlayableDirector` 元件連起 asset 與實例，從 asset 建一張 `PlayableGraph`；binding 用 `SetGenericBinding`／`GetGenericBinding`；多個 director 用同一份 asset 時各自一張 graph、各自的 binding（<https://docs.unity.com/en-us/engine/current/script-reference/unityengine/playables/playabledirector>）。
- **內建軌道**：Activation、Animation、Audio、Control（子 timeline、粒子、prefab、`ITimeControl`）、Playable（自訂，片段腳本繼承 `PlayableAsset`）、Signal（要在被綁的物件上放 Signal Receiver）（<https://docs.unity3d.com/Packages/com.unity.timeline@1.8/manual/trk-add.html>）。Animation 軌道**引用** AnimationClip，timeline 自己不是動畫片段。
- **時鐘**：`DirectorUpdateMode` 有 `DSPClock`（音訊時鐘）、`GameTime`（`Time.time`，受 timeScale）、`UnscaledGameTime`、`Manual`（「自己呼叫 `PlayableGraph.Evaluate` 給 deltaTime」）（<https://docs.unity.com/en-us/engine/6000.6/script-reference/unityengine/playables/directorupdatemode>）。`time` 可以直接改；`Evaluate()` 在 `PlayableDirector.time` 求值。Wrap mode：Hold、Loop、None（`extrapolationMode`）。
- **事件**：Signal Emitter 是軌道上的 marker，指一個 Signal asset；Signal Receiver 把 signal 對到反應。反應可以選「Editor and Runtime」，預覽時也觸發（<https://docs.unity3d.com/Packages/com.unity.timeline@1.8/manual/wf-signals.html>）。Emitter 有 Retroactive、Emit Once 兩個選項、預設關，頁面沒解釋意思〔未確認：時間跳過時補不補發〕。
- **相機**：Cinemachine 軌道驅動時「覆蓋 Cinemachine Brain 依 priority 的決定」，停止驅動時交回 Brain；相鄰片段是切、重疊片段是混合；Ease In/Out 和 Brain 的即時相機混合（<https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/concept-timeline.html>）。
- **自訂軌道**：`PlayableBehaviour`＋`ScriptPlayable<T>`；範例有自訂的 track、clip、mixer（Tween、Time Dilation、Video、Text），混合由「TrackMixer `PlayableBehaviour`」做（<https://docs.unity3d.com/Packages/com.unity.timeline@1.8/manual/samp-custom-samples.html>）。
- **網路**：讀到的 Timeline、Playables、Cinemachine 頁都沒提〔未確認〕。
- **位置**：Timeline 是官方套件（`com.unity.timeline`），不在引擎核心；Playables 在核心。

### 1.2 Unreal Sequencer〔文〕

- **資料與實例分開**：Level Sequence asset「含 Sequencer 的資料：軌道、相機、關鍵幀、動畫」，交給關卡裡的 Level Sequence Actor 才綁到關卡；沒有關卡引用時開啟，內容是 unbound（<https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-sequencer-movie-tool-overview>）。
- **binding**：Possessable 引用關卡裡已有的 actor（soft object path）；Spawnable 在 sequence 開始時生、結束時毀，可選 owner（This Sequence／Root Sequence／External）與 Net Addressable（伺服器與 Client 用同一個名字引用）（<https://dev.epicgames.com/documentation/unreal-engine/spawn-temporary-actors-in-unreal-engine-cinematics>）。
- **Actor 的播放設定**：Auto Play、Loop、Play Rate（「不影響 Time Dilation」）、Start Offset、Restore State、Pause at End；Cinematic 設定：Disable Movement Input、Disable Look At Input、Hide Player、Hide Hud、Disable Camera Cuts（同上 overview 頁）。**Unreal 把「鎖輸入、藏 HUD」放進播放設定**，這是它的框架（Gameplay Framework 的 PlayerController）有這些概念；GYO 的 Engine 沒有（第 3.1 節）。
- **時鐘**：Clock Source 有 Tick（引擎預設）、Platform（系統時間）、Audio（音訊時鐘）、Timecode、Relative Timecode；「Lock to Display Rate at Runtime」（<https://dev.epicgames.com/documentation/en-us/unreal-engine/sequencer-cinematic-toolbar-in-unreal-engine>）。tick resolution 與 display rate 的定義〔未確認〕。
- **編輯器**：倒著播、逐幀、跳到上／下一個 key；左鍵拖播放頭會求值預覽，中鍵拖「不求值」地移動時間（<https://dev.epicgames.com/documentation/unreal-engine/sequencer-cinematic-editor-unreal-engine>）。
- **事件**：Trigger 事件在 key 那一幀求值；Repeater 在區段內每幀求值；軌道有「Fire Event when Forwards」「Fire Event when Backwards」兩個屬性；事件的邏輯在每個 sequence 的 Director Blueprint（<https://dev.epicgames.com/documentation/en-us/unreal-engine/cinematic-event-track-in-unreal-engine>）。跳轉、拖動時觸不觸發〔未確認〕。
- **相機**：Camera Cut 軌道決定哪台 Cine Camera 在用；「Can Blend」讓重疊區段混合，ease in/out 能和遊戲相機互相混合（<https://dev.epicgames.com/documentation/en-us/unreal-engine/cinematic-camera-cut-track-in-unreal-engine>）。
- **跳過**：Blueprint 有 Play、Set Playback Position、「Go to End and Stop」、Stop；「Go to End and Stop」是官方說的跳過方式（保留 sequence 本來會留下的改動）；每條軌道有 When Finished＝Restore State／Keep State，Actor 層的設定會蓋過它（<https://dev.epicgames.com/documentation/unreal-engine/play-cinematics-from-blueprints-in-unreal-engine>、<https://dev.epicgames.com/documentation/unreal-engine/what-happens-when-my-cinematic-ends-in-unreal-engine>）。
- **網路**：工具列有「以 Client 或 Server 的情境模擬」的選項（toolbar 頁）；`bReplicatePlayback` 的行為、播放是伺服器權威還是 Client 本地〔未確認：API 頁讀不到內容〕。
- **位置**：Sequencer 隨引擎附帶（Level Sequence 是引擎模組）〔知〕。Skeletal Animation 軌道引用 AnimSequence〔知〕。

### 1.3 Godot：`Animation`＋`AnimationPlayer`〔源〕

- **一個資源同時是「骨骼動畫」與「timeline」**：`Animation` 的軌道種類（`scene/resources/animation.h:50-59`）：`TYPE_VALUE`（任何屬性）、`TYPE_POSITION_3D`／`ROTATION_3D`／`SCALE_3D`（骨骼與節點）、`TYPE_BLEND_SHAPE`、`TYPE_METHOD`（呼叫任何節點的方法）、`TYPE_BEZIER`、`TYPE_AUDIO`、`TYPE_ANIMATION`（播另一個 `AnimationPlayer` 的動畫）。插值 `InterpolationType`（`:63-68`，含 cubic）、更新模式 `UPDATE_DISCRETE`／`UPDATE_CAPTURE`（`:73-76`）。
- **能對任何屬性做動畫的原因是反射**：軌道存 `NodePath path`（`animation.h:112-117`）；mixer 用 `get_node_and_resource` 拆出節點與屬性子路徑（`scene/animation/animation_mixer.cpp:733`、`:761`），每幀 `t_obj->set_indexed(t->subpath, value)`（`:1689`）。Godot 的 Object 系統用字串就能找到並寫入任何屬性。
- **音效與子動畫也是軌道**：`AudioKey` 帶 `Ref<Resource> stream`、`start_offset`、`end_offset`（`animation.h:223-229`）——音效可以從中間開始播；`AnimationTrack` 的 key 是動畫名（`:242-248`）。
- **時鐘**：`AnimationCallbackModeProcess` 有 physics、idle、manual（`scene/animation/animation_mixer.h:55-57`）；manual 由呼叫者 `advance(delta)`（`:460`）。
- **seek 與方法軌**：正常播放時觸發 `(time, time+delta]` 範圍內的方法 key（`animation_mixer.cpp:1724-1731`）；被外部 seek 時只呼叫最近的那一個 key（`FIND_MODE_NEAREST`，`:1716-1723`）。**在編輯器裡（預覽）方法軌不呼叫**：`can_call = is_inside_tree() && !Engine::is_editor_hint()`（`:1237`、`:1707-1711`）。
- **倒著播**：`play_backwards`（`scene/animation/animation_player.h:209`）、`seek`（`:239`）、`set_speed_scale`（`:225`）。
- **相機路徑**：`Curve3D::sample_baked_with_rotation`（`scene/resources/curve.h:372`）＋`PathFollow3D::set_progress`（`scene/3d/path_3d.h:75`、`:106`）：常見寫法是在 `Animation` 裡對 `progress` 做 value 軌〔知：寫法是社群慣例〕。
- **資料**：`Animation` 是 Resource，存成 `.tres`／`.res`，編輯器的動畫面板讀寫它〔知〕。

### 1.4 Bevy：`bevy_animation` 與生態

- **`AnimationClip` 能對任何 component 做動畫，但靠的是程式，不是資料**〔源〕：`AnimationClip{curves, events, duration}`（`crates/bevy_animation/src/lib.rs:105-111`）；curve 依 `AnimationTargetId`（UUID，`lib.rs:187`）找目標實體，實體用 `AnimatedBy(player)` 連到播放器（`lib.rs:215`）。對任意欄位的 curve 用 `animated_field!(Transform::translation)` 這個巨集，它產生一個「`&mut C` → `&mut 欄位`」的**閉包**（`animation_curves.rs:794-801`、`AnimatedField` `:215`）；`BlendableProperty::get_mut` 取可寫的屬性（`:190-202`）。`curves` 欄位標 `#[reflect(ignore)]`（`lib.rs:106-107`），事件也是閉包 `AnimationEventFn(Arc<dyn Fn(&mut Commands, Entity, f32, f32)>)`（`lib.rs:136`）——**閉包存不進檔案**。所以從檔案來的只有 glTF 的節點與 morph curve；「對任意屬性做動畫」的 clip 要由程式組出來。
- **事件**：`add_event(time, event)`（`lib.rs:326`），播放越過時以 `Commands` 觸發（observer 接）（`trigger_untargeted_animation_events`，`lib.rs:1003-1040`）。`set_seek_time` 跳過去「不觸發中間的事件」、`seek_to` 會在下一次更新觸發（`lib.rs:709-726`）——**跳轉時要不要補發，Bevy 讓呼叫者選**。
- **時鐘與排程**：`advance_animations` 讀 `Res<Time>`（`lib.rs:1045-1049`；`PostUpdate` 裡的預設 `Time` 是 virtual time，會暫停〔知〕），整組在 `PostUpdate`、`TransformSystems::Propagate` 之前（`lib.rs:1316-1335`）。速度可以是負的（`ActiveAnimation::update` 處理 `speed < 0`，`lib.rs:576-598`）。
- **曲線**：`bevy_curve` 有 `CubicBezier`、`CubicHermite`、`CubicCardinalSpline`（`crates/bevy_curve/src/cubic_splines/mod.rs:55`、`:146`、`:275`），可當相機路徑；`bevy_animation/src/gltf_curves.rs` 有 stepped、cubic、cubic rotation 等關鍵幀 curve（`:11`、`:49`、`:113`）。
- **核心沒有 timeline／sequencer**〔文〕：bevy 的 issue 搜「timeline」第一頁沒有 timeline／sequencer 的提案（<https://github.com/bevyengine/bevy/issues?q=timeline>）；相近的是 #19554（`AnimatableProperty` 存取 resource）、#15725（UI 的過渡動畫，S-Needs-Design）。
- **生態**〔文，只讀 README〕：
  - `bevy_tweening` 0.17（對 bevy 0.20）：一個 component 一個 tween，`Sequence`／`Delay` 串接，平行就是各自排一個；`Lens` trait 寫欄位；只有完成事件；在 `Update` 推進；看起來只有程式、沒有檔案格式（<https://github.com/djeedai/bevy_tweening>）。
  - `bevy_tween` 0.14：timeline 是實體樹，父實體的 `TimeRunner` 管子實體的時間；可平行、串接、重疊；有 loop、time scale、「向前／向後跳」「跳到任何時刻」；可在任何時刻發自訂事件；README 沒提檔案格式；標明「會有破壞性變更」（<https://github.com/Multirious/bevy_tween>）。
  - Bevy Assets 的 Animation 分類只有一個自稱 timeline 的：`bevy_ecs_animations`「ECS-first timeline management for animating anything」（<https://bevy.org/assets/#animation>，沒有打開）。**沒有找到把鏡頭路徑、字幕、音效放在一起的 cutscene crate**〔未確認：那一頁約 10k 字沒讀到〕。

### 1.5 比較

| | Unity Timeline | Unreal Sequencer | Godot Animation | Bevy |
|---|---|---|---|---|
| 位置 | 官方套件 | 引擎附帶 | 引擎核心 | 核心沒有；生態有 tween 類 |
| timeline 和骨骼動畫片段 | **分開**：Animation 軌道引用 AnimationClip | **分開**：Skeletal Animation 軌道引用 AnimSequence〔知〕 | **同一個** `Animation` 資源 | **同一個** `AnimationClip`（但任意屬性只能由程式組） |
| 寫到目標的方法 | binding＋各軌道的 Playable | binding（Possessable／Spawnable）＋各軌道 | 反射（`NodePath`＋`set_indexed`） | 閉包（`animated_field!`） |
| 資料進檔案 | 是（asset） | 是（asset） | 是（Resource） | 否（只有 glTF 來的） |
| 時鐘 | DSP／Game／Unscaled／Manual | Tick／Platform／Audio／Timecode | physics／idle／manual | `Res<Time>`（virtual） |
| 跳轉時的事件 | Retroactive 選項〔未確認意思〕 | Forwards／Backwards 屬性；跳轉〔未確認〕 | 外部 seek 只呼叫最近的 key | 呼叫者選（`set_seek_time`／`seek_to`） |
| 預覽時觸發事件 | 反應各自選「Editor and Runtime」 | 「Call In Editor」 | 編輯器裡不呼叫方法軌 | （沒有編輯器） |
| 相機接管 | Cinemachine 軌道覆蓋 Brain | Camera Cut 軌道；可關 | 動畫 `Camera3D.current` 或路徑〔知〕 | 作者寫 |
| 鎖輸入、藏 HUD | 作者寫〔知〕 | 播放設定內建 | 作者寫〔知〕 | 作者寫 |
| 跳過 | 改 `time`、`Evaluate`〔文〕 | 「Go to End and Stop」＋Keep／Restore State | `seek` | `seek_to`／`set_seek_time` |
| 網路 | 文件沒提 | Net Addressable、Client／Server 模擬；權威〔未確認〕 | 沒有 | 沒有 |

**看得出的共同點**（四家都有）：(1) 一份資料、多條軌道、各軌的 key；(2) 資料與「綁到哪些物件」分開（Godot 用相對路徑、Bevy 用 UUID，也是同一個意思）；(3) 播放頭可以暫停、改速度、跳轉；手動時鐘（manual）是三家都有的底；(4) 事件與連續值分開處理：連續值在任何時刻取樣，事件只在「越過」時觸發，跳轉時的規則要另外定。

**分歧**：timeline 與骨骼動畫片段是同一種資料（Godot、Bevy）還是 timeline 引用片段（Unity、Unreal）。第 6 節決定 GYO 走哪一邊。

## 2. 各歸哪一層

### 2.1 播放器

**前提**：取樣（在時刻 t 求每條軌的值）與「越過了哪些事件」是純計算，作者自己寫得出來；但每個要過場的產品都要它，而且它的資料格式要讓 Tool 與 Game 交接（§1-7），格式的持有者必須是一個大家都依賴的地方。**Engine 要提供的是機制（取樣、事件越過、播放頭、資料契約），不是過場的規則（何時播、播的時候遊戲怎麼樣）。** 四家裡三家把它放引擎或官方套件，Bevy 是唯一沒有的，生態補的兩個 crate 都沒有檔案格式。

推薦拆成兩層：

| 層 | 內容 | 放哪 |
|---|---|---|
| (a) 取樣機制 | 軌道與 key 的型別、插值（step、linear、cubic、slerp）、`Sample(track, t)`、`EventsCrossed(t0, t1, direction)`、播放頭（時刻、速度、暫停、wrap、seek 要不要補發事件）、`Validate`。不連 World、不讀時鐘 | 新的 Engine Module `timeline`（函式庫，`library_only`），同 `model` 的第 1 層 |
| (b) 寫進 World | 一個播放器元件（時刻、速度、binding 表）、每幀推進的 System、**binder 登記處**（軌道的種類名 → 把值寫進 World 的函式）、內建的 binder（`Transform`、`Camera` 的 active 與 projection、音效、UI binding、事件→`Message`） | 新的 Engine Module `world_timeline`（同 `world_ui`、`world_render` 的分法）；音效、UI 的 binder 分在各自的膠水裡，`world_timeline` 不依賴 audio、ui |

名字：`timeline`（Unity 的詞、中性）。不用 cutscene（「過場」是產品的詞）；sequence（Unreal）也是中性的詞，選 timeline 是因為它直接說出「照時間排的多條軌道」。

### 2.2 為什麼要 binder 登記處：GYO 沒有反射

Godot 能對任何屬性做動畫靠反射（第 1.3 節）；Bevy 靠閉包、所以任意屬性的 clip 進不了檔案（第 1.4 節）。GYO 的 Kernel 沒有反射（`engine/` 裡 `git grep -i reflect` 沒有結果）；元件只有登記名（例：`{.name = "object_fps_pvp.SplashState"}`）。

所以資料檔裡的一條軌道寫的是**種類名**（例：`"transform"`、`"camera.fov"`、`"sound"`、`"ui.binding"`、`"event"`）與**binding 名**（例：`"heli_camera"`），播放時：

1. binding 名 → 實體：由觸發的一方給（Unity 的 `SetGenericBinding`、Unreal 的 Possessable）；沒給的是失敗，告知、不中斷（E2）。
2. 種類名 → binder：作者在 `Configure` 登記（同 asset 的 `AssetLoaderRegistry::Register<…>()`）。Engine 的元件由 Engine 的膠水登記；產品的元件（直升機的旋翼轉速）由產品登記。沒登記的種類名是失敗、告知。

**和 Bevy 的衝突點與膠水量**：Bevy 把「怎麼寫」放進 curve 本身（閉包），GYO 改成資料裡只放名字、寫法查登記處——因為 §1-7 要求資料檔能單獨存在、Tool 不認得產品的程式。膠水是一張「名字 → 函式」的表與每個種類一個小函式（`Transform` 一個約 20 行，推估、未寫）。這和 Godot 的「字串找屬性」是同一個意思，只是登記是顯式的。

### 2.3 資料格式

**前提**：格式的持有者要是 Engine——Tool 與 Game 都依賴它，產品之間共用；格式的內容（這一段過場有哪些軌道）是作者的資料。

- Data Contract `gyo.timeline`（JSON，經 JSON Module；asset 型別 `timeline`；loader 由 `timeline` 提供、產品在 `Configure` 登記），持有者是 `timeline` Module。同 `gyo.ui`（`ui` Module 持有、資料在產品）。
- 檔案只寫中性的東西：時長、軌道（種類名、binding 名、key、插值）、事件 marker（名字、payload）。引用別的 asset（音效、模型片段）用 asset id。
- 產品的詞只出現在**值**裡（binding 名、事件名、line ID），不出現在格式的欄位名裡（§1-6）。

### 2.4 編輯工具

**前提**：作者能處理。Game 沒有工具也能跑（§1-7），工具是之後的事；它只認得 `gyo.timeline` 與 `timeline` 的取樣函式（預覽時用 manual 時鐘拖播放頭）。

- 照 Godot：預覽時不觸發遊戲的事件（第 1.3 節的 `can_call`）；照 Unity：要在預覽時也有反應的，由那個 binder 自己標明（第 1.1 節「Editor and Runtime」）。
- 工具在 `tools/`，不在 Engine（「Engine 不承擔工具職能」）。
- 什麼時候做、先用什麼頂：第 8 節 Q5。

### 2.5 遊戲的觸發

**前提**：作者能處理。「撤離成功時播」「開場播完切主選單」「多人時誰看得到」都是遊戲的規則。

- 產品的 System 收到自己的事件（撤離成功的 `Message`、`Scene::Splash` 的進場），生一個帶播放器元件與 binding 表的實體；播完（播放器發一個中性的「結束」訊息）或跳過時，產品切場景。
- 模式配置（`v8-arch-gyo2-mode-config`）決定這個模式掛不掛過場的 System 與哪些 timeline asset（核心 PvP 模式不掛）。

## 3. 和現有機制的關係

### 3.1 相機（`engine/modules/camera`）與「接管」

- 現況：`Camera` 有 `active`、`order`（camera.zh-Hant.md 第 2 節）；PvP 的 `PlaceCamera` 每幀把 `Camera3d` 放在自己的眼睛（`games/object_fps_pvp/src/client/ClientWorld.cpp:696-713`）。
- 推薦：過場用**另一台相機實體**（binding 名 `heli_camera`），timeline 寫它的 `Transform` 與 `Projection` 的 fov；切換用「activation」軌道寫兩台相機的 `active`（Unity 的 Activation 軌道、Unreal 的 Camera Cut）。遊戲相機的 `PlaceCamera` 不用改。
- 寬銀幕黑邊：`Letterbox`（CM-6）已經有了，timeline 可以開關它或改比例（2.39:1）。
- 「鎖輸入」：遊戲把自己的操作 System 掛 run 條件（Kernel 的帶值 run 條件在 #367 的 Kernel 步做，HANDOFF `v8-arch-gyo2-mode-config-u2`），或用 `state_machine` 多一個狀態。**不加進 timeline**：Unreal 能內建是因為它的 PlayerController 有「移動輸入」的概念；GYO 的 Engine 沒有 action map（交互研究 Q6 的缺口），加旗標就是 Engine 替作者決定輸入是什麼。
- 遊戲相機與過場相機之間的混合（Unreal 的 Can Blend、Cinemachine 的 Ease）：第一版不做；要做時是 timeline 的一種軌道（兩個 `Transform` 依權重 `Blend`），不需要 camera 改。

### 3.2 動畫取樣與混合（`engine/modules/model`）

- 現況：第 1 層是純函式（`SampleClip`、`Blend`、`ComputeModelSpace`），「播哪個片段、播到哪、權重」是呼叫者的；第 2 層（播放器）先不做（model.zh-Hant.md 第 4 節）。
- 角色的登機動畫：timeline 的「clip」軌道只給**（片段名、片段時刻、權重）**，寫進一個中性的元件；產品現有的擺姿勢 System 讀它、呼叫 `SampleClip`。timeline 不擺骨頭。這和 Unity 的 Animation 軌道「引用片段、交給 Animator」同形。
- 第 2 層做出來之後，這個元件可以改成直接寫第 2 層的播放器；timeline 的資料不必改。

### 3.3 音效（`engine/modules/audio`）

- 「sound」軌道的 key 越過時，binder 呼叫 `Submitter::Play`，`at`＝播放頭開始的時間基準讀數＋key 的時刻。混音器把時間點換成取樣位置（audio.zh-Hant.md 第 2 節），所以聲音對得準 key，不落在畫面的格點上。
- **缺口**：混音器只能「從 clip 的第一個 frame 播」（晚到的請求也是從頭播）。過場暫停後繼續、跳到中間、預覽時拖播放頭，都要「從 clip 的第 N 個 frame 開始」。Godot 的 `AudioKey::start_offset`（`animation.h:225`）、kira 的起始位置〔知〕有這個。另外 DI 3 定「暫停不作用在混音器上」：過場暫停時，配音要 `Stop`，繼續時從偏移再播。第 8 節 Q8。

### 3.4 UI 文字與字幕（`world_ui`、`text`）

- 字幕軌道是「字串、step 插值」的軌道：key 的值是 line ID（Ink 的研究 Q4 已定 line ID 的約定）；binder 寫進產品的 `UiBindings`（例：`subtitle`），UI 文件照 binding 顯示。多語系等字串表（RC-INK4）。
- **缺口**：淡入淡出（開場 logo、轉場黑幕）要連續地改元素的 alpha。`gyo.ui` 的顏色來源只有 `color` 與 `select`（ui.zh-Hant.md 第 4.1 節），沒有「binding 給的顏色或不透明度」。兩條路：(甲) `gyo.ui` 加一種顏色來源（binding 的顏色，或元素的 opacity binding）——改資料契約、加版本；(乙) 用渲染管線 A2 的全畫面 pass 做黑幕（只能整個畫面）。第 8 節 Q6 的開場例子要先選一條。

### 3.5 時鐘（`world_time`）

見第 4 節。

### 3.6 Ink 台本（暫停中）、交互事件、模式配置

| | 負責 | 例子 |
|---|---|---|
| Ink（`plugins/ink`） | 有分支、等玩家選的對話；推進由作者的 System 每幀呼叫 `Advance` | 商人問「要買劍嗎」 |
| timeline | 線性、照時間走、不等玩家（最多「可跳過」） | 直升機起飛 |
| 交互（PvP 的門，研究 Q1、Q2） | 玩家的動作觸發遊戲狀態改變 | 按 E 開門 |
| 模式配置 | 這一場掛哪些系統、用哪些資料 | 撤離模式掛過場、核心 PvP 不掛 |

接法都是產品的膠水：Ink 的指令（`play_timeline("heli")`，Ink 研究的指令機制）讓產品開一段 timeline；timeline 的事件 marker（`resume_dialogue`）由 binder 變成產品的 `Message`，產品再推 Ink。交互觸發的短動畫（門打開）通常不是 timeline，是遊戲狀態＋模型動畫；要做成 timeline 也行，但門的開關狀態仍在模擬裡，timeline 只演出。

## 4. 時間怎麼推進

**前提**：「用哪個時鐘」是作者的選擇（四家都給選項）；Engine 要做到的是取樣不綁時鐘。

### 4.1 推薦

- `timeline` 的取樣與播放頭**不讀時鐘**：播放頭的推進函式收 delta（三家的 manual 模式；同 `model` 第 1 層「沒有時鐘」）。
- `world_timeline` 的預設 System 用 **Frame World 的 `ActiveTime`（每幀的、可暫停的 virtual time）** 推進，掛在 `Main`／`PostUpdate`、`TransformPropagate` 之前（照 Bevy 的 `advance_animations` 的位置，`lib.rs:1316-1335`）。所以遊戲暫停（`TimeControl`）時過場也停；要「暫停選單開著時過場照跑」的作者，用選項換成 `RealTime`。
- **不用模擬的 tick**：表演性的過場只在 Client 播、不進模擬、不進一致性雜湊。PvP 的 Client 的模擬在另一個 World、另一條執行緒（`ClientWorld.cpp:190-228` 的 Sim World／Frame World），過場屬於 Frame World。
- 音效的時間點用時間基準（第 3.3 節），所以畫面掉幀時聲音照樣準。Unity 的 `DSPClock`、Unreal 的 Audio clock 是「用音訊時鐘推進畫面」；GYO 反過來「把畫面的時刻換成音訊的位置」，混音器已經做了，不必另加時鐘。

### 4.2 影響模擬的「劇本」不是過場

直升機真的停在撤離點、玩家要在它起飛前上去——這是**遊戲狀態**，要在 Match 的模擬裡（tick、權威）；Client 的過場只演出。`timeline` 的取樣是純函式，所以同一份資料**可以**在 Sim World 的固定步裡用 `ActiveTime` 驅動（照 tick），但浮點在各平台是否逐位元一致沒有驗過（`determinism.md` 的 DET-0 仍未解）。第一版不承諾，第 8 節 Q3。

### 4.3 跳過、暫停、倒轉預覽

| 操作 | 推薦的規則 | 根據 |
|---|---|---|
| 暫停 | 速度 0 或 virtual time 暫停；連續值停在當下；配音要 `Stop`（第 3.3 節） | 四家都有 |
| 改速度 | 播放頭的 `speed`，可為負 | Bevy `speed < 0`（`lib.rs:588`）、Godot `play_backwards` |
| 跳轉 | `Seek(t, events)`：`events` 是「不補發」或「補發越過的」 | Bevy 的 `set_seek_time`／`seek_to`（`lib.rs:709-726`） |
| 跳過 | 跳到結尾、**套用最後的值**、只發標了「跳過時也要發」的事件（例：切結算畫面） | Unreal 的「Go to End and Stop」＋Keep State |
| 倒轉預覽（工具） | 連續值在任何 t 都能取樣，所以倒轉只是 `Sample(t)`；事件預設不觸發，軌道可標方向 | Unreal 的 Forwards／Backwards、Godot 的編輯器不呼叫方法軌 |

## 5. 資料格式要不要和動畫片段共用

**前提**：Engine 的事（兩個 Module 的資料契約怎麼分）。

| 選項 | 好處 | 代價 |
|---|---|---|
| (甲) 分開：`gyo.timeline` 引用 `model` 的片段（asset id＋片段名）；共用的只有插值的數學 | 和 Unity、Unreal 同形；`model` 的片段照舊從 FBX 來、不動已核准的 `fbx` 第 1 版；timeline 的軌道種類可以自由加 | 和 Bevy（主參考）的「一個 `AnimationClip`」不同；兩種資料 |
| (乙) 合一：`model` 的 `AnimationClip` 擴成任意屬性的軌道（Godot、Bevy 的形） | 一種資料；編輯器一個 | `model` 是不依賴 World 的函式庫，任意屬性的軌道需要 binder（World 的東西）；`AnimationClip` 現在只有 TRS 三個 channel（model.zh-Hant.md 第 2.1 節），擴充要改已核准的型別；FBX 不帶字幕、事件 |
| (丙) 都不共用，連插值也各寫一份 | 最少耦合 | `SampleClip` 的二分搜尋、slerp 寫兩份，之後會分歧 |

**推薦 (甲)**。和 Bevy 的衝突點：Bevy 能合一，是因為它的 curve 帶閉包、clip 不必進檔案；GYO 的 timeline 必須進檔案（§1-7），而骨骼片段來自 FBX。膠水：把 `model` 的 key 搜尋與插值（`Animation.hpp` 的內部）提到兩邊都能用的地方（`timeline` 依賴它，或放 core 的 math），`SampleClip` 的行為不變。

master 時代的 `gyo.anim_events`（`docs/architecture/animation-events.md`）是「片段上的具名時刻」，用片段名引用片段、意思由產品定——和 (甲) 的引用方式相同，可以當歷史參考（不是 gyo2 的決定）。

## 6. 鏡頭路徑從哪裡來

### 6.1 兩種來源

| 來源 | 做法 | 好處 | 代價 |
|---|---|---|---|
| (A) timeline 裡的 key | `Transform` 軌道：位置用 cubic（Catmull-Rom／Hermite，Bevy `bevy_curve` 有），旋轉 slerp；或「路徑＋沿路徑的進度」兩條軌（Godot 的 `PathFollow3D`） | 不需要別的軟體；手寫 JSON 就能做簡單的飛越 | 沒有工具時很難調；要自己算朝向 |
| (B) DCC 的相機動畫 | Blender 做相機動畫匯出 FBX；`model` 的 loader 把相機節點的動畫烘成片段（每秒 60 key，model.zh-Hant.md 第 3 節）；timeline 的「node」軌道引用那個片段的某個節點，取樣後寫進相機的 `Transform` | 編輯器是 Blender（專案的模型契約範例已經用 Blender 4.5 做）；不必先寫工具 | 只有 `Transform`；fov 不在 `ModelData` 裡（ufbx 能讀相機屬性〔知〕，要加就是 `fbx` 格式加版本）；座標要對到地圖（Blender 裡要有地圖的參考） |

### 6.2 推薦

先 (A)＋(B) 的「node」軌道都做（兩者都是「在 t 取 `Transform`」，差別只在 key 從哪來）。撤離例子的鏡頭路徑用 (B)；開場只有 UI，用不到。編輯工具（第 2.4 節）出來之前，複雜的鏡頭靠 Blender。

## 7. 畫面品質依賴渲染管線的哪幾批

**前提**：渲染是 Engine 的事，已有總宣告（render-pipeline-declaration.zh-Hant.md）；這裡只列過場會用到的。timeline 機制本身不依賴渲染。

| 過場想要的 | 批 | 先做 timeline 的話缺什麼 |
|---|---|---|
| 線性 HDR、exposure | A4 render-hdr-tonemap（A 段進行中） | 沒有「曝光」軌道可寫；畫面照現在 |
| 黑幕淡入淡出（全畫面） | A2 render-phases 的全畫面 pass，或 UI（第 3.4 節） | A2 之前只能用 UI 的不透明矩形，但 UI 沒有連續 alpha |
| 受光、陰影、天空 | B4、C1、C2、C3 | 直升機與角色是 unlit：形狀與動作看得出，沒有立體感 |
| color grading、bloom、霧 | D1 | 沒有「電影感」的調色；`ColorGrading` 的軌道等 D1 |
| 相機快速移動時的鋸齒 | D2、D3（TAA） | 邊緣閃爍 |
| 旋翼的塵土 | E1 render-particles | 沒有粒子 |
| 景深（DOF）、動態模糊 | **不在清單**（render-pipeline-declaration.zh-Hant.md:257：「Bevy 有，清單沒點名」） | 過場常用；要不要加進渲染管線的清單是第 8 節 Q7 |
| 寬銀幕黑邊 | 已有（`Letterbox`，CM-6） | 不缺 |

**結論**：timeline 先做不會白做——機制（取樣、binding、事件、音效、字幕、相機切換）在 unlit 下就能完整驗收；渲染的批做完後，畫面自動變好，資料不用改。只有「寫渲染參數」的軌道（曝光、光的強度、grading）要等對應的批，那些軌道是之後加的 binder，不改格式。

## 8. 選項、推薦、分批、決策點

### 8.1 推薦的整體形

```text
games/object_fps_pvp（觸發、binding、產品的 binder、跳過的按鍵、鎖輸入）
        │
world_timeline（播放器元件、推進 System、binder 登記處、Transform／Camera／事件的 binder）
   │        └── 音效、UI 的 binder 在各自的膠水（不讓 world_timeline 依賴 audio、ui）
timeline（函式庫：軌道、key、插值、事件越過、播放頭、gyo.timeline 的 codec 與 loader）
        │
math、json、asset

tools/（之後）：timeline 編輯器，只認 gyo.timeline 與 timeline 的取樣函式
```

### 8.2 分批

模型／檔位照 navigator：上段不用小模型；實作與設計審查 Opus／high，純淨 Code Review 由主會話照規則排（Fable／high）。估時是粗估（未量），只用來排序。

| 批 | 內容 | 段別（根據） | 實作者／審查者／驗證者（模型／檔位） | 前置 | 估 |
|---|---|---|---|---|---|
| T1 `timeline-core` | `timeline` 函式庫：軌道型別（float、vec3、quat、color、bool、string、event）、插值、`Sample`、`EventsCrossed`、播放頭（速度、wrap、seek 兩種、跳過）、`Validate`；從 `model` 提出共用的 key 搜尋與插值（`SampleClip` 行為不變） | 上段（公開介面；動到 `model` 的內部） | Opus／high、Opus／high、Opus | — | 10～16 h |
| T2 `timeline-contract` | `gyo.timeline` v1 的 codec、loader（型別 `timeline`）、資料契約測試、範例檔 | 上段（資料形式） | Opus／high、Opus／high、Opus | T1 | 8～12 h |
| T3 `world-timeline` | 播放器元件、推進 System（預設 `ActiveTime`、可換）、binding 表、binder 登記處、內建 binder（`Transform`、`Camera::active`、`Projection` 的 fov、`Letterbox`、事件→`Message`、「model 節點」軌道）；音效與 UI binding 的 binder 放膠水 | 上段（公開介面） | Opus／high、Opus／high、Opus | T2 | 14～20 h |
| T4 產品的第一個用例 | 依 Q6：開場改成 timeline（logo 淡入、字、時間到切主選單、任意鍵跳過），或撤離過場 | 下段（產品內；不改 Engine 的介面） | Opus／high、Opus／high、Opus | T3（＋Q6 的缺口） | 6～10 h |
| （之後）T5 編輯器 | `tools/` 的 timeline 編輯器 | 依 Q5 | — | T2 | 未估 |

缺口的批（Q6、Q8 選了才有）：UI 的 alpha（`gyo.ui` 加版本，上段，Opus／high）；混音器從偏移開始播（audio 的公開介面，上段，Opus／high）。

PR：照 `fewer-prs`，T1～T3 一條線一個 PR、各批各自 commit；T4 跟產品的線。

### 8.3 要使用者決定的點

| # | 問題 | 選項 | 推薦與理由 |
|---|---|---|---|
| Q1 | 播放器放哪 | (a) Engine：`timeline`＋`world_timeline`（第 2.1 節）；(b) 只在 `object_fps_pvp` 裡寫 | **(a)**。三家放引擎或官方套件；格式要讓 Tool 與 Game 交接（§1-7）；「建機制這一輪保守的選項會被上層蓋住」（`gyo2-mechanism-not-conservative`）。(b) 便宜，但第二個產品要重寫、格式沒有持有者 |
| Q2 | 和動畫片段共用格式 | (甲) 分開、引用片段、共用插值；(乙) 合一；(丙) 全不共用 | **(甲)**（第 5 節）。偏離 Bevy 的理由：Bevy 的任意屬性 clip 進不了檔案；膠水＝提出共用插值 |
| Q3 | 時鐘 | (a) 只在 Client 的 Frame World、預設 virtual time、可換 real time；(b) 現在就承諾也能在 Sim World 照 tick 跑（要驗跨平台一致） | **(a)**，取樣保持純函式、不擋 (b)。(b) 要先解 DET-0 類的浮點一致性，沒有點名的需求 |
| Q4 | 鏡頭路徑的來源 | (A) timeline 的 key；(B) Blender 的 FBX 相機動畫；(A)＋(B) | **(A)＋(B)**（第 6 節）。(B) 讓 Blender 當編輯器；fov 要不要從 FBX 讀另外再定 |
| Q5 | 編輯工具什麼時候做 | (a) 先不做：手寫 JSON＋Blender＋（B3 之後）熱重載；(b) 和 T1～T3 同時做 | **(a)**，但不是因為「先手動」：成熟方案（Unity、Unreal、Godot）都有編輯器，所以工具遲早要；現在沒有第二個過場、預覽要的渲染（光照）也還沒有，先做工具會在 unlit 的畫面上調鏡頭。使用者若要照「有成熟方案就直接做成工具」，選 (b) |
| Q6 | 第一個用例 | (a) 開場（`Scene::Splash` 換成 timeline）；(b) 撤離過場 | **(a)**。撤離現在只是暫停選單的一個按鈕（`game-flow-declaration.zh-Hant.md:102`），沒有直升機、沒有撤離點；開場已經存在、小、能驗字幕／淡入／跳過／切場景。代價：淡入需要 UI 的 alpha（第 3.4 節的 (甲)），要一起定 |
| Q7 | 景深、動態模糊 | (a) 加進渲染管線的清單（新的批）；(b) 先不加 | 交使用者：渲染總宣告已定「清單照原樣全做」，加項目是擴大那條線的範圍。過場是第一個點名它們的 Feature |
| Q8 | 音效從中間開始播 | (a) 混音器加起始偏移（T3 或之前）；(b) 先不做：暫停、跳轉時配音只停、不續播 | **(a)**，在 T4 之前。暫停後繼續、拖預覽都要；Godot 的 `AudioKey::start_offset` 有這個。改 audio 的公開介面（上段） |

### 8.4 本研究沒有做的

- 沒有讀 Unreal 的 Transform／Audio／Fade／Subtitles 軌道各頁（軌道清單 <https://dev.epicgames.com/documentation/unreal-engine/sequencer-track-list-in-unreal-engine>）。
- 沒有打開 `bevy_ecs_animations`、`bevy_time_runner`。
- 沒有查第三方的 C++ timeline 函式庫能不能當 Plugin 接（沒有找到常見的候選〔未確認〕）。
- Unity Signal 的 Retroactive、Emit Once 的意思，Unreal 的 `bReplicatePlayback`：查不到原文。
