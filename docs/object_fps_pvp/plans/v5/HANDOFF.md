# PvP v5 交接

更新：2026-10-03。**第01–04批已完成（第03批2026-10-02結案驗收通過；第04批PR #14合併為`cef1b39`）。**
**第04批2026-10-02完成**（計畫複審PR #13合併為`dcb19d1`；工作分支`claude/pvp-v5-batch04`；經過見[第04批dev_log](../../../dev_logs/2026_10_02_pvp_v5_batch04.zh-Hant.md)），**第05批進行中**（2026-10-02使用者啟動；工作分支`claude/pvp-v5-batch05`，自`cef1b39`）：05-1～05-4完成（2026-10-03），剩05-5完整驗收；未執行長測、未升格v5穩定基線。

## 第05批進度（記錄器，隨工作更新）

使用者決定（2026-10-02）：
- 「v5結案」＝第05批＋完整驗收＋全部通過即升格v5穩定基線（同v4的結案方式）。此決定即視為完整GUI三輪與兩組30分鐘長測的授權。
- 平台：只在本機macOS（Intel／Metal）；Windows／Linux依跨平台手法標「未執行」，狀態表按平台分列。

開工時發現：完整驗收的兩個runner仍是v4語意。`run_action_legal.py`（headless長測）沒有生命、換彈或死亡；
`run_timing.py --combat`（GUI三輪）的`combat_latency.hpp`只追蹤HP，不分生命、不換彈。第05批計畫「先修正驗收語意，再量測」
正是這個缺口。v5的逐生命核對已存在於第03批的玩法probe（`--gameplay-v5`，固定16秒計畫）與`gameplay_evidence.py`。

| 子批次 | 內容 | 建議檔位 | 狀態 |
|---|---|---|---|
| 05-1 | headless長測v5化：玩法probe可重複16秒計畫為多個循環（生命世代相對遞增），分析器逐循環／逐生命核對；`--soak`長模式與短模式 | high | 完成（CTest 42／42；突變14／14；開發實跑2循環60 Hz、3循環144 Hz與單循環矩陣clean-60皆PASS，不計入驗收） |
| 05-2 | GUI combat v5化：射擊遇空彈匣換彈、死亡／重生期間依v5規則、逐生命HP與唯一傷害核對 | high | 完成（CTest 43／43；突變9／9；開發實跑16秒與兩輪120秒，第二輪120秒整輪PASS，不計入驗收） |
| 05-3 | 整合短測與架構檢查：依指紋重用第03／04批短測（05-1改了action probe，25案矩陣須重跑）、一輪雙GUI整合短模式、產品移除／owner選擇／Match不連結Renderer與SDL | medium | 完成（矩陣25／25、GUI短測、長測短模式、產品移除皆通過；2026-10-03） |
| 05-4 | `MANUAL_ACCEPTANCE.md`與`ACCEPTANCE_STATUS.md`（跨平台、按平台分欄）；README／HANDOFF／dev_log；PR | medium | 完成（2026-10-03；PR待CI） |
| 05-5 | 完整驗收：事前宣告；GUI三輪×120秒combat（計次、fix/06補跑規則）；60 Hz與144 Hz各1808秒長測（113循環，串行）；全部通過寫v5穩定基線並結案 | medium（執行約1.5小時，機器須閒置） | 未開始 |

使用者2026-10-02確認拆分與檔位，05-1以high執行。

- 05-1紀錄：
  - probe（`gameplay_action.hpp`）新增`--cycles N`（1–113）：把第03批的16秒計畫重複N次。B每循環死亡一次、A從不死亡，
    所以B的動作生命世代每循環+1、A固定為1；計畫JSON帶`cycle`、`cycles`、`lives_per_cycle`與絕對生命世代。
    A每循環結束剩11發，下一循環第12發會變成空彈拒絕，因此**只在後面還有循環時**於14.6秒加一次`cycle-reload`；
    單循環仍是原16秒計畫（矩陣用）。每幀另記雙方Client的相位追蹤狀態與修正計數。
  - 為何另寫runner與分析器：v4的`run_action_legal.py`／`analyze_legal`假設「全部接受、HP打到0後仍可射擊」，
    v5會在第13發起變成空彈拒絕；第03批`gameplay_evidence.py`把全部幀載入記憶體並逐幀重算戰鬥狀態，
    144 Hz跑30分鐘約1.2 GB證據，無法沿用。新增`run_gameplay_soak.py`（不經relay，與v4長測相同，
    以Gateway限流計數與Client傳輸上限核對容量）與串流分析器`gameplay_soak_evidence.py`（重用第03批的
    `combat_expected`、`validate_accepted_actions`、`client_disturbance`等判定，按生命分組並依Tick快取）。
  - 檢查：每個宣告動作恰一次且判定與計畫完全一致、每循環判定簽名相同；逐幀HP／彈藥／換彈／最後射擊與該生命的唯一裁決一致；
    B的生命1…N+1連續、每生命恰一次死亡且在該循環內、180 Tick等待、屍體不水平移動、重生建立新epoch且有對應的LifeRespawn重設；
    A從不死亡；每位玩家每循環都有跳起並落地；乾淨跑次的移動門檻（送出P95≤22、Actual P50≤50／P95≤66.7、≥99%）、
    零非重生重設、零Match掉時、零≥100ms幀。
  - 「延遲不隨時間漂移」：以**既有**門檻（50／66.7ms）套用到每10個循環（160秒）的窗口，任一窗口不合格即失敗；
    不是新數字。相位追蹤（狀態秒數、修正與遲到修正次數、每循環修正數）只報告，不新增門檻。
  - 長模式：`--soak`固定113循環（1808秒，涵蓋1800秒的最小整循環數），只允許60／144 Hz；短模式2–4循環。
  - 開發實跑（不計入驗收，`pvp-v5-batch05-1-dev-{1,2,matrix}/`）：2循環60 Hz與3循環144 Hz皆PASS，
    每循環判定簽名一致、位置每循環回到同一點；144 Hz 48秒產生33 MB證據、分析1.2秒／39 MB記憶體（推估30分鐘約1.2 GB、45秒）。
    單循環矩陣`clean-60`仍PASS。
  - 突變（以dev-1實跑資料改壞後重分析）14／14被抓：判定被改、重複裁決、致命傷害消失、早於排程送出、錯生命、
    幀HP／彈藥被改、屍體移動、慢幀、跳躍消失、LifeRespawn重設遺失、重設原因非重生、Actual改為Held、後段延遲漂移。
  - 指紋影響：action probe已改，第03批結案的25案矩陣不再符合「指紋未變才重用」，05-3須以新probe重跑；
    `gameplay_evidence.py`未改。
  - v4的`run_action_legal.py`（`--legal-shots`）與`analyze_legal`在v5無法通過，不再作為v5驗收入口；
    沒有刪除（範圍外），已列入[v6交接](../v6/HANDOFF.md)第5項的驗收工具清理。
