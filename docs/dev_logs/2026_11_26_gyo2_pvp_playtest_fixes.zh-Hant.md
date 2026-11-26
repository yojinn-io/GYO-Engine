# 2026-11-26 LAN 實機場次的 8 項回饋（`v8-product-pvp-playtest-fixes`）

Owner：產品（Object_FPS_PVP）。GYO2 主會話開的任務卡；決定與原話在 `docs/architecture/target/v2/HANDOFF.md` 的 `v8-arch-gyo2-lan-playtest-2026-11-26`，證據在主 checkout 的 `build/evidence/gyo2-lan-session-20261126/`（`REPORT.ja.md`）。作者 Opus 5.5／high（上段：arena／角色／sfx 的資料格式）。一條 PR 線，三段：A 聲音（1–3）、B 移動規則（4、5、7）、C HUD／流程（6、8）。

## A 聲音

### 2 蹲下不出腳步聲；別人的腳步聲

- 原狀（gyo2 `280c27a6`）：`PlaySounds` 只看自己的身體（`FootstepTracker` 一個），不看蹲姿。
- `FootstepTracker::Observe` 多吃 `crouched`：蹲著走不出聲，蹲著走的距離不算進下一步（站起來後重新數一步）；蹲著從台子掉下去照樣有落地聲。
- `ClientSound::steps` 改成每個玩家一個 tracker：自己的照舊置中；別人的照 `HeardGains`（空間化＋距離＋遮擋，見 3）在對方身體中段發聲。看不到的玩家（離開、不再複製）的 tracker 與開槍紀錄一起清掉，回來時重新開始、不補播。
- 對方的位置用 Frame World 的 `PlayerBody`（內插後顯示的位置），腳步跟畫面一致。

### 3 遮擋與最大可聽距離

- `audio/sfx.json` 升到 **version 4**：每個 cue 可帶 `maximum_distance`（沒有＝不限距離）、`spatial.occluded_gain`（必填，0～1）。只接受 version 4（照這個檔原來的規則：版本不同就拒絕）。
- 預設值：`remote_shot` 200 m（最大的地圖 250 m 見方，對角約 350 m：地圖另一頭的槍聲聽不到，半張地圖內聽得到）；`footstep` 30 m、`landing` 40 m（大約一條街；落地比腳步重，遠一點）；`occluded_gain` 0.25（約 −12 dB：牆後還聽得出方向，但明顯比沒擋時小）。都是依地圖尺度定的起點，未經實機調整。
- 遮擋：`world::Blocked(arena, a, b)`：從聽者的眼睛到聲源（對方身體中段）的線段被 arena 的牆（`InstalledArena::walls` 的 BoxSet）切到，就算擋住；只碰到線段終點（聲源貼著牆）不算。用 physics_3d 現有的 `CastRay(Shape, Pose, Ray, maxT)`，沒有改 Engine。只乘增益，不做濾波（卡片的指定）。
- `HeardGains(cue, ...)`：超過該 cue 的最大距離回 nullopt（不播），否則 `SpatialGains`×（擋住時）`occluded_gain`。

### 測試與突變（A）

- 邏輯：「a crouched walk is silent…」「heard gains: …maximum distance…occluded gain」，sfx 的解析加三個壞值（沒有 occluded_gain、大於 1、maximum_distance 0）。
- World：「the arena stands between two points…」（出貨的 pvp_arena 的柱子：擋住、上方／旁邊／停在前面不擋、貼著面不擋）。
- Client：「the other player's steps sound while it walks standing, not while it walks crouched」（另一個 Client 蹲著走兩秒：Mixer 送出的聲音數不變；站起來走一秒：增加）。
- 突變 8 個，8/8 killed：`pvp-crouch-steps-sound`、`pvp-crouch-distance-kept`、`pvp-sound-no-range`、`pvp-sound-no-occlusion`、`pvp-blocked-ignores-walls`、`pvp-blocked-at-touch`、`pvp-remote-steps-silent`、`pvp-remote-crouch-sounds`。
- 沒有外部測試的接線：`PlaySounds` 把 `Blocked` 的結果交給 `HeardGains` 那一行（Mixer 只數送出幾個，量不到增益）；兩端各自有測試。

