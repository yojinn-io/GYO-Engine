# 第 05 批：C1——30 FPS 修正前後對比（任務 1、2）與視窗操作

狀態：**未開始**。PR 線 P1b。依賴第 04 批與 TT-1。
起因：2026-10-07 使用者指示保存 v6 的 30 FPS 失敗案例，在 v7 分執行緒完成後再跑一次比較，預期 clean-30 與 clean-60 的結果相近（v7 README「v6 的 30 FPS 失敗案例與各幀率的對照」）。定義見 D31。

## 事前宣告草案（未核准；批次開始時核准；2026-10-09 依第 04 批的結果更新）

對象與建置：
- before：v6 最終量測來源 `fee92ff`。產品程式＝`c7d6dd3`，與 tag `object_fps_pvp-v1.1.0` 的產品程式相同。
  - 與第 03a 批同一份建置：同一路徑、`build.sh` 的 DEPS，從零建置。
  - 產物雜湊與 STABLE_BASELINE 比對；不一致時記錄，確認產品程式的 tree 雜湊相同即可繼續，只有重現不了行為參考範圍時才停下。
- after：P1b 的頭（含已合併的 P1a），只有任務 1、2，仍是 pv6。
  - 網路 worker 的輪詢與送出節拍不變。ClientConnection 多一個給模擬角色的 snapshot 佇列，以及帶世代的 `SendInput`（穩態下行為相同）。Match、Gateway 沒改（第 04 批的權威 digest：對 v6 最終 tree 35／35 相同）。
  - 過期政策是 D30 的修訂版（2026-10-09，最小門檻 150 ms）。
  - Gateway 結果通道的缺陷兩棵 tree 都保留。
  - 只在 CI 綠燈並凍結的 head 上執行；之後任何非文件的變更，都要依宣告重新評估受影響的項目。

跑次：
- clean-30、clean-60 每棵 tree 至少 6 輪，交錯先後，機器閒置。
- clean-144 作為對照組，每棵 tree 至少 3 輪。

主機狀態：
- 主分類用參考包的定義：Match snapshot_produced 對擬合 60 Hz 格點的晚醒 P99，≥6.0 ms 算 8 ms 狀態。P1b 沒有改 Match，所以兩棵 tree 可比，也與 v6 對照表一致。
- 每輪同時記錄 probe TimerBaseline，並報告兩種分類的一致率，作為 C2 之後的分類依據。
- 報告各棵 tree 的狀態頻率，不假設兩樹相同，只在同一狀態內比較；頻率差異列為發現。
- 每輪前後各量一段沒有 Client 的 Match 空轉晚醒，證明 after 的 Client 沒有把主機推向另一個狀態；頻率明顯偏移就停下報告。
- 兩種狀態都設輪數上限（每棵 tree 每案最多 12 輪），上限內沒有樣本就標「未驗證」，不算通過。

各指標的權威來源：
- Held 筆數與時段、Actual 比率、停頓重設次數：由兩棵 tree 共用的同一個抽取器計算。
- 通過與失敗：以凍結分析器判定。C1 用的 Python 分析器子集（v6 最終清單 `0d2ffcfb…` 中的 Python 檔：07a 的規則加 `PROTOCOL_VERSION 6`，另加 `action_probe.py` 的一處註解）另立 sha256 清單並凍結；宣告中記錄與 07a 的差異。
- 分執行緒後語意改變的規則（STALL_RULE、seed 夾住豁免、每幀的 runtime_gap、60 Hz ±2 步）：只以第 04 批的分析器 v7 判定，清單事先列出；凍結版的結果照列、不作判定依據。
  - 分析器 v7＝`build/acceptance/object_fps_pvp/command_evidence_v7.py`（ID `pvp-v7-commands-1`），它匯入的凍結 `command_evidence.py` 一起列入 sha256 清單。每輪結束後以 CLI 各跑一次，結果寫在該輪目錄的 `command-evidence-v7.json`。
  - before 的 trace 也跑 v7，結果只列、不判定：v6 的 seed 夾住來自畫面幀（30 FPS 時約 33 ms），照 v7 的規則會算成模擬的 gap。

指標與比較：
- 指標：各狀態的通過率、Held 筆數與發生時段、停頓重設次數、Actual 比率；移動延遲 P50／P95 只記錄。模擬步的晚醒依狀態記錄。
  - 延遲的預期（第 04 批的發現）：常數不變時，v7 的「產生→執行」固定約 37 ms（RTT 0），「輸入取樣→執行」比 v6 多約 1 幀。這是已知的差異，C1 只記錄，C1 之後與相位追蹤一起決定。
  - 參考（不計次的開發跑次，第 04 批的頭）：clean-30、clean-60 各 1 輪通過，Actual 1677／1680，P50 約 37 ms，P95 約 38 ms。
- 兩種比較：同一幀率的 v6 對 v7；同一棵 tree、同一狀態下各幀率之間的差距。
- 判定草案（批次開始時由使用者核准或修改）：
  - 各狀態下，v7 clean-30 的通過數 ≥ v7 clean-60 的通過數減 1。
  - v7 clean-30 的 Held 比率 ≤ v7 clean-60 的 2 倍，而且 ≤ v6 clean-30 同狀態的 1/3。
  - v7 clean-60 不比 v6 差（在宣告的容許範圍內）。
- 第 03a 批的數字一起列出，分開顯示 03 與 04 的貢獻。

使用者操作的 L2：
- 重現 IP-2 的標題列拖動與角落縮放。判定：拖動期間 Match 的替代連續 ≤3 Tick，停頓重設 0。

L3（使用者）：
- 連續射擊中切換 Spaces、縮小、遮住視窗。保留 Client、Gateway、Match 的日誌與 movement trace。判定不斷線。
- 30 FPS 的手感。

## 驗收點

- L2：照核准的宣告執行；失敗的跑次保留，先有限定位，不默默重跑。
- L3：使用者的清單通過；重現 Spaces 斷線算失敗，並保留日誌。

## 建議檔位

medium（執行與記錄）；宣告草案以 high 撰寫。依 D39，宣告草案建議由 1 位評審加 1 次對抗式檢查，批次開始時徵求同意。

## 停止條件

- v7 clean-60 的退步超出宣告。
- before 無法重現 v6 參考範圍（表示環境改變）。
- 主機狀態頻率在兩樹間明顯偏移。
- Spaces 斷線重現，或視窗被遮住時模擬晚醒 ≥100 ms：保留日誌後停下，提出 macOS 程序活動宣告（App Nap 對策）作為新的 Architecture Delta，交給使用者決定。
- 凍結分析器拒絕 after 的 trace。

## 平台表

| 平台 | 本批 |
|---|---|
| macOS Intel（Metal、120 Hz） | L2、L3 |
| Windows（D3D12） | 第 15 批的 LAN 場次 |
| Linux、macOS arm64 | 未驗證 |