- 05-2紀錄（使用者2026-10-02確認，high）：
  - 預先宣告的SDL排程（`combat_latency.hpp`）：沿用v4每0.8秒一格、自0.2秒起，改為17格一循環（13.6秒）：
    4發擊殺join→3發打屍體（死亡等待中）→空格→4發擊殺重生的新生命→第12發打屍體→空彈匣點擊→R換彈→換彈中點擊→空格。
    每格與其依賴的死亡／重生／換彈邊界至少隔0.6秒，所以每個動作的判定都可事前確定。120秒＝150格、116個送出動作、18次死亡。
  - 實跑確認的v5行為：空彈匣與換彈中的點擊由Client在本機擋下（不送出、不播射擊動畫），probe逐次檢查；
    對死亡目標的射擊被受理、扣彈、命中World、0傷害（Match不把屍體放進命中候選）。
  - HP紀錄每幀加上完整CombatState（生命、彈藥、換彈、最後射擊）與PlayerState生命欄位，HUD另記彈藥、生命、死亡。
    分析器（`combat_gui_evidence.py`，schema 2）以第03批的`combat_expected`逐生命重算每幀快照與HUD；
    命中的目標生命以排程預測，再以快照的生命時間線核對；死亡等待180 Tick、重生建立新epoch、射擊者從不死亡；
    Match的LifeRespawn重設必須與觀測到的死亡／新生命逐一相同。
  - 共用移動分析器`command_evidence.analyze_commands`原本把重生的epoch重設判為「不明重設」，重生首幀的時間重設判為干擾，
    舊生命被取消的命令算成未執行。改為：LifeRespawn重設另列（`unexpected_resets`只計其他原因）；
    重生首幀只有與LifeRespawn精確配對時才不算干擾（規則從第03批`gameplay_evidence.client_disturbance`移到共用函式，兩邊共用）；
    生命週期取消的命令必須有對應的LifeRespawn，才移出分母。`run_timing.py`在有重生時要求combat證據已核對，否則失敗。
  - 60 Hz產量檢查原為整段±2步（頭尾兩個邊界）。實跑中join每次重生約少1個命令：契約規定每次播種重新起算固定步相位（≤1步），
    新epoch首次相位修正≤±2 Tick（`MovementPhaseMaximumCorrectionSeconds`）。因此每次LifeRespawn另加3步容許，
    依契約上限換算，不是新數字。120秒輪join 17次重生、容許53步，實測少14／20步。
  - `test_combat_gui_evidence.py`原本未登錄CTest（既有缺口），本次改寫並登錄（`combat_gui_evidence`，逾時30秒）。
  - 開發實跑（不計入驗收，`pvp-v5-batch05-2-dev-{1..4}/`）：dev-1為改分析器前的16秒原始資料；dev-2 16秒短測PASS；
    dev-3 120秒的combat與可見延遲皆過，但產量檢查失敗（促成上一項）；dev-4 120秒整輪PASS：可見P50／P95 36.0／39.4ms（200／200）、
    移動Actual P95 37.8ms、裁決抵達P95 107ms、射擊回饋P95 6.2ms、18次死亡／重生、視窗乾淨。
  - 突變（以dev-4資料改壞後重分析）9／9被抓：屍體射擊造成傷害、換彈被拒、空彈點擊被送出、join快照HP、create快照彈藥、
    HUD彈藥、HUD生命、LifeRespawn重設缺一、死亡等待不是180 Tick。
  - 指紋影響：`gameplay_evidence.py`改為呼叫共用配對函式，05-3的25案矩陣本來就要以新probe重跑。