## B 移動規則

都在 sim 共用的 `MovePlayers`／`StepBody`（Match 與 Client 預測同一份；Client 照舊預測自己的 `PlayerBody` 與 `PlayerCombat`）。

### 4 地圖邊界＝隱形牆＋底部死亡線（使用者已決定）

- arena 檔升到 **version 4**（只接受 4）。新成員：`floor`（可省：地板平面的高度；省略＝沒有地板平面，靠地圖自己的地面）、`kill_height`（必填：腳低於它就死；要低於地板與每個重生點）、`fall_damage`（見 5）。
- **隱形牆**：腳的 x、z 夾在 `[radius, width − radius] × [radius, depth − radius]`（`InsideBounds`，MoveAndSlide 之後）：撞上去只停那一軸，另一軸照走（沿牆滑）。跟重生點的檢查是同一個範圍。
- **地板平面變成 arena 資料**：原本「y = 0 的地板」是寫死在 `StepBody`／`Fall`／判定射擊的規則，而且不分 arena 範圍內外。地圖本身有地面的 arena（kiriyu 三張）在 y = 0 也有一層看不到的地板，所以從邊緣走出去、或掉進地面板（約 y 9～11.5）底下，會站在 y = 0（玩家回報 3 的「我在地下」很可能就是這裡）。有這層地板，死亡線永遠碰不到；所以：
  - `pvp_arena`、`pvp_corners`、`biome` 留 `"floor": 0`：它們靠這層地板（前兩張沒有地板方塊；biome 約 1/3 的格子底下沒有方塊）。
  - `kiriyu_extraction_250m`、`kiriyu_incident_250m`、`kiriyu_town_graybox` 不設 floor：每一格都有方塊（extraction 的地下室有自己的 0～0.5 地板、town 的地下室到 −3）；town 的 −3 地下室原本被 y = 0 的地板擋在上面，現在走得到。incident 有 1 格（0.5 m）沒有任何方塊，是地圖本身的洞；身體半徑 0.25 m，可能掉得下去（掉下去就死）。
  - `kill_height`：地面最低處再往下 10 m（training 三張 −10、extraction／incident −10、town −13）。
- 死於死亡線：HP 歸 0，`Die`（和被打死同一個函式：`respawnTick`、丟掉裝填中），之後照舊由 `RespawnPlayers` 重生。死後身體照舊只受重力，3 秒內重生。

### 5 墜落傷害

- `fall_damage: {"safe_height": 4.0, "points_per_metre": 15}`（點數；內部是 milli-points，HP 100 = 100000）。落地速度 v 換成落差 v²/2g，超過安全高度的公尺數 × 每公尺點數（無條件捨去）。不另存「從哪裡開始掉」：落地那一步的下落速度就決定落差（`Fall` 停在地板時把速度記在 `VerticalStep::impact`，撞上方塊時用那一步的下落速度），兩端算出來一樣。
- 預設值的依據：跳躍 0.6 m 永遠不痛；town 的屋頂離地約 5～8 m，跳下來扣 15～60；10 m 扣約 90；約 10.7 m 以上致死（例：extraction 從地面掉進地下室約 11 m）。安全高度 4 m ≈ 一層樓多一點。這是依地圖尺度定的起點，未經實機調整；之後改 arena 檔就好。
- 落地傷害讓 HP 歸 0 也走 `Die`。墜落不是命中：不寫 `HitRecord`，沒有紅閃與命中音。

### 7 按住空白鍵連跳

