# worker 驗收探針：ACK 檢查的相位與 D226 的計時器喚醒（v7.1 之後）

v7.1 留下的兩個未量的問題（HANDOFF「範圍的限界」：「D226 的 2 次計時器喚醒來源不明；ACK 檢查的相位問題沒有量」）。背景與 D225～D227 的量測在 [worker-windows-timing.md](worker-windows-timing.md)。只改 worker 探針（`build/acceptance/object_fps_pvp/worker_main.cpp`），產品程式不改。

## 讀程式得到的假設（還沒量）

網路執行緒的喚醒只有三種：socket 可讀、別的執行緒 post（SubmitAction、Drain 推進 ACK、新的輸入、請求），以及一個計時器（`ClientConnection.cpp` 的 `Arm()`）。計時器排在所有期限中最早的一個（Hello 的 250 ms、liveness、輸入送出、動作送出、10 秒的統計）。期限超過 2 ms（`CoarseWakeLead`）時，計時器設在**期限前 2 ms**，那一輪 `Service` 跑完一次，再用 Waiter 等到期限本身，跑第二輪。

- **問題 2 的假設 H**：D226 的 2 次「mock 看不到的喚醒」是 Hello 期限前 2 ms 的那一輪。那一輪不送 Hello（還沒到 250 ms），但會送出已可送的 action；Hello 約 2 ms 後才送出。D226 的時間線只印到 action 到達為止，所以看不到後面的 Hello。
- **問題 1**：`v7-08-drain-no-wake` 讓 Drain 不喚醒網路執行緒，ACK 會等到下一次無關的喚醒才送出：Hello 那兩輪（含前 2 ms 那一輪），或 mock 回覆 Hello 的 Welcome／Snapshot 抵達的那一輪。Drain 若落在某次 Hello 之後幾 ms 內（回覆還沒到），或下一次 Hello 前約 42 ms 內，突變就會碰巧存活。註解寫的「next Hello, up to a second away」與 `HelloInterval`（250 ms）不符。

## 事前宣告（D243，2026-10-29 核准）

使用者：「核准 worker-wake-phase.md 中 D243 的事前宣告，開始跑。」核准前沒有跑任何量測（只建置了 5 個執行檔）。以下內容核准後不改，跑完不放寬。

### 共同

- 本機 macOS（Intel），worktree 的 `build/target/_build/test`，探針直接執行（不經 CTest），串行。每次前記錄 load average（1 分鐘），保存完整輸出。跑的期間我不做其他工作；同一台 Mac 上其他會話的活動控制不了，照實記錄。
- 執行檔事先建好、複製到 scratchpad 再跑；跑的期間不改工作樹。突變版的建法：把 `tests/object_fps_pvp/mutations.json` 該條的 `find` 換成 `replace`，建置、複製執行檔、還原檔案。
- 觀測都只輸出到 stderr，不影響判定。

### 量測 1：ACK 檢查的相位（問題 1）

- **觀測（要提交的版本）**：在 ACK 檢查（「an advanced ACK waited for an unrelated wake」）之前印一行 `v7-08 ack timeline:`，沿用 `WakeTimeline`，以 Drain 前的時刻（`drainedAt`）為 0：Drain 前 20 ms 到第一個 ACK 到達後 5 ms 之間 mock 收發的封包，加上 Drain 之前最後一個 Hello 的到達時刻（`prior_hello_ms`，正值＝Drain 前幾 ms）。D226 的那一行也加上 `prior_hello_ms`，格式其餘不變。
- **兩個執行檔**：O＝上述觀測版；M＝同一份加上 `v7-08-drain-no-wake`。O、M 交錯（OMOM…），各 20 次，共 40 次。
- **每次的結果**：
  - M：killed＝結束碼非 0 且輸出含「an advanced ACK waited for an unrelated wake」；survived＝結束碼 0；其他＝其餘。
  - O：通過＝結束碼 0；其他＝其餘。