- 05-3紀錄（使用者2026-10-03確認，medium；來源`4f91a0a`，事前宣告與指紋在
  `build/target/_build/test/logs/pvp-v5-batch05-3-20261003/`的`declaration.md`、`fingerprints.txt`）：
  - 產物確認：測試建置`ninja -n`無待建項目，正式Match／Gateway／Client與probe即為`4f91a0a`的產物。
  - 25案真網路玩法矩陣（新probe與分析器，一次）：25／25通過，1075／1075動作，干擾解除後新裁決最慢1.081秒（門檻1.5秒）。`matrix/`
  - 雙GUI整合短測（計次一輪，`--gui --combat --short`，16秒60 FPS）：PASS，可見P50／P95 36.3／38.1ms（20／20）、
    2次死亡／重生、11命中／4屍體射擊／1換彈、視窗乾淨，未補跑。`gui-short-1/`
  - 長測短模式（2循環60 Hz）：PASS。`soak-short/`
  - Match依賴：`otool -L`只連結CoreFoundation、libc++、libSystem，符號無SDL／Renderer。
  - 產品移除（`removal/`，含腳本`removal_check.sh`）：`git archive HEAD`的隔離副本刪除281個owner檔案與
    `engine/config/projects.csv`的登錄列後，`test` preset組態與完整建置成功；CTest 26／27，唯一失敗`build.ci`的4項
    release pipeline測試都是`git rev-parse`在非repo副本失敗；在副本內建立git repo重跑`build.ci`即通過。
    剩餘非文件引用1處：`services/gyo_gateway/README.md`說明產品Gateway模組依賴公共模組（方向正確；移除產品時該說明會過時，屬文件）。
  - 量測後檢查無孤兒程序（Match／Gateway／probe／runner皆已結束）。
- 05-4紀錄（使用者2026-10-03確認，medium）：
  - 新增[手動指南](MANUAL_ACCEPTANCE.md)：共同前提（`test` preset建置、一次一項、全新輸出、指紋）、正式Client四終端、
    L3原生操作清單（v5預期）、L1／L2短測命令、量化門檻（含05-1／05-2確立的逐生命、LifeRespawn、產量容許與漂移窗口規則）、
    完整驗收命令（GUI三輪＋兩組長測）與fix/06補跑規則、平台註記。命令以本機`--help`與05-3實跑核對；
    雜湊用Python，避免`sha256sum`／`shasum`的平台差異。
  - 新增[驗收狀態](ACCEPTANCE_STATUS.md)：短測與完整驗收兩表，按macOS／Windows／Linux分欄；Windows／Linux與完整驗收全標「未執行」；
    回報欄位與升格條件。
  - 第05批計畫狀態、README進度與接續文字、本批dev_log（`docs/dev_logs/2026_10_03_pvp_v5_batch05.zh-Hant.md`）已更新。

## 第04批進度（記錄器，隨工作更新）

| 子批次 | 內容 | 建議檔位 | 狀態 |
|---|---|---|---|
| 04-1 | 第一人稱Reload（`WeaponViewModelAction::Reload`，以權威reload Tick定錨、本機時間平滑推進）；HUD | high | 程式完成（建置、CTest `-L pvp` 16／16）；畫面待04-4截圖確認 |
| 04-2 | 遠端動作：玩家animset加Shoot／Reload／Jump三段／Death01；`PlayerPresentationFrame`帶同區間CombatState與grounded；上半身組合、跳躍狀態、死亡保持；ActionId去重與生命隔離 | high（生命／時間線隔離xhigh） | 程式完成（呈現測試19 cases／12616 assertions、突變9／9被抓、CTest `-L pvp` 16／16）；畫面待04-4截圖確認 |
| 04-3 | 驗證工具跨平台（切片8）：平台指紋、視窗擺放；probe新增SDL注入的v5動作短測模式 | high | 完成（CTest `-L pvp` 17／17；開發實跑action 30／60／144與capture皆PASS，不計入04-4） |
| 04-4 | 驗證與記錄：CPU／CTest、L1 30／60／144短片段、L2 Metal capture冒煙與圖像、L3使用者人工清單；README／HANDOFF／dev_log | medium | L1／L2／L3通過（macOS Intel／Metal）；dev_log與README已更新 |
| 04-5 | 步幅：依速度混合Walk_Loop／Jog_Fwd_Loop（使用者2026-10-02決定），各自校準防滑步 | xhigh | 完成（呈現測試21 cases、突變5／5、CTest 41／41）；人物短測跨FPS相位一致；使用者目視步態接受（`pvp-v5-batch04-5-gait-manual/`） |

- 開工時發現：GUI probe的`--gpu-driver`只列`auto|d3d12|vulkan`，macOS依賴`auto`選到Metal；第03批的玩法GUI（Space／快射／R／重生）
  是X11/XTest的`run_gameplay_gui.py`，SDL注入的probe沒有對應模式，因此04-3須補一個動作短測模式。