- `rules/movement_rules.json` 升到 **version 2**，加 `hold_jump`（必填 bool；出貨 true）。放在角色的移動規則（所有 arena 共用），之後技能改的是這個值（`StanceRules::holdJump`）。
- 規則：`sim::StepJumps(pressed, held, rules) = pressed || (holdJump && held)`。Client 的 Controls 多帶「空白鍵按著」（`StepInput::jumpHeld`，跟 crouch 一樣取最新），Sim／Solo World 寫每一步的輸入時套這條規則，答案放進原本的 `PlayerInput::jump`（一步一次的按下）。**網路的 input 格式不變**；Match 照舊「著地時有 jump 就跳」，所以按住時一著地就再跳。
- 控制過期（stale：Frame World 停住）時，按住的跳和按下一樣丟掉（不會在畫面停住時一直跳）。

### 測試與突變（B）

- 邏輯：arena v4 的解析（floor／kill_height／fall_damage 的正反例、v3 帶 v4 成員也拒絕）、`Fall` 的有無地板與 impact、`LandingDamage`／`BelowKillHeight`、`StepJumps`、movement_rules v2（缺 `hold_jump`、非 bool、v1 拒絕）、Controls 帶 `jumpHeld`。
- World：「the arena's bounds are an invisible wall…」「a body that falls below the kill height dies…」「a landing from above the safe height costs HP…」（沒有牆、只有邊界的 arena：直走停在半徑、斜走沿牆滑；沒有地板：掉下去在死亡線下一步內死；4 m 內不扣、10 m 扣約 90 點（±8%，最後一步的速度略大）、30 m 死）。
- Client：「holding Space jumps again on every landing; letting go stops」（按住 2 秒起跳 ≥ 3 次、放開不再跳、點一下跳一次）；J5（主執行緒停住）加上按住空白鍵：停住前每步都要跳，過期後不跳。
- 突變 12 個新的＋改 1 個舊的（`pvp-arena-version`：改成「讀了 version 3」），13/13 killed：`pvp-bounds-open`、`pvp-bounds-at-edge`、`pvp-kill-height-ignored`、`pvp-floor-everywhere`、`pvp-landing-unhurt`、`pvp-landing-speed-lost`、`pvp-fall-no-death`、`pvp-fall-safe-ignored`、`pvp-hold-jump-ignored`、`pvp-hold-jump-always`、`pvp-hold-not-carried`、`pvp-hold-kept-stale`。第一次跑 `pvp-arena-version` 存活：v3 的測試文字缺 `kill_height`，本來就會被拒；測試改成帶 v4 成員的 v3 後 killed。

## C HUD／流程

### 8 命中標記（類 COD）

- 自己的一槍打中人時，準心四周的斜角出現一個 X（四條斜線，從中心 6 px 到 15 px、2 px 粗），0.25 秒內淡出；爆頭換色（白→紅 `[1.0, 0.25, 0.2]`）。
- 來源：Match 的 `ShotRecord`（複製到 Client）。`present::HitMarker` 照 tick 判「新的一槍」：新的命中就顯示；新的沒中不把正在顯示的拿掉；第一次看到的紀錄（加入、換場景）不顯示。和命中音（`PlaySounds` 的 `confirmedHeard`）看同一份紀錄，各自記。
- `ui/hit_feedback.json` 升到 **version 2**，加 `hit_marker`（`gap_pixels`、`length_pixels`、`thickness_pixels`、`color`、`head_color`、`seconds`，全部必填）。只接受 version 2。
- 畫法：HUD 層一個 `Shape2d`（`kHitMarker`），四條斜線各是一個沿線段的四邊形（`Stroke`）。

### 6 啟動畫面（先用文字）

- 新場景 `Scene::Splash`：沒有 `--connect` 時程式的第一個場景。Frame World 放 `ui/pvp_splash.json`（canvas `splash`：黑底、「GYO」160 pt、「ENGINE」28 pt），文件排好之後 `ClientStart::splashSeconds`（2 秒）到了、或按任何鍵／滑鼠鍵，就 `SceneRequest{Menu}`。秒數從排好算起（載入慢不會把標識吃掉）。
- 接現有的流程框架：跳轉是「場景之間」那一層（`SceneRequest` → 兩幀之間 `ChangeScene`），沒有另一套導航；回主菜單時不會再看到。之後換圖檔：文件的 `text` 換成 `image`（加一個資產），程式不動。
- `game-flow-declaration.zh-Hant.md` 修訂 7 記了這一條。`ClientStart::first` 的預設仍是 `Menu`（現有的測試與 `acceptance_macos.sh` 從主菜單開始）；`client_main` 設 `Splash`。