- **收支**：40 次都要有結果，而且每次都要有 `v7-08 ack timeline:` 這一行（M 的 killed 也要，行印在 Require 之前）。
- **判定**（互斥，依序取第一個成立的）：
  1. 「確認不了」：任一執行檔的「其他」超過 2 次，或有任何一次（不含「其他」）沒有印出時間線。
  2. 「有相位問題」：M 有 1 次以上 survived；或 40 次中有任何一次 `prior_hello_ms` < 5（Drain 在 Hello 之後 5 ms 內，回覆可能在 Drain 之後才到）或 > 200（Drain 在下一次 Hello 前 50 ms 內：40 ms 的判定＋前 2 ms 那一輪＋約 8 ms 的節奏餘裕）。
  3. 「目前的流程避開了 Hello」：M 20／20 killed，而且 40 次的 `prior_hello_ms` 都在 5～200。
- **之後（依判定，再交你決定）**：2 → 提議比照 D227 修（Drain 前等 mock 收到 Hello 再隔約 100 ms），並另寫突變確認的宣告；3 → 目前流程碰巧避開，是否仍比照 D227 固定相位，交你決定；1 → 交你決定。
- **另外記錄（描述，不判定）**：M 的 ACK 是哪一個事件之後送出的（Hello 之後、回覆之後、或前面沒有封包）。

### 量測 2：D226 的計時器喚醒的來源（問題 2）

- **量測用的探針（只在 scratchpad，不提交）**：從量測 1 的觀測版改兩處：(a) v7-08 action 檢查的 SubmitShot 改在 mock 收到 Hello 後 **240 ms**（下一次 Hello 期限前約 10 ms，在前 2 ms 那一輪之前）；(b) action 到達後再等 10 ms，時間線印到 action 後 10 ms。
- **三組**：
  - E-M：(a)(b)＋`v7-08-submit-no-wake`，20 次。
  - E-O：(a)(b)，沒有突變，10 次（對照：action 應在 SubmitShot 自己的喚醒時送出）。
  - R-M：只有 (b)，SubmitShot 的時刻回到 D227 之前（閒置 50 ms 後立刻送），加 `v7-08-submit-no-wake`，20 次。這是重現 D226 的條件，看「前面沒有封包」的那幾次後面是不是緊跟著 Hello。D226 的出現率約 2／20，**可能 0 次出現，那時 R-M 不提供結論**。
  - 交錯（E-M、E-O、R-M、E-M、R-M…），共 50 次。
- **每次的分類**（看時間線；SubmitShot＝0）：
  - 「前 2 ms 的那一輪」：SubmitShot 與 action 之間 mock 沒有收發任何封包，而且 action 之後 0.5～3.0 ms 內 mock 收到 Hello。
  - 「跟 Hello 同一輪」：action 之前 3 ms 內 mock 收到 Hello（或收到 Hello 後 mock 的回覆在 action 之前）。
  - 「SubmitShot 自己的喚醒」：action 在 SubmitShot 後 2 ms 內到達，之前沒有封包，之後 3 ms 內沒有 Hello（只有 E-O 預期）。
  - 「其他來源」：action 之前沒有封包、之後 3 ms 內也沒有 Hello，而且不在 SubmitShot 後 2 ms 內（或在 E-M 中出現前一類）。
  - 「其他」：結束碼非 0、時間線沒有印出等。E-M、E-O 的 40 ms 判定預期會通過（action 約在 8 ms 或 1 ms 內到達）；E-M 若結束碼非 0，記為「其他」並保存輸出。
- **判定**（互斥，依序取第一個成立的；依據 E-M）：
  1. 「確認不了」：E-M 或 E-O 的「其他」超過 2 次。
  2. 「是 Hello 期限前 2 ms 的那一輪（H 成立）」：E-M 20 次中至少 18 次是「前 2 ms 的那一輪」，而且「其他來源」0 次。
  3. 「有別的來源」：E-M 有 3 次以上「其他來源」。
  4. 「判定不了」：以上都不成立。