- 04-1紀錄：
  - 換彈動畫等Snapshot顯示權威換彈才開始（契約「Reload 1.5秒權威進度」），按R當下只有既有的「Reload requested」文字；
    進度以首次看到時的權威已過時間定錨，之後以本機時間平滑推進並夾在1.5秒內，死亡／新生命／權威結束即回Idle。
    Mark23 Reload原長3.733秒由武器模型依進度對應（約2.49倍速）。
  - 新增獨立計數`reloadAnimationStarts`：`animationStarts`維持「本機立即射擊回饋次數」，因`combat_latency.hpp`與
    `weapon_short.hpp`檢查它等於送出的動作數，換彈要等權威確認，沿用會在送出與確認之間短暫不等。
  - 操作提示「WASD move | Space jump | Click shoot | R reload」第03批已存在，切片7不需再加。
  - 修正第03批留下的HUD重疊：「CONNECTION POOR」警告原畫在y=70–100、水平置中，與左上HUD面板的HP／彈匣行重疊
    （800×600與1280×720皆然），改到畫面下方中央。
- 04-2紀錄（xhigh）：
  - 設計：遠端動作是呈現時間的純函數。輸入只有`presentationSeconds`與時間線同一區間、同一生命的`CombatState`／`PlayerState`
    （`ResolvePlayerActions`）。時間線游標只前進、停住時凍結，因此重送、裁決ACK、重複Snapshot與時間線停住都不會重播，
    恢復後也不補播已過的動作；不需要以ActionId記錄「已播過」的狀態。唯一有狀態的是跳躍（`grounded`轉換），
    重設（spawn／生命／epoch／不連續／長幀／瞬移）時依當下狀態直接進Loop或落地，不捏造起跳。
  - 合約時長：射擊0.1667秒（Pistol_Shoot原長0.633秒，約3.8倍速）；Jump Start／Land各0.1秒（原長1.333／1.267秒，約13倍速），
    只有上升中離地才播Start，掉落直接Loop；換彈依權威區間（Pistol_Reload 1.667秒對應1.5秒）；Death01原速2.4秒後保持末姿態到新生命。
    上半身（`spine_01`以下）取持槍／射擊／換彈，下半身取移動或跳躍；死亡為全身，武器與髮飾跟隨最終合成姿勢。
  - 根運動量測（ufbx）：Jump系列骨盆不高於站姿（Start由蹲回站、Land下蹲緩衝），不會與權威垂直位移疊加，不需剝除；
    Death01骨盆落地並水平位移約0.55m。
  - 防禦：動作Tick早於`lifeStateTick`不顯示。Match重生時已重建`CombatState`、死亡時清除換彈，這是雙重保險。
  - 突變檢查9項（拿掉生命防護×2、掉落也播Start、重設捏造起跳、死亡不夾、射擊區間閉合、射擊不縮放、死亡時套上半身、
    射擊優先於換彈）全部被測試抓到。
  - 資產：玩家animset加入shoot／reload／jump_start／jump_loop／jump_land／death；檔名`locomotion.animset.json`改為
    `animations.animset.json`（內容已不只移動；只有`asset_catalog.json`引用）。`presentation.json`加`actions`時長。
  - 待04-4截圖確認：射擊3.8倍速與起跳13倍速的觀感、Start開頭的瞬間蹲低。
- 04-3紀錄：
  - 平台指紋（`platform_fingerprint.hpp`）：延遲報告寫`platform_*`（OS、架構、SDL視訊驅動、GPU驅動、更新率、可用區域、輸入方式），
    JSON模式（weapon／player／action）附`platform`；`presentation_evidence.platform_evidence`讀取（舊probe無指紋記為absent），
    `run_timing`摘要新增「Round N platform」行。GPU驅動取自`RenderDevice().GetInfo().driver`，不改Engine。
  - 視窗擺放：對角配置由macOS擴大到所有能由程式擺放視窗的平台；`SDL_SetWindowPosition`失敗（如Wayland）時保留預設位置，
    並記錄`window_placement_error`。摘要與干擾說明文字隨之更新。
  - SDL注入的動作短測：`--action-short --fps 30|60|144`與`--action-capture --fps 60`（`action_short.hpp`），runner
    `run_action_short.py`（GPU驅動預設auto、可選metal），`summarize`測試5項並登記CTest `object_fps_pvp.action_runner`；
    `test_service_startup.py`的RUNNERS加入新runner。情境：按住只射一發、R換彈、換彈中射擊／再按R被擋、換彈中後退、
    跳躍中射擊、打空彈匣與空彈匣點擊被擋、空彈匣換彈、擊殺；目標死亡時移動／跳躍／開火／換彈全被擋、重生滿HP與彈匣並可射擊；
    雙方互看遠端射擊、換彈、跳躍三段、死亡保持與新生命，同一ActionId不得重播。
  - 開發實跑（不計入04-4，macOS Intel／Metal）：action 30／60／144與capture皆PASS；capture 8張圖已目視
    （第一人稱換彈、遠端換彈／跳躍／射擊／死亡／死亡保持）。report-only延遲短測一輪：可見P50／P95 37.0／40.8ms，摘要含平台行。
    證據：`build/target/_build/test/logs/pvp-v5-batch04-dev-{action-1..4,timing-1}/`（git忽略）。
  - 實跑中發現：本機射擊冷卻閘以最新Snapshot的Tick比對`nextAllowedShotTick`，該Tick約比權威晚2 Tick；
    12 Tick間隔的點擊在幀抖動下會在本機被擋（第03批既有的「冷卻點擊不排隊」設計，未改）。probe改用15 Tick間隔，
    精確10 Tick邊界由domain測試涵蓋。是否讓本機閘門補償這個落差，屬日後的產品手感議題。
  - 既有工具狀況（未改，超出範圍）：`weapon_short.hpp`觀察方仍檢查v4「HP=0仍可移動與射擊」，在v5必然失敗；
    其v5涵蓋已由動作短測取代。`run_player_short.py`／`run_weapon_short.py`的GPU驅動選項補上metal、預設auto。