### 測試與突變（C）

- 邏輯：「Hit marker: a newer own shot that hit a player shows it…」（第一筆不顯示、沒中不顯示、命中顯示並淡出、同一筆不重播、爆頭換色、沒中不拿掉）；`hit_feedback.json` 的解析加三個壞值（沒有 `hit_marker`、`gap_pixels` 0、`head_color` 兩個分量）。
- Client：「an own shot that hits shows the hit marker, in the head colour for a head; it fades」（在 Match 寫 `ShotRecord`，照 `JudgeShots` 的寫法：Frame World 畫出斜線的形狀、顏色對、淡出後沒有；爆頭時是頭的顏色）；「the start screen shows the logo document, then the menu after its seconds or at once on a key」（0.5 秒：24 幀時還在、40 幀時到主菜單；60 秒：30 幀後還在、按 Enter 就到主菜單；主菜單的文件排得出來）。
- 突變 9 個，9/9 killed：`pvp-marker-first-shown`、`pvp-marker-replayed`、`pvp-marker-miss-clears`、`pvp-marker-no-head`、`pvp-marker-not-drawn`、`pvp-marker-never-fades`、`pvp-splash-no-wait`、`pvp-splash-stays`、`pvp-splash-unskippable`。`pvp-splash-no-wait` 第一次是 wrong_failure（測試在「排好時還是啟動畫面」用了沒有標籤的 REQUIRE），改成帶 `[pvp-splash-time]` 的訊息後 killed。
- 畫面的樣子（X 的大小、標識的字）沒有在真的視窗看過，交使用者試玩。

### 1 遠距離擊中聲（使用者的決定：B「照距離」）

- 主會話轉來：使用者原話「這個選B」。B＝保留肉聲，但遠了就聽不到；打遠處的敵人時只能靠準心標記或敵人的反應判斷有沒有打中。比過的選項：A＝UI「嗒」聲不衰減＋肉聲空間化；C＝兩者都要。不加 UI 命中音；第 8 項的命中標記是唯一不受距離影響的命中回饋。
- 做法：給射手的 `hit_confirmed`（`hit_flesh`）不再 `CentredGains` 置中，改在被擊者身體中段照 `HeardGains` 播（和 3 同一套：距離衰減、最大距離、遮擋）。`sfx.json` 的 `hit_confirmed` 加 `maximum_distance` 40 m（和落地聲同級：肉聲比槍聲輕）。Client 看不到被擊者（不在 Frame World）時不播。
- 測試：Client「an own hit sounds at the target: heard near, not across the big map」（pvp_arena 的兩個出生點約 7 m：送出一個聲音；`kiriyu_extraction_250m` 的兩個出生點約 200 m：沒有聲音，命中標記照樣出現）。
- 突變 2 個，2/2 killed：`pvp-hit-sound-centred`（照舊置中）、`pvp-hit-sound-silent`（不播）。

## 設計審查（Opus／high，作者同模型；審查者只拿差分與原始要求）

