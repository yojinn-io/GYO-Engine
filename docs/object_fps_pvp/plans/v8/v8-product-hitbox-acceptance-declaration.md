# v8-product-hitbox-acceptance 的事前宣告

更新：2026-11-03。Owner：`object_fps_pvp`。**狀態：已核准**（`v8-product-hitbox-acceptance-declaration-approve`，2026-11-03，4 項照建議 (a)；檔位照建議。L2 的機器尚待使用者告知，A3 在那之前不開工）。計畫：[hitbox-plan](hitbox-plan.md) 第 8 節（「L1、相關突變、B4／B5 的量測、L2 試玩（收支行）」）。估時 8～14 h（計畫估 4～6 h，見「為什麼比計畫大」；超過 21 h 停下回報）。

## 這批做什麼

hitbox-line 的收尾驗收：

- 已完成、還沒進 master 的程式，開 PR 跑 L1：`v8-product-real-match-acceptance-repair` 與 `v8-product-hit-clip-by-part`。這包括新的 `animation_digest`（`16fdd42a3aa49d35`）在四平台是否一致。
- 量 B5：Client 畫面上的骨骼和 Match 判定用的骨骼差多少。
- 準備 L2 的離線跑次腳本，並在 L2 跑：
  - P2 移過來的 G5、G6（`v8-product-hit-gui-aim-l2`）；
  - 試玩：形狀大小、頭部受擊的觀感。
- 合併前的完整突變（D184）。

不在這批：
- 回溯（rewind-line，排在 entity-line 的 split 之後）。
- `run_timing.py --gui --combat` 的 HUD 檢查失敗：HANDOFF 未結事項，另外處理，先建檢測。

## 開工前查到的事實（2026-11-03）

1. **B4（wire 大小）已經量過，不必重做**：A3 實測 snapshot 884／1022／842 B，D3 的 Welcome 多 10 B，都在宣告的上限內。這批只在 dev_log 引用。
2. **B5 還沒量**：
   - S2 的「every kind of input」證明的是：同一組權威值下，兩邊的姿勢逐位元相同。
   - 但 Client 的遠端玩家是在兩個 snapshot 之間內插後才畫（呈現時間不是整數 Tick），Match 用的是 Tick J 的值。兩者實際差多少沒有量過。
   - 已知的差：判定的 Tick 動作重新開始時最多 0.14 m（S2、P1 的量測）。
3. **G5、G6 只能在 Linux 桌面跑**：`run_gameplay_gui.py`、`run_native_window.py` 靠 X11／XTest。這台 Mac 跑不了，所以 kit 裡 X11 的部分無法在本機乾跑。
4. **P2 的程式還在工作樹，未提交**（`v8-product-hit-gui-aim-l2`）：`gui_aim.hpp`、`combat_latency.hpp`、`gui_main.cpp`、`native_window.hpp`、`run_native_window.py`。IU-3 合併後重新建置通過。
5. **CI 只跑 `cpu|shader` 標籤**：`hit_part_cross`、worker 和真 Match 的 runner 都只在本機跑。

## 為什麼比計畫大

- 計畫估 4～6 h 時，還沒有 P2 的 G5、G6 移進來，也沒有「離線場次每台一支腳本、交出前實際跑過」的要求。
- 計畫原本寫 Sonnet；這批是你指定為重要的 Feature（① 部位 hitbox），屬上段，照 navigator.md 不能用小模型。

## 分成四個子批

```text
v8-product-hitbox-acceptance
├── A1 v8-product-hitbox-pr        「上段｜開 PR、L1、H6′（新 digest 四平台）、合併前的完整突變」
├── A2 v8-product-hitbox-b5        「上段｜量 B5：Client 內插後的骨骼與 Match 判定的骨骼差多少（只量不判，或照決定）」
├── A3 v8-product-hitbox-l2-kit    「上段｜L2 的離線 kit：每台一支腳本（啟動、探測、跑 G5／G6、收 log）」
└── A4 v8-product-hitbox-l2        「上段｜L2 場次：G5、G6、試玩、收支行」
```

| 代號 | 估時 | 依賴 | 檢收點 |
|---|---|---|---|
| A1 | 2～4 h（含 L1 等待） | 本宣告 | K1～K3 |
| A2 | 2～3 h | 本宣告 | K4 |
| A3 | 2～4 h | `v8-product-hitbox-l2-machines` | K5 |
| A4 | 2～3 h＋你的時間 | A3 | K6～K8 |

A1、A2 可以馬上做；A3 等你告訴我 L2 的機器。

## 模型與檔位（navigator.md；檔位照建議）

| 子批 | 段 | 根據 | 実装者 | 審査者 | 検証者 |
|---|---|---|---|---|---|
| A1 | 上段 | 你指定為重要的 Feature；資料契約（digest）的跨平台判定 | Opus 5.5／medium（執行與記錄） | 不需要（不寫程式） | 不需要（不改判定） |
| A2 | 上段 | 同上；量測工具會動到 player_content 的呼叫 | Opus 5.5／high | Opus 5.5／high | 不需要 |
| A3 | 上段 | 同上；腳本是場次的唯一入口 | Opus 5.5／high | Opus 5.5／high（逐條核對選項與 probe 的限制） | 不需要 |
| A4 | 上段 | 同上 | Opus 5.5／medium（照宣告判定與記錄） | 不需要 | 不需要 |

