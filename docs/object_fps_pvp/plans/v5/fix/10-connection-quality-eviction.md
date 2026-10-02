# 10 高延遲／低幀率玩家的伺服器保護（連線品質移出）

狀態：**已解決（2026-10-02實作與CPU驗證完成（PR #11）；第03批結案驗收通過）**。設計、閾值與HUD警告於2026-10-02核准。Owner：`object_fps_pvp`。
相關：閾值以[08](./08-a1-clock-drift.md)提案（token bucket＋持續相位追蹤）為前提驗證；索引見[README](./README.md)。

## 問題成因

使用者要求：遇到高ping、低幀率而系統無法妥善處理的玩家，直接移出伺服器。目的有二：
保護其他玩家的體驗，以及不再持續嘗試處理一個無法完美處理的情況。

現行沒有任何伺服器端的連線品質判斷：Gateway只有每秒120包的流量限制與5秒無封包逾時，
Match只有Starvation／Backlog移動重設。Host也量不到RTT：輸入窗口沒有回傳Client看到的Snapshot，
只有動作請求帶`observed_authority_tick`（250ms參考年齡上限，只在射擊／換彈時）。

模擬（08的模擬器、提案版、0ppm、每格18組相位×5分鐘）顯示兩條由設計決定的硬邊界：

| 邊界 | 正常範圍 | 崩潰 | 原因 |
|---|---|---|---|
| 延遲 | RTT≤140ms：零Held、零重設 | RTT 150：預測凍結31–40%；160：凍結81–94%；200以上：Held約75% | Client未確認窗口上限12個命令（200ms），約RTT 150時塞滿（`MaxPendingCommands`） |
| 幀率 | ≥30 FPS：零Held（28 FPS約0.4%） | 25 FPS：Held 7–10%；20：25–31%；15：31–49%；10：53–94% | 幀間隔超過lead的2 Tick（33.3ms），兩幀之間的命令必然來不及送達（lead 2是固定政策） |

## 影響

- 對其他玩家：被替代（Held／Neutral）的移動與反覆重設，使該玩家的角色在別人畫面上停頓、滑動或瞬移。
  本作沒有射擊回溯，高延遲主要傷害該玩家自己，但凍結與瞬移仍影響所有人。
- 對該玩家：預測凍結、輸入大量遺失；在這些範圍內調整演算法也無法改善（受lead與窗口上限限制）。

## 如何復現

以08的模擬器（`build/target/_build/test/logs/pvp-v5-ct-prototype-20261002/drift/`）：

```bash
E=build/target/_build/test/logs/pvp-v5-ct-prototype-20261002/drift
SIM_WORKER=bucket CT_STAT=5 CT_N=240 CT_D=2 CT_LATE=2 python3 $E/sweep.py $E/out_kick_grid.txt CT \
  fpsx60,fpsx45,fpsx35,fpsx30,fpsx28,fpsx25,fpsx20,fpsx15,fpsx10 0 0,50,100,120,140,150,160,180,200,250 6 300
python3 $E/grid.py $E/out_kick_grid.txt kicked held frozen ref50
python3 $E/kicks.py $E/out_kick_suite.txt 40       # 19種正常情境的誤踢數
```

`fpsxN`為N FPS；`kicked`為18局中被移出的局數；閾值可用環境變數`KICK_REF`（ms）、`KICK_HELD`（%）、
`KICK_RESETS`（每窗口次數）、`KICK_WINDOWS`調整。模擬器的參考年齡在Host收到窗口時以Client發布時看到的
Snapshot計算，等同下文的新欄位。

## 解決方案（提案，待核准）

### 判定

Match每10秒（600 Tick）評估一次每位玩家；加入後的第一個10秒不評估（啟動卡頓與窗口暖機）。
一個窗口在下列任一成立時判為不合格：

| 項目 | 閾值 | 依據 |
|---|---|---|
| 輸入參考年齡中位數（≈RTT＋Client發布延遲） | ＞160ms | 60 FPS時約RTT 150：預測開始凍結；RTT 140在30 FPS約159ms，為邊界 |
| 被替代的移動（Held＋Neutral）比例 | ＞5% | 正常情境最高約0.9%（10%掉包）；25 FPS已7%以上 |
| Starvation／Backlog移動重設 | ≥1次 | 正常情境每局15分鐘0–數次；崩潰區約每秒一次 |

**連續3個不合格窗口（30秒）即移出**。持續惡劣的玩家約在加入後40秒移出；一次性卡頓、單次重設不會觸發。