- **R-M（描述，不判定）**：「前面沒有封包」的每一次，後面 3 ms 內有沒有 Hello。有 Hello，就把 D226 的那 2 次歸到同一個機制（推論，因為 D226 的時間線沒有後面的部分，無法直接證明）；沒有，記為「其他來源」並交你決定。
- **之後（依判定，再交你決定）**：2 → D226 的來源記為 Hello 期限的前 2 ms 一輪（產品的設計，不是缺陷），D227 的 100 ms 間隔已經避開（前 2 ms 那一輪在 Hello 後約 248 ms）；檢查不必改，只更新註解與文件。3、4 → 依那些時間線，提議下一個觀測（可能要在產品側記錄喚醒的原因，那是 Architecture Delta，先問你）。

### 範圍的限界

- 只在 macOS（Intel）；Windows 上的 v7-08 兩項不在範圍（D231 的限界照舊：Windows 的 worker 驗收不算發行條件；mock 的 `Pump` 遇到 WSAECONNRESET 會中止驗收；「a token wait outlived four commands」以 mock 輪詢的約 1 ms 刻度量）。
- mock 看到的是封包的到達時刻，不是 Client 的喚醒本身；計時器是否在前 2 ms 喚醒，是從 action 與 Hello 的到達順序推的。
- 量測 2 的 240 ms 是為了讓 H 預測的現象每次都出現；它證明這個機制存在、而且會產生 D226 那樣的時間線，不直接證明 D226 那 2 次就是它（R-M 只能補強）。
- 20 次只能分辨相當大的差別。

### 估計時間

建置約 30 分（worktree 第一次設定）；量測 1 約 25 分；量測 2 約 30 分；判讀與文件約 30 分。

## 結果（2026-10-29，本機 macOS Intel）

收支：一致。量測 1 的 40 次、量測 2 的 50 次都有結果，每次都保存了輸出。量測 1 的「其他」1 次（O 第 1 次），量測 2 的 E-M、E-O「其他」0 次。

### 量測 1：判定 3「目前的流程避開了 Hello」

- M：20／20 killed。ACK 在 Drain 後 112.0～117.8 ms 送出。
- O：19 次通過，1 次「其他」（第 1 次，load 3.64）：失敗在別的檢查「worker emitted a resend catch-up burst」（`worker_main.cpp` 輸入重送的那段，不是 ACK 檢查），沒有時間線（那一步在 ACK 檢查之前）。那次的節奏行：Client 自己的送出間隔最小 16.72 ms，mock 看到的到達間隔最小 7.30 ms，也就是 mock 晚讀了一個封包。原因的切分（§14.1）：推論是環境或 mock 側（mock 的讀取延遲），不是產品；只發生 1 次，沒有再量。
- `prior_hello_ms`：40 次都在 131.0～136.0 ms（Drain 在某次 Hello 後約 133 ms）。下一次 Hello 前 2 ms 的那一輪在 Drain 後約 115 ms，離 40 ms 的判定約 75 ms。
- M 的 ACK 前面是什麼（描述）：18 次前面沒有封包、ACK 之後 1.7～2.0 ms 才收到 Hello；2 次（第 14、17 次）是 Hello 先到、mock 回覆之後的那一輪送出。ACK 的送出時刻（約 115 ms）＋`prior_hello_ms`（約 133 ms）≈ 248 ms，就是 Hello 期限前 2 ms。
- 意義：現在的流程讓 Drain 落在兩次 Hello 的中間，這項檢查目前不是靠相位碰巧。可是這個相位是前面各步驟的耗時累積出來的，不是這項檢查自己定的；前面的步驟改了，相位就可能移動。

### 量測 2：判定 2「是 Hello 期限前 2 ms 的那一輪（H 成立）」

