# 網路作為 Kernel 上的 plugin：設計稿

更新：2026-11-10。Owner：架構（架構師會話）。**性質：設計稿（設計圖層級），不是 pv10 的設計，不改現況。** 使用者 2026-11-10：「網路部分先記下來，形成設計稿。」
證據：`build/evidence/kernel-study/{bevy_core,replicon,lightyear}.md`（只讀審閱，每條有檔名行號；bevy 0.20.0-dev、bevy_replicon 0.45.5、lightyear 0.31.0）。
方法：不抄功能，抽不變量。兩萬二千行的複製層（replicon）加一整套 netcode（lightyear，0.31 的複製層就建在 replicon 上）接進 Bevy 時引擎一行未改，所以「它們用了引擎的哪些東西」就是 Kernel 的最小協定，「它們自己守了哪些規則」就是網路層的設計圖。

## 1. Kernel 必須提供的八條（網路 plugin 用到的全部）

| # | 不變量 | 兩個 plugin 怎麼用 | GYO 現況 | GYO 的偏離（刻意） |
|---|---|---|---|---|
| 1 | 具名、顯式、可插入的階段清單 | replicon：收在 PreUpdate、送在 PostUpdate、tick 遞增在 FixedPostUpdate；lightyear：回滾在 PreUpdate、輸入在 FixedPreUpdate、對時在 Last | 沒有名字；Match 的順序寫在 host 程式裡 | 每種 host 一份固定序列（Match、Client、Editor）；階段內可重現的全序 |
| 2 | 存取即契約，初始化時檢查衝突 | replicon 手寫 SystemParam 宣告「讀全部被複製的元件」；lightyear 的 PredictionWorld 同 | 沒有 | 註冊時顯式宣告 read／write／exclusive，Kernel 驗證；單一 writer |
| 3 | 型別抹除的統一登錄（id → 函數表） | 序列化表、時間線表、回滾比較函數都是 id → 函數指標 | AssetManager 的 LoaderRegistry 有類似形狀；其餘沒有 | id 由登錄順序或契約決定，**不**由型別首次使用或雜湊決定 |
| 4 | 結構變更延後、在同步點依序套用 | despawn／移除走緩衝；回滾的標記走 Commands | EntityStore 3b（RecordSpawn／Destroy，依 id 升冪）已設計 | 同 Bevy，但套用順序由階段序與 id 決定，不由拓撲平手 |
| 5 | plugin 兩段生命週期：build 收集、finish 凍結 | replicon 在 finish 結算協議雜湊、建通道、動態組系統 | 沒有 | 同 Bevy；finish 後登錄不可再變（wire id 的前提） |
| 6 | 生命週期 hook（add／insert／remove／despawn） | 複製靠它知道結構變了；PreSpawned 靠 on_add 算雜湊 | 沒有 | 回呼依**註冊順序**，不依雜湊（Bevy 的 Observer 依 EntityHashMap） |
| 7 | 每元件每實體的變更 tick | 「跟這個 client 上次確認的 tick 比」決定要不要送 | 沒有（digest 是另一回事） | 變更以**依 Plan 位置取值的單調 u64 stamp**標記（spec A.1；使用者 2026-11-11 決定），tick 視角以 `tickStamps[tick]` 換算：`changed > tickStamps[T+1] − 1`；(c) 區禁用變更過濾。原稿「以遊戲 tick 標記」會讓同 tick 內排在讀者之後的寫入被漏掉 |
| 8 | 時間是可替換的 Resource，固定步可重入 | 回滾＝同步重跑 N 次 FixedMain 並手動推進 Time<Fixed>；對時＝改 virtual 速度、步長不動 | Time／FixedTickRuntime 有基礎；「可重入、可替換」沒約定 | 模擬 step 必須是純函數 Step(state, inputs, tick)；時間只來自 tick |

結論：網路不在 Kernel 裡。Kernel 提供這八條，網路是它上面的 plugin——Bevy 的分層在兩個第三方 crate 上驗證過。

## 2. 網路層自己的設計圖（從兩個 plugin 抽出的不變量）

每條寫：原則｜來源｜GYO 對照（一致／缺／要變形）。

### N1 兩個 tick 領域分開
模擬 tick（每固定步 +1）與複製 checkpoint（送出的序號）是兩個數；server 把模擬 tick 當 userdata 附在複製訊息上，client 維護對應表。｜lightyear checkpoint.rs；replicon ServerTick。｜**一致**：GYO 的 Game Tick 與 UDP sequence 已分開（network-architecture「UDP sequence／Game Tick」）。