- 計畫原本寫 Sonnet 5.5，照 navigator.md 改成 Opus。
- 如果某一批需要改既有判定，照 §14.4 另外指派検証者（不用小模型）。

## A1：開 PR、L1、H6′、完整突變

- **PR 的內容**（依 `v8-product-hitbox-pr-order` 決定）：
  - 本分支已提交的 `v8-product-real-match-acceptance-repair`、`v8-product-hit-clip-by-part`；
  - 加上 master 合併後的文件。
- 判定：
  - **K1**：L1 三平台通過。
  - **K2（H6′）**：`animation_digest` 在 Intel Mac（本機）、linux-x64、macos-arm64、windows-x64 都是 `16fdd42a3aa49d35`。從 L1 artifact 的 `tests.xml` 讀；不同就停下回報。
  - **K3**：合併前在背景跑完整突變（D184），只在預期失敗時才算 killed。全部 killed；有存活或 stale 的就停下回報。
- 合併照 PR 合併的規則：L1 全過後，由你確認，再用 `gh pr merge --merge`。

## A2：量 B5（決定 `v8-product-hitbox-b5-threshold`）

- 方法：同一段權威序列（S2 的情境：跑步、跳躍、蹲下、受擊、換彈、轉向）。
  - Client 側照 `PvpApplication` 的作法，在兩個 snapshot 之間依呈現時間內插，取樣骨骼。
  - Match 側用各 Tick 的權威值取樣（姿勢模型）。
  - 比較同一個時刻（Client 的呈現時間對應到 Match 的 Tick）7 個形狀的端點距離。
- 記錄最大值與分布（P50、P95），依情境分開。
- **K4**：依決定，(a) 只量、記錄（不判通過或失敗），或 (b) 照宣告的門檻判定。

## A3：L2 的離線 kit（決定 `v8-product-hitbox-l2-machines`）

- 每台機器一支腳本，包辦：
  - 啟動（Match／Gateway／Client 或 GUI probe）；
  - 探測：ping、列出所有網卡並讓你確認同網段；
  - 跑 G5、G6；
  - 收 log 並打包。
- 程式與資產事先完整放在機器上，現場不靠網路。
- **K5**：
  - 交出前，每條命令和每支腳本，在能跑的機器上實際跑過一次。Mac 用 127.0.0.1 跑非 X11 的部分。
  - X11 的部分無法在 Mac 乾跑，這是範圍的限界。場次開始時，先在 Linux 上乾跑 G5、G6 各一次，失敗就停下，不進入試玩。

> **2026-11-03 修改（`v8-product-hitbox-g5g6-docker`）**：使用者提議「我可以啟動 Docker，你直接用 docker 測試」。G5、G6 改在本機的 Docker 容器裡跑。
> - 環境：ubuntu 24.04，套件和 CI 的 Linux 列相同；Xvfb 加 Mesa lavapipe（軟體 Vulkan），openbox 當視窗管理員；`ci-linux` preset 在容器內建置。
> - Dockerfile 與建置腳本在主 checkout 的 `build/evidence/v8-hitbox-l2-docker/`。
> - 判定（K6、K7）不變，時間檢查的重跑規則也不變。
> - 量不到的：實體 GPU 與驅動、真的桌面與合成器、真實的輸入裝置；Docker Desktop 的 VM 的計時和實機不同。所以時間檢查的失敗不能直接當成產品的缺陷，照 §14.1 切分。
> - 試玩（K8）仍要實機與人，等使用者告知機器；A3 的 kit 只剩試玩用的部分。

> **2026-11-03 試行 1 的結果與重跑的宣告（`v8-product-hitbox-g5g6-rerun`，使用者：「新宣告修正重跑」）**
> - 試行 1（Docker，證據在 `build/evidence/v8-hitbox-l2-docker/`）：
>   - G5 **不成立**：「12tick native shot rejected or missed」。`run_gameplay_gui.py` 有自己的原生滑鼠 `aim(yaw)`，pitch 固定拉回 0，P2 漏改。原宣告對 G5 的描述（17 格的 SDL 排程）也錯了。
>   - G6 **不成立**：V3、V4（四槍的 hp 路徑）通過，之後截圖時容器缺 `xwd`（環境）。
>   - 兩次失敗都保留在紀錄裡。
> - 修正：
>   - `run_gameplay_gui.py` 的 `aim` 改成可帶 pitch，射擊前轉向 probe 狀態檔的 `torso_aim`（和 G6 相同）。新生命那一槍不判命中，照舊。
>   - Docker image 加 `x11-apps`。
> - 重跑的判定（看到結果後不改）：G5′、G6′ 各跑一次，要整體通過；只在時間檢查失敗時可重跑一次，傷害、生命相關的錯誤不重跑，直接停下回報。
> - **範圍的限界**：P2 改的 SDL 排程（`combat_latency.hpp`）只有 `run_timing.py --gui --combat` 用到，G5′、G6′ 都驗不到它；那條路徑卡在 HUD 檢查的未結事項，在那之前寫成「未驗證」。