blocker 0。處理：
- S1（改）：墜落傷害只測了「落在地板平面」，沒測「落在地圖自己的地面」（kiriyu 三張只有這條路；`descent` 在那裡只靠 `-fall.velocity`）。world 測試改成兩種都跑（加一塊頂在 y = 0 的地面、沒有地板平面），新突變 `pvp-landing-on-map-unhurt`。
- 6（改）：`points_per_metre` 的上限原本寫「一個身體的 HP（100000）」，和 `CombatRules` 的預設綁在一起；改成只要放得進 `std::uint32_t`（`LandingDamage` 本來就會飽和）。
- 10（改）：啟動畫面的文件失敗時，原本黑畫面停到按鍵為止；改成失敗就直接去主菜單（原因照舊由 `WatchDocument` 記進 `FrameNotes::failure`）。**這條沒有測試**：文件 id 寫在 `BuildSplash`，測試換不了一份壞文件。
- S2（接受，記下）：按住空白鍵連跳時，Match 少收到一步輸入（遲到、遺失）就用 held 填（`InputOptions` 把一步一次的按下清掉），那一步不跳；Client 預測了跳，被校正一次（輕微的拉回）。這是「網路的 input 格式不變」的代價；要消掉就得讓 held 填照 `holdJump` 保留 jump，或在輸入帶「按著」——都是改輸入的語意，交主會話。
- S3（記下）：`hold_jump` 是角色規則，但只有寫輸入的 Client 端套它，Match 不看。之後若技能在 Match 上按玩家改它，Client 要從複製的狀態讀同一個值。另外，按住空白鍵時站在地上按蹲，每一步都先跳（`jumpFromGround` 優先），不是只有按下那一步。
- S4（記下）：`PlaySounds` 的遮擋與最大距離接線沒有經 Client 測到增益（Mixer 只數送出幾個）；最大距離這次由項目 1 的測試經 Client 測到，遮擋的接線仍只有兩端各自的測試。
- S5（已查）：kiriyu 三張每一格都有方塊（見 B 的 4），incident 有 1 格洞。
- 7（接受）：落地速度用那一步結束時的速度，比真正的落差多算一點（10 m 約多 0.3 m）；兩端一樣，不會分歧。
- 8（記下，後續）：墜落扣血沒有紅閃、命中音或 HP 變紅（不寫 `HitRecord`）；要不要另給墜落的回饋由使用者決定。
- 9（記下）：啟動畫面的秒數在 `ClientStart`，不在資料；換成動畫時可能要搬。
- 11（照讀法）：蹲著落地仍有落地聲（使用者的原話只說蹲下移動）。
- 12、13：沒有重現（邊界外緊貼牆時夾回去可能和牆重疊；測試找命中標記的方法靠「斜線」）。

## 可攜模式（使用者追加：照 Godot）

- 主會話轉來：使用者原話「選1，在 #360 裡一起改」。比過的選項：(1) Godot 式可攜模式；(2) 永遠放在執行檔旁邊。選 (1) 的理由：Program Files 和 macOS 的 `.app` 是唯讀的，Gatekeeper 還會做路徑隨機化；更新時整個資料夾被換掉，設定和存檔也跟著消失。
- 做法（`client/LocalFiles.hpp` 的 `PortableDirectory`、`ChooseDataPlace`，`client_main.cpp`）：預設照舊 `OS::Files::UserDataDirectory("GYO", GYO_GAME_ID)`；執行檔所在目錄有空檔 `portable` 時，`settings.json` 和各 profile 的資料夾（`<dir>/<profile>`）改放那裡。執行檔在 `X.app/Contents/MacOS` 裡時，到 `.app` 外面那一層找標記、也放在那一層（Godot `editor/file_system/editor_paths.cpp:144-160` 的 self-contained mode；Godot 只看 `MacOS`／`Contents` 兩層，這裡多要求上一層以 `.app` 結尾）。兩種分隔符都認（Windows 的路徑）。
- **Engine 不改**：OS 層已有三平台的 `OS::Process::ExecutablePath()`（Windows 的 module path、Linux 的 `/proc/self/exe`、macOS 的 image path）；「所在目錄」與選哪一條由遊戲決定。拿不到執行檔路徑時不進可攜模式（記警告）。
- log：啟動時印一行「portable mode: the files are beside the program, in …」或「the files are in the user's data directory, …」。
- 測試：Client「portable mode: the marker beside the program puts the files there…」（沒有標記→使用者資料目錄；有→執行檔旁；別處的標記不算；Windows 分隔符；`.app` 外面那一層；不是 bundle 的 `MacOS` 目錄；拿不到執行檔路徑；兩者都沒有→不存）。本機實機：`build/target/object_fps_pvp/bin/gyo_object_fps_pvp --frames 20`，沒有標記印使用者資料目錄，放了 `portable` 印 bin 目錄。
- 突變 3 個，3/3 killed：`pvp-portable-ignored`、`pvp-portable-always`、`pvp-portable-inside-bundle`。
- 沒做：LAN 試驗包改用這個模式（不在這個 PR）；Windows、Linux 上的實機沒跑（純函式的測試在三平台的 L1 跑）。