- 04-4紀錄（來源`a3962cf`，macOS Intel／Metal；Windows／Linux未執行）：
  - CTest全標籤41／41（開跑前，`99db266`）；`-L pvp` 17／17（`a3962cf`）。
  - L1／L2：`run_action_short.py`四案。第一次（`pvp-v5-batch04-20261002/`）action30失敗並保留：量測中實體滑鼠移動，
    actor yaw在無排程轉向時漂移約2.5°，之後射擊未命中、目標未死。當時probe無法區分外部輸入，故新增「未排程yaw變化」
    偵測（標`disturbed`、runner註明非產品判定），提交`a3962cf`後在`pvp-v5-batch04-20261002-run2/`重跑：action 30／60／144
    與capture皆PASS，皆無干擾；目標死亡約9.5秒、3.0秒後重生，死亡中位移0；遠端看到16發、2次換彈、跳躍三段，重播0。
    L2的8張截圖已目視；限制：遠端人物距離遠（約120像素高），射擊中與待機的上半身差異在截圖中難以分辨，由L3確認。
  - L3（使用者人工，`pvp-v5-batch04-20261002-manual-2/`）：清單1–8通過。第一組（`-manual/`）只短暫進入世界，不計。
  - 使用者回報與決定（2026-10-02）：
    - 上下視角：遠端人物上半身沒有依pitch瞄準（契約「持槍／瞄準」的缺口；素材有Pistol_Aim_Up／Neutral／Down）。決定：延到v6（[v6交接](../v6/HANDOFF.md)第1項）。
    - 步幅：Jog_Fwd_Loop原速足部約5.94 m/s，移動3 m/s時以約0.5倍速播放（第02批防滑步校準），看起來是慢動作大步跑；
      素材為使用者提供的UAL，不是Agent自製。決定：依速度混合Walk_Loop／Jog（04-5）。
    - 受擊反應：不在契約與本批範圍（素材有Hit_Chest／Hit_Head；Snapshot無受擊時點）。決定：延到v6（[v6交接](../v6/HANDOFF.md)第2項）。
    - CONNECTION POOR：使用者在操作中看到警告。Client紀錄有三次約1.2秒與兩次0.7–0.8秒的render停頓，緊接視窗互動
      （釋放指標）；Engine SDL GPU後端用阻塞的`SDL_WaitAndAcquireGPUSwapchainTexture`，Metal在視窗拖動／縮放時取得
      drawable最多等約1秒，主迴圈停住、不產生移動命令，1.2秒約72 Tick Held（10秒窗口12%>5%）使該窗口不合格。
      警告判定本身正確。決定：先記錄為已知問題（Engine層，修正屬Architecture Delta），延到v6（[v6交接](../v6/HANDOFF.md)第3項）。
    - 04-5紀錄（xhigh）：
      - 量測（測試`PvP walk and jog native speeds are measured stance evidence...`）：Walk_Loop 1.333秒、著地足速約0.93 m/s；
        Jog 0.933秒、約5.96 m/s（沿用第02批校準6.0）；兩者左／右腳著地相位皆約0／0.5，同一步態相位驅動、無需偏移。
      - 設計：Jog權重＝clamp((速度−0.93)/(6.0−0.93))；速度取時間線前後兩個權威狀態的水平位移（`SnapshotPresentation.planarSpeed`），
        與Client幀率無關；無速度樣本（hold／停住）時保留上一權重。週期距離＝lerp(0.93×1.333, 6.0×0.933, 權重)，
        相位（cycles）每幀增加帶號位移÷週期距離。3 m/s時權重0.408、週期3.02 m、每秒約2步（原純Jog約1.07步）；
        混合後著地足速實測2.85／2.99 m/s（目標3）。設定`presentation.json`的`locomotion.walk_native_speed／jog_native_speed`
        取代`jog_stride_scale`；觀測與第02批分析器由Jog秒改為步態週期（`phase_cycles`、`cycle_distance`、`jog_weight`）。
      - 突變5項（權重改用本幀位移速度、hold重設權重、Jog用Walk時鐘、週期忽略Walk、權重斜率錯）皆被抓到。
      - GUI（開發驗證，`pvp-v5-batch04-5-player-1/`）：人物短測同路程0.5／1.0／1.5單位的相位在30／60／144 FPS間差<1e-15週期。
        144 FPS案在幀率門檻失敗（join實測120 FPS＜85%×144）：兩視窗CPU準備世界皆約2.1ms，join的render中位8.14ms（create 2.75ms），
        判斷為本機同時跑兩個144 FPS視窗的GPU／呈現容量，非04-5的CPU成本；照實保留，不重跑挑分數。
      - 截圖模式（`-player-2/`）原失敗於區段檢查：Metal讀回使每張截圖造成>100ms長幀而重設步態相位（第02批Linux／Vulkan約68ms
        未觸發）。分析器的區段檢查比照既有幀率門檻與同路程比較，限非截圖模式；同一跑次重新分析通過。8張截圖已產生。
    - 自己死亡時「持槍手臂還在」：待使用者釐清是第一人稱（程式在死亡時隱藏）或對方畫面的屍體手持槍（Death01全身含掛槍，現行設計）；
      延到v6（[v6交接](../v6/HANDOFF.md)第6項）。本機冷卻閘落差與`weapon_short`清理亦列於v6交接第4、5項。