- E-M：20／20 是「前 2 ms 的那一輪」：SubmitShot 與 action 之間 mock 沒有收發任何封包，action 在 SubmitShot 後 7.86～8.33 ms 到達，Hello 在 action 之後 1.61～1.97 ms 到達。「其他來源」0 次。
- E-O（對照）：10／10 是「SubmitShot 自己的喚醒」，action 在 0.13～0.30 ms 到達，Hello 在約 10 ms。
- R-M（描述）：15 次被 40 ms 的判定殺死（Hello 在 SubmitShot 之前 0.3～4.8 ms；action 在 243～249 ms，其中 13 次前面沒有封包、Hello 在 action 後 0.15～2.0 ms，同一個機制），5 次是 Hello 在 SubmitShot 之後、action 跟 Hello 同一輪（存活）。「前面沒有封包、action 卻很早送出」的情形（D226 的那 2 次）**0 次出現**，依宣告，R-M 不提供 D226 那 2 次的直接結論。
- load average（1 分鐘）：量測 1 為 0.96～3.72，量測 2 為 0.87～2.15。
- **分析程式的修正（§14.1 的切分：Analyzer 的錯誤）**：第一版分析把 action 自己的封包算成「SubmitShot 與 action 之間的封包」，E-M 有 7 次、E-O 有 2 次落到「未分類」。原因：時間線的 `action_ms` 取自 `actionAttempts` 的時刻，事件列表裡的 `in:Actions` 取自 `packets`，兩者是 `Pump` 裡分開的兩次 `Clock::now()`，差 0.01～0.02 ms。修正後只把 action 自己的那個封包排除在外，分類規則不變；修正前後兩個版本都留著（`analyze2_v1.py`、`analyze2.py`）。
- 意義：Hello 期限前 2 ms 的那一輪（`Arm()` 的 `CoarseWakeLead`）會送出已可送的 action，而 Hello 要約 2 ms 後才送出。D226 的時間線只印到 action 為止，所以這種喚醒在 mock 看來是「沒有封包的喚醒」。這是產品的設計（v7 第 08 批 `730984ec` 起：asio 計時器提前 2 ms 喚醒，最後一段用 Waiter，避開 macOS 的計時器合併），不是缺陷。D226 的那 2 次歸到這個機制是推論：R-M 沒有重現，但 D226 的 action 時刻（0.27、1.06 ms）與 D226 的 Hello 分布（SubmitShot 前後約 3 ms）都和它相容。
- D227 的修正（Hello 後 100 ms 才 SubmitShot）離前 2 ms 那一輪（Hello 後約 248 ms）約 148 ms，已經避開。

證據：本機 scratchpad 的 `m/`（git 忽略）：`variants.py`（5 個執行檔的建法）、`run.py`、`bin/`、`m1/`、`m2/`（每次的完整輸出與 `results.txt`）、`analyze2_v1.py`、`analyze2.py`。（2026-10-31 補註：這裡的 scratchpad 內容已經不在，未保留）

## 決定（D244）與 ACK 檢查的時機修正：事前宣告（2026-10-29 核准）

使用者：「問題 1 選 (b)：比照 D227 固定 ACK 檢查的相位，先寫突變確認的事前宣告給我核准。問題 2 照建議只改註解與文件。」核准：「核准 worker-wake-phase.md 中 D244 的事前宣告，開始跑。」核准前沒有跑任何量測（只建置了 3 個執行檔）。以下內容核准後不改，跑完不放寬。

- **修正（只改 `build/acceptance/object_fps_pvp/worker_main.cpp`，產品不改）**：
  - 新增 `AwayFromHello`：等 mock 收到下一個 Hello，從那個 Hello 的到達時刻起再等 100 ms。內容就是 D227 那一段，搬成函式；逾時訊息仍指出呼叫的那一行（`source_location` 往下傳）。
  - ACK 檢查：原本「閒置 50 ms 後 Drain」，改成閒置 50 ms 後先 `AwayFromHello`，再 Drain。Drain 前一次 Hello 在約 100 ms 前，下一次 Hello 前 2 ms 的那一輪約在 148 ms 後，兩邊都離 40 ms 的判定很遠。
  - v7-08 的 action 檢查（D227）：改呼叫 `AwayFromHello`，行為不變。
  - 註解（問題 2）：寫明 Hello 前 2 ms 的那一輪（`CoarseWakeLead`）也會送出已可送的東西；拿掉「next Hello, up to a second away」。
  - 40 ms 的判定與兩個失敗訊息都不改。量測 1 的時間線（`v7-08 ack timeline:`）與 `prior_hello_ms` 保留。