## 跟上 #358（S4-C：地圖在 World 生成中讀、牆在 worker 建）

- merge `origin/gyo2`（不 rebase）。衝突：6 個 arena 檔（#358 的 `map` 改成 catalog id ＋ 本卡的 version 4 邊界成員）、`Arena.hpp`／`Arena.cpp`（兩邊的說明與解析合在一起：map 只要非空）、`PvpWorld.hpp`／`.cpp`（`InstalledArena` 拿掉了）、`ArenaTests.cpp`、`mutations.json`（照 id 三方合併，兩邊都改的 id 沒有）。
- 遮擋的牆：#358 之後牆只在 Sim／Solo World（`kArenaEntity` 的 `Collider`），Frame World 沒有 `InstalledArena` 可查。改成 `WorldSnapshot::walls`（BoxSet 的本體共用、不可變，複製只是參照）帶到 Frame World 的 `ClientSound::walls`；`world::Blocked` 改吃 `BoxSet`。
- 測試：world 的牆改用 `LoneMatch::SetWalls`；本卡新加的 Client 測試改用 #358 的 `Setup::Loaded()`（等地圖讀完、自己的玩家到了）再多跑 60 幀：merge 後按住連跳的那一案用固定 120 幀，6 次有 2 次還沒 playing。
- 本機：logic 63／world 38／client 43（network 2 skip），cpu ctest 254/254；突變（本卡的 36 個＋#358 新加的 11 個）47/47 killed。

## L1 run 45772574407 的 linux-x64 紅（`client: with the Sim World lent, a click among several frames…`）

- 那次 run 的 head 是 `246719ce`（dispatch 時可攜模式的 push 還沒到），不是當時 PR 的 head。windows、macos 綠。
- 失敗在 `ClientTests.cpp:615` 的 `REQUIRE(captured)`：第一下點擊沒有拿到指標。
- **重現**：點擊在 loading 頁（`GamePage::Loading`）時不拿指標，是規格（`ReadControls`：loading 期間點擊與 Tab 是 loading 畫面的）。那個案等的是「固定 120 幀＋自己的玩家到了（`playing`）」，不是「loading 畫面走了（`ready` 且 `started`）」。本機 8 個並行（模擬 CI 的負載）量：`playing` 之後到 `ready` 有 1～43 幀；在 `playing` 時點擊、`ready` 還沒到的 8 次，8 次都沒有拿到指標（暫時的實驗案，沒有提交）。
- **判斷**：不是 #360 的回歸。本卡沒有改讀取、就緒、loading 頁、指標的路徑（`ReadControls` 只多一行 `jumpHeld`）；是測試的前提（等的條件）在慢的 runner 上不成立，gyo2 上同樣的案也有這個競爭（只是剛好沒碰到）。
- **修**：`Setup::Shown()`（`Loaded()`＋等 `ready && started`＋兩幀讓 loading 頁離開），放在「第一下點擊拿指標」之前：`it connects…`、`a click in a frame that runs no step…`、`J5…`、`with the Sim World lent…`、`holding Space…`、`a shot kicks the first-person weapon…`。期望值不變（點擊照樣要拿到指標、照樣只開一槍）。單人與暫停的兩案本來就等 `started`／在遊戲中，不動。
- 驗證：這 6 案 8 個並行 × 2 輪＝16 次全過。