### 模擬驗證（提案版，18局×5分鐘）

- 崩潰區：FPS≤25全部移出；RTT≥160全部移出；RTT 150在≥35 FPS全部、60 FPS 12／18；
  28 FPS在RTT≥140移出。中位移出時間40秒。
- 正常情境（19種×0／±20ppm×RTT 0／20／40，3078局）：只有「卡頓風暴」（每400ms卡45ms、持續60秒）
  被移出（142／162），其餘18種（30／40／52 FPS、vsync掉幀、10%掉包、開局卡頓、Host晚醒、網路抖動等）為0。
  RTT 100同樣只有風暴；RTT 140時30 FPS、5ms網路抖動各9／54、5%雙向掉包27／54被移出（延遲與掉包疊加）。
- 卡頓風暴期間該玩家約6.5%的移動被替代，傷害程度與持續25 FPS相同，因此以同一規則移出。
  若要放過這種60秒的暫時狀況，持續時間須超過70秒，代價是真正無法處理的玩家要影響別人一分鐘以上。
- **前提**：同一規則套在現行A1（未做08的修正）上，正常情境會誤踢382／3078局（30 FPS 36%、10%掉包26%、
  vsync 45 FPS 22%、55–58 FPS 14%），等於以本系統的缺陷懲罰玩家。保護機制必須與08的修正同時或之後上線。

### 機制

1. **量測**（`MatchRuntimeHost`，Match擁有）：
   - 新欄位`PlayerInput.observed_authority_tick`（Client填入發布時最後套用的Snapshot Tick；
     產品Gateway合併窗口時取最大值）。Host對每個帶來新命令的窗口，以`publishedReferences_`的發布時刻計算年齡；
     找不到（超過約1.07秒）視為超限。
   - `PvpMatch`提供每位玩家的累計裁決數、被替代數與移動重設數；Host以窗口差分計算。
2. **移出**：連續3個不合格窗口時，Host在Tick邊界執行與Leave相同的清理，並產生移出結果
   （玩家、原因、三項量測值）。原因：最後一個窗口的參考年齡超限為`high_latency`，否則為`unstable_input`。
3. **Runtime協定**：新增Match→Gateway的`PlayerEvicted { player_id, reason, reference_age_ms, substituted_permille, resets }`。
   產品Gateway收到後做與Leave相同的清理，送Client既有的`Error { code, message }`
   （`code`為`evicted_high_latency`／`evicted_unstable_input`，`message`含量測值），並結束該Session。
4. **Client**：沿用既有Error處理回到大廳並顯示原因（例：「延遲過高（約180ms），已被移出伺服器」）。
   可重新加入；條件未改善時約40秒後再次移出。
5. **（選配，建議）HUD警告**：`PlayerState`加連續不合格窗口數，HUD在第1、2個不合格窗口時提示
   「連線品質不佳，20秒內未改善將被移出」。不做則玩家只會在被移出時才知道原因。

### 須核准的契約變更

- §1新增「連線品質移出」政策列（上述閾值與30秒規則）；§2 `PlayerInput`加`observed_authority_tick`；
  Runtime協定加`PlayerEvicted`；Client `Error.code`新增兩個值；（選配）`PlayerState`加警告欄位。
- Architecture Delta：Match新增「依移動品質移出玩家」的責任（產品內，`object_fps_pvp`）；
  新增Match→Gateway的移出方向（原本只有Gateway→Match的Leave）；Engine與`services/gyo_gateway`不改，
  依賴方向不變。閾值是Match的固定常數，不經Ready／Welcome下發。

### 實作分批

接在08提案的分批之後：
6. Host量測與判定、`PlayerInput`新欄位、`PlayerEvicted`、Gateway移出流程與Client顯示及測試（high；判定邏輯xhigh）。
   （選配）HUD警告與其欄位。

## 驗證

- 尚未實作。上表為模擬；實作後須補Host判定的單元測試（窗口、寬限、連續3窗、原因）、Gateway移出流程（Go）、
  Client顯示，並以模擬器重跑誤踢與崩潰區兩組設定。

## 殘留風險與後續

- 閾值只在模擬中驗證；真實網路的RTT分布、顯示器時序與機器負載可能使邊界附近（RTT 140–150、28–30 FPS）
  的結果不同。邊界附近的玩家可能被移出，這是本設計刻意的取捨（用固定lead 2與12命令窗口換低延遲）。
- 參考年齡依賴Client誠實填寫Snapshot Tick；本作為LAN／開發情境，不作為反作弊。