第03批待結案守門：乾淨可見延遲短測曾有P50 **51.125ms >50ms**（啟動相位）。2026-10-01經使用者
批准實作方案A1啟動相位對齊（PR #2，合併為`ff11ee3`）；2026-10-02補上低幀率守門與驗收器修正（PR #3，合併為`9cd7f26`）。
其後一度依使用者決定以MVP技術驗證收尾、暫緩結案驗收。門檻、lead、插值與60Hz始終不變。各問題見[修正與已知問題](fix/README.md)。
2026-10-02：A1「每epoch量一次」在長局會因時鐘漂移失準（[fix/08](fix/08-a1-clock-drift.md)），與reseed取消（fix/02）
及fix/03、fix/09一起處理。使用者核准後已實作「輸入worker 60/s token bucket＋閉環的持續相位追蹤」與連線品質移出
（[fix/10](fix/10-connection-quality-eviction.md)），CPU驗證通過，經分支`claude/pvp-v5-phase-tracking`（PR #11，合併為`bec86b7`）；
A1、HostLate、低幀率守門與`StartPhaseSkip`已刪除。實機report-only GUI冒煙1輪PASS（可見P50／P95 36.8／38.8ms；不計次、不作為驗收證據），兩方量測全程tracking。
**結案驗收（2026-10-02，來源`2cecd3a`）：計次GUI可見延遲短測3輪有效輪皆通過（可見P50 37.3／36.6／35.9ms、P95 38.2／40.8／37.3ms）；25案真網路矩陣25／25通過（1075／1075動作）；第03批結案。**經過與驗證見
[追蹤dev_log](../../../dev_logs/2026_10_02_pvp_v5_phase_tracking.zh-Hant.md)；整體思考鏈見[延遲整改回顧](LATENCY_CASE_STUDY.md)。

## 閱讀入口

1. [進度與停止規則](README.md)、[v5唯一契約](../../protocol-v5.zh-Hant.md)。
2. [03完整v5玩法與交付](03-v5-gameplay-and-delivery.md)、[第03批dev_log](../../../dev_logs/2026_09_28_pvp_v5_batch03.zh-Hant.md)。
3. 先處理下述時序守門及[修正與已知問題](fix/README.md)（每個問題一份：成因／影響／復現／解決方案）；
   [04完整動作呈現](04-complete-action-presentation.md)尚不可視為依賴已驗收，須另外明確啟動。
4. 第02批人物校準、GUI證據與限制見[第02批dev_log](../../../dev_logs/2026_09_28_pvp_v5_batch02.zh-Hant.md)。
5. [原始v4基線與素材／測例清單](BASELINE.md)。v4完整認證只涵蓋原指紋，不自動涵蓋本候選。

## 已實作的第03批

- Client、產品Gateway、Match及兩份proto／Go bindings／HTTP join／Ready／Welcome
  一起使用v5，拒絕v1–v4；試跑需使用同批三角色並重啟舊程序。
- Match擁有跳躍規則、12發彈匣、10Tick單發、90Tick換彈、100HP／25傷害及180Tick重生。
  規則由Ready／Welcome下發，Client不以CSV或FBX時長決定玩法。
- 共用3D固定步、膠囊掃掠／支撐／頂頭、垂直預測／replay／鏡頭校正。
  Space沿只在第一個合法command消費；Held無jump，窗口滿／死亡／失焦／重同步清沿。
- LifeGeneration與movementEpoch獨立；重生保留PlayerId／Session及持續ActionId／帳本。
  舊生命請求仍得terminal裁決與ACK，晚到結果不改新生命HUD。舊移動取消另列觀測。
- Dead抑制操作／瞄準，保留中立命令與垂直落地；死亡取消換彈。
  到期先重生／補彈，然後全體移動，最後按穩定順序裁決動作，無新增同Tick互殺。
- 正式Space／R／左鍵及HUD已接入；R同幀優先，按住不連發，換彈／冷卻點擊不排隊。
  HUD使用最新權威HP／ammo／reload／death倒數；本機換彈請求有即時文字提示。
- 遠端位置／combat取同一呈現區間；生命或生死變更建立新段，人物相位重設。
  本批只有Idle／Jog及原槍模回饋，完整Jump／Reload／Death與素材快射重定時留第04批。

## 實作定位與下一批注意

- `Movement.*`／`PvpMatch.*`：產品權威、固定物理與生命狀態機。
  `Combat.hpp`：玩法唯一預設及ActionKind；`Arena`提供移動規則。
- `ClientConnection.*`：完整v5驗證、life／epoch窗口隔離、動作可靠交付與Drain。
  通用入口`SubmitAction(kind, life, observedTick, yaw, pitch)`；`SubmitShot`保留便利包裝。