- **判定的變更（AGENTS.md §14.4）**：
  1. 守的性質：Drain 推進 ACK 時，喚醒網路執行緒立刻送出 ACK（v7 第 08 批），不等無關的喚醒。
  2. 放寬的面：預期沒有。判定的式子不變，只改 Drain 的時刻。舊判定殺死的突變（`v7-08-drain-no-wake`），新判定也必須殺死（下面第 1 項）；這項檢查沒有其他已知的失敗例。
  3. 收緊的面：Drain 的相位由這項檢查自己決定，不再依賴前面各步驟的耗時；前面的步驟改了，突變也不會因相位碰巧而存活。
  4. （確認沒有放寬面之後不適用）
  5. 證據：量測 1 的結果，以及本節的結果。
- **確認的項目與判定**（本機 macOS Intel，串行；每次保存完整輸出；每次前記錄 load average；第 1～3 項交錯 FM、FO、FM、FS，重複 10 輪）：
  1. **FM＝修正版＋`v7-08-drain-no-wake`，20 次**：killed＝結束碼非 0 且輸出含「an advanced ACK waited for an unrelated wake」，而且 `v7-08 ack timeline:` 的 `prior_hello_ms` 在 99～110。**通過＝20／20 killed**。
  2. **FO＝修正版，10 次**：結束碼 0，而且 `v7-08 ack timeline:` 與 `v7-08 wake timeline:` 的 `prior_hello_ms` 都在 99～110。**通過＝10／10**。
  3. **FS＝修正版＋`v7-08-submit-no-wake`，10 次**：killed＝結束碼非 0 且輸出含「an eligible action waited for an unrelated wake」。**通過＝10／10 killed**（D227 那一段搬成函式後檢出不變）。
  4. **worker 檢查的全部突變（47 個，`mutations.json` 中 `check` 為 `worker` 的）**：用產品的突變 runner 跑一次。**通過＝47／47 killed**。wrong_failure 與 survived 依 D185 分類：wrong_failure 只單獨重跑 1 次，killed 就記為環境；survived 不重跑，直接列為不通過。
- **整體**：1～4 全部通過，才算「修正有效」。任何一項不通過，就不再重跑、不修改，停下來回報（附時間線與輸出）。
- **範圍的限界**：
  - 只在 macOS（Intel）；Windows 上的 v7-08 兩項不在範圍。D231 的限界照舊：Windows 的 worker 驗收不算發行條件；mock 的 `Pump` 遇到 WSAECONNRESET 會中止驗收；「a token wait outlived four commands」以 mock 輪詢的約 1 ms 刻度量；D227 只在 macOS 用突變確認過（本次的修正也一樣）。
  - 量測 1 的 O 有 1／20 在另一項檢查（「worker emitted a resend catch-up burst」）失敗，推論是 mock 側的讀取延遲。第 1～3 項若再出現，照上面的規則記為不通過並停下，不重跑。
  - 同一台 Mac 上其他會話的活動控制不了，照實記錄。
- **估計時間**：第 1～3 項約 15 分鐘，第 4 項約 70 分鐘。

### 結果（2026-10-29，本機 macOS Intel）：不通過，停下

收支：不一致。第 1～3 項 40 次都有結果；第 4 項**沒有跑**（見下）。