> **2026-11-03 試行 2 的結果與第三次試行的宣告（`v8-product-hitbox-g5g6-trial3`，使用者：「修正後重跑」）**
> - 試行 2：
>   - G5′ **不成立**：第三槍打到腿。
>   - G6′ **不成立**：V3、V4 通過後，容器缺 PIL（環境）。
>   - 紀錄保留。
> - 觀察跑次（不算判定）找到原因：probe 的 `torso_aim` 正確（0.0625），但 runner 的 `aim()` 送出移動後只等 80～100 ms 就讀狀態檔，在約 27 fps 時讀到舊值，於是同一個修正送了兩次，相機停在 0.125。
> - 修正：
>   - 兩支 runner 的 `aim()` 每次移動後，等到 `mouse_consumptions` 增加、而且連續兩次讀到相同的 yaw／pitch，才算下一個修正；
>   - Docker image 加 `python3-pil`；
>   - 新增單元測試「the probes' torso aim is judged Body from every product spawn」，守住瞄準點。
> - 判定不變：G5″、G6″ 各一次，要整體通過；只在時間檢查（例如 G5 的射擊間隔 10～20 Tick）失敗時可重跑一次。傷害、生命相關的錯誤不重跑，直接停下。時間檢查重跑後仍失敗，記為 Docker 環境的限界，G5 留到實機。

## A4：L2 場次

- **K6**：G5 `run_gameplay_gui.py` 預設案例通過（`combat_gui_evidence` 的 17 格，hp 照每發 25000）。
- **K7**：G6 `run_native_window.py` 通過（hp 路徑 75000→50000→25000→0）。
  - K6、K7 都是：只在時間檢查失敗時可以重跑一次，兩次都記錄；傷害、生命相關的錯誤不重跑，直接停下回報（沿用 P2 的宣告）。
  - K6、K7 通過後，P2 的程式才提交，並寫 P2 的 dev_log。
- **K8 試玩（記錄，由你判斷）**：
  - 形狀的大小：頭、軀幹、腿打起來是否合理；
  - 頭部受擊的動作看起來是否自然；
  - 斷線、重連各一次後，部位判定是否照常。
  - 收支行：送出的射擊數、收到的決定數、計數的拒絕數，從 log 對帳。
  - 你覺得難打中時，記下來交給之後的形狀調整，不在這批改半徑。

## 突變

- A1 的完整突變就是 D184 的合併前全量。
- A2 新增的量測工具只回報、不判定（選 (a) 時），不登記突變。選 (b) 時，門檻判定要有突變：把 Client 的內插改成不內插，判定要失敗。
- A3、A4 的腳本不在 CTest。K5 的乾跑就是它們的檢查。

## 等你決定

### `v8-product-hitbox-pr-order`：P2 要不要等 L2

- (a) 現在就開 PR，不含 P2；P2 在 L2 通過後另開一個小 PR（建議）。
  - 理由：
    - L2 的機器與時間還沒定；
    - repair 與 P1 已完成，新 digest 的四平台一致（H6′）越早看越好；
    - 分支不會拖太久。
  - 代價：多開一個 PR，多跑一次全量 CI。
- (b) 等 L2 通過，P2 一起進同一個 PR。
  - PR 最少，但 H6′ 和 repair 都要等 L2。

### `v8-product-hitbox-l2-machines`：L2 用哪些機器

- 需要你告訴我：
  1. 一台有 X11 桌面的 Linux（G5、G6 與 Match 都可以在它上面跑）。Wayland 的話，要確認能不能用 XWayland 跑 XTest。
  2. 試玩用的 Client 機器（Windows？台數？）。
  3. 場次時有沒有網路。
- 機器確定前，A3 不開工。

### `v8-product-hitbox-b5-threshold`：B5 要不要判定

- (a) 只量、記錄（建議）。
  - 理由：沒有根據可以訂門檻；B5 的作用是讓試玩時的觀感有數字可對照。
- (b) 宣告門檻判定：例如 P95 ≤ 最小形狀半徑的一半。形狀半徑是 0.105～0.184 m（`hitbox.json`），最小的一半約 0.05 m。
  - 數字是推論，不成立時要回頭決定改什麼。

### 模型表

- 照上表（四批都 Opus 5.5，A2、A3 用 high，A1、A4 用 medium）（建議）。

## 範圍的限界

- K2（H6′）只看 L1 跑的三個平台加本機；macos-x64 由本機代表。
- G5、G6 只在一台 Linux 桌面跑；Windows 上的 GUI runner 不在範圍內。
- B5 只用合成的權威序列；真實網路下的內插誤差（抖動、遺失）不在範圍內，在 L2 的觀感裡間接看。
- HUD 檢查的失敗另外處理；如果 L2 的 G5、G6 也碰到同樣的 HUD 錯誤，照 §14 停下，不在現場改。