- C++／proto保留歷史名稱`ShotRequest`／`ShotDecision`／`ShotRejection`與`shots`容器，
  實際支援Shot／Reload。wire enum的0是無效值；C++與wire拒絕值部分不同，必須明確映射。
  Shot角度要求presence，Reload不得帶角度；不要用static_cast替換映射。
- `LocalPlayerPrediction`含verticalVelocity／grounded／life；`SnapshotTimeline`回傳同段combat。
  `PvpApplication`以最新生命限制操作；切換所在幀丟棄轉換前收集的控制沿。
- `WeaponFeedbackObservation`提供life、HP／ammo、reload起訖／進度、death倒數與裁決kind；
  `RemoteMovementObservation`及人物frame帶life／dead。只讀觀測不是正式遊戲測試控制。
- 第04批沿用第02批女性／UAL／世界槍及Mark23，明確產品骨骼遮罩，不導入Enemy／Campaign。
  Reload原素材約3.733秒，Shoot約0.333秒；玩法時程分別1.5／約0.167秒，需做呈現映射。
  Jump Start／Land不可延遲物理；死亡動畫不能決定重生。人物命中仍是膠囊。
- 當前兩人產品Join前預備一個遠端GPU instance，Leave清識別但重用GPU；身高1.8、
  固定腳底anchor、Jog相位按路程。後退反向、側移近似，不宣稱全週期零滑步／IK。
- 第01–03批內容已存於`de87bb9`（wip）；啟動相位對齊經PR #2（分支`claude/project-thread-lv6bg5`）
  合併為`ff11ee3`；低幀率守門與驗收器修改經PR #3（`claude/pvp-v5-start-phase-guard`）合併為`9cd7f26`。
  不可把前批修改當無關內容刪掉。

## 驗證與證據

本批根目錄：`build/target/_build/test/logs/pvp-v5-batch03-20260928/`。
| 驗證 | 結果／入口 |
|---|---|
| 建置／CPU | 最後Client／Match／Gateway及probes成功；domain116 cases／1,395,489 assertions，人物12 cases／4,189 assertions |
| Go／wire | unit／race通過；完整datagram含頭仍≤1200；`go-validation.log`為原工具結果轉錄 |
| 證據工具 | 10項CTest短回歸、最後gameplay分析器17反例通過 |
| 真網路 | 25／25案、1075／1075動作逐生命交付退休；`gameplay-combined-25.json` |
| 恢復 | 新動作最慢1.046秒、穩定Actual起點0.550秒且維持250ms，原1.5秒門檻不變 |
| 主迴圈停頓 | 108／250／6000ms通過，worker存活；`main-stalls-1/recovery-results.json` |
| 生命清理 | 死亡等待中Leave／重入、Match斷線清空life／world／rules／帳本；`network-lifecycle-1/` |
| 真原生GUI | `gameplay-gui-3/`通過Space／快射／HP／R／死亡／180Tick重生／Esc；HUD圖已檢視 |
| 可見延遲 | `gui-timing-1`乾淨P50／P95 51.125／52.370ms未達標；`gui-timing-2`42.914／43.724ms通過，皆20／20配對 |
| 架構／指紋 | `architecture-fitness.json`靜態11項通過，未重跑完整remove build；`completion.json`為最後指紋且acceptance_complete=false |

原始失敗與退出碼見`execution-ledger-final.json`／`acceptance-bounded-summary.json`。
驗收器曾誤把故障時過期參考要求接受、把hold觀測算送包，已補反例並另寫重分析，原檔保留。
gui2有未排程mouse delta，來源未證實；不把受干擾結果或原GUI延遲失敗靜默剔除。
測試程序已全部退出，沒有要求使用者啟動服務或補跑長測。

## 第03批結案前的阻擋：啟動相位（已處理，見開頭）

`startup-phase-analysis.md`以相同command cursor分解交越：第02批本機→權威約24.504ms，
這次失敗輪30.698ms；權威→遠端約20.228／20.477ms，人物呈現並未增加約6ms。
首次輸入接納後等下一Authority Tick為9.073→14.781ms，首命令相位差6.675ms持續整場。
相關兩步lead／首次發布／獨立60Hz／一Tick插值公式本批未改。

此證據說明固定參數不保證每個啟動相位都達可見P50≤50ms：w（0～16.7ms）在啟動時抽定，
之後整個epoch固定加在延遲上。推估約18%的啟動會超過50ms。

**2026-10-01修正（方案A1，已獲使用者批准的契約補充）**：
- `MatchRuntimeHost`記錄每個epoch首窗口（含seq1）的收到時刻，到執行seq1的Tick時算出等待w，
  以可選`PlayerState.epochStartWaitMicros`／wire `epoch_start_wait_us`隨該epoch的Snapshot下發。
  Match不讀取，也不改Tick排程。
- `LocalPlayerPrediction`記錄首窗口發布時首個合法步已過時間；收到w後每epoch一次把固定步相位
  調整「w＋已過時間−4ms」，每幀最多移動經過時間25%，本機顯示不倒退。w>1Tick＋2ms
  （Host遲到）、epoch中途重新播種或首窗口後丟棄時間時不採用。