- **判定：不是「修正有效」**（第 1 項不通過）。依宣告，不重跑、不修改，停下回報。
  1. FM：**17／20 killed**（不通過）。另外 3 次在 ACK 檢查之前的別的檢查失敗，沒有到達 ACK 檢查（沒有時間線）：
     - 第 2 次（load 2.03）：程序 abort。產品的 `GYO_ASSERT failed: found!=input.commands.end() && *found==existing`（`ClientConnection.cpp` 的 `SendInput`，network-architecture §7 的 r7：未確認的命令不能被拿掉或改內容）。crash report 的堆疊：探針的主執行緒 `main` → `SendInput`。最後一行輸出是重送節奏行，所以位置在其後的 60 FPS 發佈迴圈（`worker_main.cpp` 的 `for(std::uint64_t sequence=7;sequence<67;++sequence)`）。〔推論，沒有量〕這個迴圈每 16.7 ms 送一個只含新命令的窗口，靠 mock 的自動 ACK 把前一個命令確認掉；ACK 的快照若還沒被 Client 套用，新窗口就少了一個未確認的命令，觸發 API 誤用的 assert。是探針（Test 自身）的時序前提，不是產品的缺陷。
     - 第 5、8 次（load 1.54、2.03）：「an interrupted line has the wrong gap or epoch」，`connection interrupted` 行的 `gap_ms` 是 523 與 563（判定 500～520）。中斷行由網路執行緒在 liveness 期限的喚醒時寫出，晚了 23～63 ms。原因沒有切分（環境的排程，或產品的計時器）。
     - 17 次到達 ACK 檢查的：全部 killed，`prior_hello_ms` 100.01～100.12，ACK 在 Drain 後 147.9～149.7 ms（下一次 Hello 前 2 ms 的那一輪）。
  2. FO：10／10 通過。ACK 在 0.13～0.19 ms，action 在 0.12～0.35 ms；兩條時間線的 `prior_hello_ms` 都在 100.01～100.10。
  3. FS：10／10 killed。action 在 148.0～148.6 ms。
  4. **沒有跑**：我寫的命令把 47 個突變 id 放在一個 zsh 變數裡傳給 runner，zsh 不做字詞分割，runner 收到一個參數而以「unknown mutant ids」結束（exit 2，1 秒內）。這是我的命令的錯誤。依宣告第 1 項已不通過，沒有補跑。
- 3 次失敗的位置都在 `CheckActions` 之前（`RunOutage`、60 FPS 發佈迴圈）。突變（`v7-08-drain-no-wake`）拿掉的那一行只在 action 的 ACK 前進時執行，第一次是在 `CheckActions` 裡，所以推論這 3 次和突變無關；但只有 FM 出現（FO、FS 各 10 次 0 次），樣本少，不能排除。
- 今天的其他執行中，`CheckActions` 之前的失敗：量測 1 的 O 第 1 次（「worker emitted a resend catch-up burst」）。三種失敗合計 130 次中 4 次。
- load average（1 分鐘）：1.14～2.85。
- 證據：本機 scratchpad 的 `m/`（git 忽略）：`bin2/`（FO、FM、FS）、`m3/`（每次的完整輸出與 `results.txt`）、`worker47.load`、`worker47.log`；crash report 在 `~/Library/Logs/DiagnosticReports/worker_FM-2026-10-29-202503.ips`。（2026-10-31 補註：這裡的 scratchpad 內容已經不在，未保留）

## 決定（D245）與重新確認：事前宣告（2026-10-29 核准）

使用者：「走 (1)：寫新的確認宣告（D245），把 ACK 檢查之前的失敗另列一類、設上限，重跑第 1 項並補跑第 4 項；那 3 種失敗另記未結事項。」核准：「核准 worker-wake-phase.md 中 D245 的事前宣告，開始跑。」核准前沒有建置或執行任何東西。以下內容核准後不改，跑完不放寬。