### N2 wire 識別碼由契約決定，兩端以雜湊握手
元件在 wire 上的 id 是登錄順序索引，不是執行期 id；連線時比對協議雜湊，不符斷線；它承認偵測不到「函式不同」。｜replicon registry.rs、protocol.rs；lightyear protocol.rs。｜**一致且更嚴**：GYO 用 protobuf 欄位號＝契約、pv 版本號握手；差別是 GYO 的契約在 .proto 檔，replicon 的在程式登錄順序——GYO 的做法更適合「契約可以手寫、可以 diff」（AGENTS §8）。

### N3 結構變更走可靠有序、值變動走不可靠、兩者有依賴
Updates（insert／remove／despawn／mapping）可靠有序、每 tick 每 client 一則、整批原子套用；Mutations（值）不可靠、帶「至少要先套用到哪個 Updates」的 tick；某實體本 tick 有結構變更時其值併進 Updates 保持原子。｜replicon channels.rs、mutations.rs、server.rs:976-998。｜**缺**：GYO 只有一條不可靠通道（Channel 0）；pv10 需求裡的「每收件者的可靠事件串流」就是 Updates 那一半。設計時要定：可靠串流是 Gateway 做（它現在無狀態）還是 Match 做（評估 C 的結論：重送狀態在 Match）。

### N4 送端的確認帳本歸複製元件持有，每收件者一份，判定值每 tick 現讀
server 對每個 client 記「每實體上次確認的 tick、元件遮罩、未確認的訊息」；它只決定重送與去重，不是遊戲狀態的副本；未確認的變更以「確認基準＋最新值」自然重送，不重送舊封包；逾時丟帳本。｜replicon client_ticks.rs、server.rs:401-410。｜**一致**：正是 AGENTS §3「狀態的權威」——帳本是複製元件自己的狀態，不是 Match 判定狀態的寫；Gateway 維持無狀態（memory gateway-original-intent）。GYO 的 snapshot 是 latest-wins，沒有帳本；加帳本時它歸「快照／複製元件」，不歸 Gateway。

### N5 可見性＝送端每收件者一份遮罩，規則由產品給
失去可見→despawn；重新可見→當新實體整個送；決定一律在 server，plugin 只執行。｜replicon visibility.rs（32 bit 上限）。｜**缺但已規劃**：entity-model 的 interest（100 m／30 m）正是這個；遮罩歸 Match 的複製元件。

### N6 client 時間線以速度追隨，步長不動；大誤差 snap 並廣播 tick 平移
不改 Time<Fixed> 的 timestep，改 virtual 速度（±10%）；誤差 >10 tick 直接 snap 並發 LocalTimelineShift，所有以 tick 索引的 buffer（輸入、預測歷史、預生成）一起平移；預測深度超過上限時暫停。｜lightyear sync.rs、timeline.rs:316-325。｜**要變形**：GYO 的 clock sync／phase／re-anchor 已有雛形（Client 的 phase_decision、re-anchor），但「tick shift 事件廣播到所有 tick 索引的 buffer」沒有；這是 presentation-smoothness 批該看的東西。

### N7 輸入以 tick 為索引的 ring buffer，冗餘送；server 遲到不回滾只外推
server 以最後確認輸入外推；之後收到不同只記錄；end_tick 超出 ±64 直接丟。｜lightyear inputs server.rs:487-604。｜**一致**：GYO 的 Match 權威、ingress 的 late／stale 計數就是這個；差別只是 GYO 的視窗與外推規則寫在 protocol-v8 §。

### N8 預測與回滾的前提：模擬 step 可重入且只讀 tick
回滾＝在 PreUpdate 同步重跑 N 次固定步、手動推進 Time<Fixed>、副作用以 is_in_rollback gate；影響結果的 resource 要登錄回滾；不能讀 frame time；預測歷史每 tick 一份、比較用 PartialEq 或自訂容差。｜lightyear rollback.rs:1186-1290、plugin.rs:162-175。｜**部分一致**：GYO 的 Client prediction 是手寫的、digest 證明 step 決定性；但「step 是純函數、可在同一幀重入」沒有寫成約定。這是 Kernel 第 8 條在 Client 上的具體要求。

### N9 延遲補償＝查詢時取歷史姿態，不倒帶世界
server 每 tick 存目標的 Position／Rotation／AABB（35 tick）；broad phase 用歷史 AABB 聯集；精確查詢用 client 的 InterpolationDelay 回推姿態再 cast。｜lightyear integration/avian lag_compensation。｜**一致**：GYO 的 hitbox-rewind 計畫就是歷史姿態查詢；需要 Collision 提供「任意姿態的單一 collider cast」。