- 這是相位調整，不是延遲本機顯示：命令仍在產生幀取樣並立即顯示，只是改在離執行Tick
  較近的固定步邊界產生。lead仍是2個中立命令，序號→Tick對應、命令數、插值與門檻不變。
- 取捨：每個啟動的抖動餘裕固定在目前「w≈4ms的幸運啟動」水準，不再有w≈15ms時的額外餘裕。
- 驗證：C++ 118 cases／1,398,468 assertions（含既有486組恢復矩陣改為經過模擬Host回報w）、
  新增相位矩陣（30／60／144FPS×RTT0／20／40×worker×12個Authority相位）全部Actual、零重設；
  模擬的「命令產生→執行」中位數：144FPS各相位差≤1幀，最差相位改善≥1/3Tick
  （RTT0最差相位：60FPS 50→37.5ms、144FPS 45.8→36.1ms）。這是CPU模擬，不是可見延遲。Go unit／race及CTest `-L pvp` 11項通過
  （`presentation_cpu`因本環境無法下載shader工具未執行）。
  2026-10-02更正：該相位矩陣的模擬worker與幀時鐘相位鎖定；改用產品式worker／漂移／抖動後，
  30 FPS對齊時Held 10–14%，「全部Actual」不成立，見[fix/01](fix/01-a1-low-fps-regression.md)。
  PR head `d74830c`的本機CPU執行為119 cases／1,398,491 assertions（`ff11ee3`測試來源亦為119個TEST_CASE），
  與上列118不同。
- 未完成：原生雙GUI可見延遲短測、真網路25案矩陣未重跑。結案前須重跑GUI短測，
  並在結果記錄每輪的w與相位調整量；不可改門檻或重跑挑分數。
  更正（2026-10-02）：可見延遲短測`run_timing.py --gui --short`以`SDL_PushEvent`注入輸入，
  可在macOS／Metal執行，不需X11；只有`run_native_window.py`、`run_gameplay_gui.py`等原生視窗runner需X11／XTest。

### 2026-10-02：低幀率守門與暫緩結案

- 已做（細節各見fix文件）：
  - 低於約54.5 FPS不對齊：最新32個幀間隔（各最多計2Tick）平均>1.1Tick時不採用或撤回，
    ≤1.06Tick持續32幀才恢復。[fix/01](fix/01-a1-low-fps-regression.md)
  - Client本機診斷`StartPhaseSkip`（HostLate／FrameRateBelowTick／CancelledByReseed），非wire；
    驗收器逐epoch記錄w、調整量、撤回與取消。[fix/07](fix/07-start-phase-diagnostics.md)
  - 驗收器：跨行程時鐘域[fix/04](fix/04-macos-clock-domain.md)、
    等Match ready再啟Gateway[fix/05](fix/05-match-gateway-startup-race.md)、
    macOS視窗對角配置、視窗干擾判定與補跑規則[fix/06](fix/06-gui-window-interference-and-rerun-rule.md)。
  - 驗證：C++ 133 cases／1,418,939 assertions、CTest `-L pvp` 16／16、Python 165項通過。
    GUI冒煙3次與矩陣單案冒煙1次皆report-only，不計次、不作為驗收證據。
- 未做（當時）：計次GUI可見延遲短測、25案矩陣、第03批結案、第04批。其後處理見本文件開頭與追蹤dev_log。
- 未解決（暫緩，無承諾）：stall reseed使該epoch其餘時間失去對齊；本機三次GUI冒煙的移動方
  都被視窗啟動卡頓取消，冒煙未量到對齊後的可見延遲。[fix/02](fix/02-a1-cancelled-by-stall-reseed.md)
- 已知問題：切點以上偶發掉幀造成starvation重設；55–58 FPS規律掉refresh時對齊與否取決於啟動時機。
  [fix/03](fix/03-a1-missed-frame-starvation.md)
- 若恢復結案：先讀fix/02與fix/06；閒置機器、一次一輪，事前宣告總輪數與補跑規則。

### 本機驗證環境（Intel Mac）

- MacBook Pro 2019（x86_64）、macOS 26.7.1、Xcode 26.6＋Metal toolchain；GUI量測須機器閒置、一次一輪。
  完整平台、工具鏈與閒置條件以[fix/02](fix/02-a1-cancelled-by-stall-reseed.md)的「如何復現 A」為準。

## 架構與停止邊界

Architecture Delta限於PvP的wire／runtime狀態、Client操作／呈現及專用驗收。
Match仍不載Model／FBX／Renderer／SDL；Engine和公共Gateway不識別FPS政策。
不新增Top-level subsystem、跨v2／Editor依賴或通用可靠傳輸框架。
2026-10-02守門只新增產品內常數與Client本機診斷，未改Engine或wire；見[守門dev_log](../../../dev_logs/2026_10_02_pvp_v5_start_phase_guard.zh-Hant.md)。

v4的`MANUAL_ACCEPTANCE.md`、`ACCEPTANCE_STATUS.md`、`STABLE_BASELINE.md`維持原文及指紋。
證據位於git忽略的build目錄，提交文件不會保存原始trace／圖像，需另行備份。
第03批短驗證後停止；第04批與最終完整驗收須另外授權，不自動跑三輪GUI或30分鐘長測。