- **與 D244 的關係**：D244 照記錄保留為「不通過」，不改寫。D245 是另一次試行。它的判定是在看過 D244 的結果之後才訂的；放寬的面寫在下面。D244 的第 2、3 項（FO 10／10、FS 10／10 killed）已通過，不重跑。
- **修正**：不改，與 D244 相同（工作樹從 D244 之後沒有再改）。第 1 項的執行檔用 `variants.py` 重新建 FM（`v7-08-drain-no-wake`），複製到 scratchpad 的 `m/bin3/` 再跑。另外記錄它與 `bin2/worker_FM` 的 sha256 是否相同；這只是參考，不影響判定。
- **判定的變更（相對於 D244 第 1 項）**：
  1. 守的性質：ACK 檢查能殺死 `v7-08-drain-no-wake`，也就是 Drain 推進 ACK 時，會喚醒網路執行緒立刻送出，不等無關的喚醒。
  2. 放寬的面：D244 中「在 ACK 檢查之前就失敗」的那一次也算不通過。D245 允許這類失敗最多 2 次，不算不通過。
  3. 收緊的面：沒有。
  4. 性質仍由「到達 ACK 檢查的 20 次全部 killed」守住，到達的次數與 D244 的要求相同。ACK 檢查之前的那 3 種失敗不由這次確認守，改記為未結事項（見最後一項）。
  5. 證據：D244 的結果（上一節），以及本節的結果。
- **第 1 項：FM，串行**。每次跑之前記錄 load average（1 分鐘），每次的完整輸出都保存。
  - 到達＝輸出中有 `v7-08 ack timeline:` 那一行（這一行在 40 ms 的判定之前寫出）。
  - killed＝到達，而且結束碼非 0、輸出含「an advanced ACK waited for an unrelated wake」、`prior_hello_ms` 在 99～110。
  - 到達但不是 killed＝不通過。立刻停下，不再跑。
  - ACK 檢查之前的失敗＝結束碼非 0（含逾時與 abort），而且沒有到達。記錄失敗訊息與最後一行輸出；如果是 abort，記錄 crash report。
  - 一直跑到「到達」累計 20 次為止。ACK 檢查之前的失敗累計到第 3 次時就停下，算不通過。所以最多跑 22 次。
  - 結束碼 0 卻沒有到達，不應發生；如果發生就停下，算不通過。
  - **通過＝到達的 20 次全部 killed，而且 ACK 檢查之前的失敗不超過 2 次。**
  - 上限的依據：今天 130 次執行中，ACK 檢查之前的失敗共 4 次（約 3%）。
    - 比率若真的是 3%，22 次中出現 3 次以上的機率約 3%，這是誤停的機率。
    - 比率若和 D244 的 FM 一樣是 15%，停下的機率約 66%；若是 10%，約 38%。
    - 所以這個上限只對「這些失敗和突變無關」做了很弱的檢驗，不能當作證明。
- **第 4 項：worker 檢查的全部突變（`mutations.json` 中 `check` 為 `worker` 的 47 個）**。第 1 項通過才跑；不通過就不跑。
  - 事前檢查已做：47 個突變的 `file` 都是產品檔（`ClientConnection.cpp` 44 個、`Movement.hpp` 2 個、`ConnectionLiveness.hpp` 1 個），與 HEAD 相同，所以不需要 `--allow-dirty`。每個突變的 `find` 在現在的檔案中各出現 1 次（stale 0）。
  - 命令（避免 D244 的錯誤）：
    - 由 Python 腳本從 `mutations.json` 讀出 id 清單，以參數清單（不經 shell）呼叫 `tests/object_fps_pvp/run_mutations.py --build-dir build/target/_build/test --only <47 個 id> --report <scratchpad>/m/m4/report.json`。
    - `--cmake`、`--ctest`、`--go` 用絕對路徑。
    - 呼叫之前，腳本先印出 id 的個數，個數必須是 47 才繼續。
    - 背景執行；跑的期間我不改工作樹。
  - runner 一開始會用沒改過的版本跑一次 worker 檢查（baseline）：
    - baseline 在 ACK 檢查或 v7-08 action 檢查失敗：停下，算不通過。
    - baseline 在其他檢查失敗：只重跑整個 runner 1 次，失敗訊息記錄下來；第二次 baseline 再失敗就停下，算不通過。
  - **通過＝47／47 killed**。其他結果依 D185 分類：
    - wrong_failure：用 `--only <該 id>` 單獨重跑 1 次。killed 就記為環境，失敗訊息記錄下來；不是 killed 就算不通過。
    - survived、stale、建置失敗：不重跑，直接算不通過。