### N10 transport 是位元組佇列＋連線實體；通道語意由上層宣告
Link 只有收／送兩條 byte 佇列與連線狀態；Transport（channel、可靠性、分片、ack、頻寬 token bucket）疊在上面；netcode（加密身分）夾在中間；三者都可換。｜lightyear io/link、transport、netcode；replicon backend.rs（無 trait，backend 只需做到六件事）。｜**一致**：GYO 的 Gateway 就是 Link＋連線身分；UDP-only liveness 不變；缺的是 Transport 那層的可靠通道（N3）。

### N11 遠端事件不早於它依賴的結構更新
事件前綴帶該 client 的 update_tick，client 端若比已套用的 Updates 新就排隊；可宣告為 independent 跳過。｜replicon message_buffer.rs、server_message.rs:476-530。｜**缺**：pv10 的可靠事件串流要帶這個依賴標記，否則「門打開了」的事件可能早於「門實體出現」。

### N12 決定性不在複製層的範圍，由模擬層保證
replicon 明寫 client 世界只是最終一致、不假設同 tick 值相同；lightyear 把 hasher 換成跨行程決定性的 SeaHash、server 不回滾。｜replicon lib.rs:704-705；lightyear prespawn.rs。｜**GYO 更嚴**：digest 逐位元相同是 GYO 的 DI；Kernel 的偏離（全序、單 writer、id 不靠雜湊）就是為了在核心守住它，而不是像兩個 plugin 那樣在上面補。

## 3. 對 GYO 現況的總表

| 面向 | GYO 現在 | 設計圖要求 | 差距 |
|---|---|---|---|
| 傳輸 | UDP Channel 0 不可靠有序、HTTP 控制、loopback TCP Match↔Gateway | Link＋Transport（通道語意可宣告） | 可靠有序通道（N3）；Gateway 或 Match 誰持帳本（N4） |
| 複製 | latest-wins snapshot 全量、每 peer 一份 | Updates／Mutations 分流、帳本、遮罩 | 帳本與遮罩歸複製元件；interest（N5） |
| 事件 | 無（擊殺訊息放不下） | 可靠事件串流、依賴標記 | pv10 四件結構之一（N11） |
| 時間線 | clock sync、phase、re-anchor（手寫） | 速度追隨、snap、tick shift 廣播 | 廣播機制（N6） |
| 輸入 | Match 權威、late／stale 計數 | ring buffer、冗餘、外推 | 一致 |
| 預測 | 手寫、digest 守 | step 純函數可重入、歷史每 tick、副作用 gate | 寫成約定（N8） |
| 命中回溯 | hitbox-rewind 計畫 | 歷史姿態查詢 | 一致 |
| 契約 | .proto 欄位號、pv 版本 | 登錄順序 id＋雜湊 | GYO 更適合（N2） |

## 3b. Peer 與 Player 分開（使用者 2026-11-11 決定，audit 第 5 項附帶）

- **Player 實體**：成員，Gateway 配的玩家段 EntityId；欄 Body／Life／InputCursor／Connection 診斷。
- **Peer 實體**：每收件者的 Ledger（N4）與 Visibility 遮罩（N5），獨立於 Player，不合一——為觀戰者等非 Player 的 Peer 留位置。一個 Peer 可能對應零個或一個 Player。
- **Link 是 external Resource，不是實體**（spec I.2）。mechanism 圖 3「Ledger 與遮罩掛在 Peer 實體上」照此解讀：Peer 是獨立實體。

## 4. 對 pv10 的含意（只列結構，不設計）

pv10-requirements 的四件結構性需求，對應到設計圖：每收件者的可靠事件串流＝N3 的 Updates 那半＋N11 的依賴標記；每收件者的相關性選擇＝N5 遮罩＋N4 帳本；私人資料塞進收件者那一份＝N4（帳本本來就每收件者一份）；動作請求的 oneof＝N7 的輸入通道。設計時的兩個決定留給使用者：可靠串流的帳本放 Match（評估 C 的建議，Gateway 維持無狀態）還是 Gateway；是否採「結構走可靠、值走不可靠」的兩通道分流。

## 5. 本稿不做的事

- 不改 pv9、不改 Gateway、不改 Match。
- 不決定 Kernel 的介面（blueprint.zh-Hant.md 的事）。
- 不評估 lightyear／replicon 能不能直接用（它們是 Rust；評估卡只看 C++ 候選）。