- **整體**：第 1 項與第 4 項都通過，加上 D244 已通過的第 2、3 項，才算「修正有效」。任何一項不通過，就不重跑、不修改，停下來回報。
- **範圍的限界**：
  - D244 的限界照舊：只在 macOS（Intel），D231 的 Windows 限界不變。
  - ACK 檢查之前的失敗本身不量、不修，原因也不切分。
  - 「與突變無關」只受到上面那個很弱的檢驗。
  - 同一台 Mac 上其他會話的活動控制不了，照實記錄。
- **未結事項（完成時寫進 HANDOFF，不論 D245 是否通過）**：worker 探針在 `CheckActions` 之前的 3 種失敗，以及 D245 中新觀測到的這類失敗：
  - `SendInput` 的 `GYO_ASSERT` abort，推論是 60 FPS 發佈迴圈的時序前提。
  - 「an interrupted line has the wrong gap or epoch」，`gap_ms` 523、563。
  - 「worker emitted a resend catch-up burst」。

  每一種附上次數、load average 與證據位置。它們都在 `worker_main.cpp` 的其他段落，之後要改這個檔案的 K10／WSAECONNRESET 會話需要知道。
- **估計時間**：重建約 5 分鐘；第 1 項約 9 分鐘（每次約 22 秒，最多 22 次）；第 4 項約 70 分鐘；判讀與文件約 20 分鐘。

### 結果（2026-10-29，本機 macOS Intel）：修正有效

收支：一致。第 1 項跑了 20 次，20 次都有完整輸出，全部到達 ACK 檢查。第 4 項選了 47 個突變，runner 收到 47 個，結果也是 47 個。

- **判定：「修正有效」**。第 1、4 項通過，加上 D244 已通過的第 2、3 項。
  1. FM：**通過**。跑了 20 次，到達 20 次，killed 20 次；ACK 檢查之前的失敗 0 次。
     - `prior_hello_ms` 在 100.00～100.14。
     - ACK 在 Drain 後 147.90～148.94 ms 才送出，也就是下一次 Hello 前 2 ms 的那一輪；Hello 在 149.80～150.05 ms 到達。
     - 每次約 22.2 秒（第 1 次 22.8 秒）。load average 1.40～3.82。
     - 重新建置的 `bin3/worker_FM` 與 `bin2/worker_FM` 的 sha256 不同（`3e305434…` 與 `6dbdd105…`）。依宣告只記錄；兩者都是在同一個工作樹、同一個突變下建置的。
  4. worker 的 47 個突變：**47／47 killed**。
     - baseline（沒改過的版本）通過。
     - 沒有 wrong_failure、survived、stale 或建置失敗，所以沒有重跑。
     - 22:26:42～22:58:31 JST，約 32 分鐘。開始時 load average 1.89。
- D244 的三種失敗（`SendInput` 的 assert、中斷行的 `gap_ms`、resend catch-up burst）這次都沒有再出現。依宣告只記錄，寫進 HANDOFF 的未結事項，不據此推論它們消失了。
- 證據：本機 scratchpad 的 `m/`（git 忽略；2026-10-31 補註：這裡的 scratchpad 內容已經不在，未保留）：
  - 執行檔：`bin3/worker_FM`
  - 第 1 項：`m5/`（每次的完整輸出與 `results.txt`）、執行腳本 `run_d245.py`
  - 第 4 項：`m4/`（`run.log`、`report.json`、`load.txt`）、執行腳本 `run_d245_item4.py`
