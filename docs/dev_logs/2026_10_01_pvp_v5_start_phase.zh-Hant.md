# PvP v5 第03批：啟動相位對齊（方案A1）

日期：2026-10-01。Owner：object_fps_pvp。狀態：程式與CPU／Go驗證完成；原生GUI可見延遲短測待重跑。

## 授權與起點

只處理[交接](../object_fps_pvp/plans/v5/HANDOFF.md)唯一待結案的可見延遲守門。使用者看過提案後
指示採用方案A（建議的A1：Snapshot新增等待欄位，目標w約4ms）。不開始第04批、不跑長測、
不改門檻、lead、插值或60Hz。起點為`de87bb9`。

## 原因

`SeedLead`在收到首個Snapshot的那一幀重設Client固定步，seq3在同幀產生並立即送出；
Match在收到seq1後的下一個Tick開始，之後每Tick一個序號。兩端皆為60Hz，所以首窗口
「收到→下一Tick」的等待w（0～16.7ms）整個epoch固定加在延遲上。第02批w=9.073ms、
第03批失敗輪14.781ms，本機→權威中位數差約6ms，呈現側沒有變化。

## Architecture Delta

1. 需求：第03批可見延遲守門（P50≤50ms）受啟動相位抽籤影響，使用者批准方案A1。
2. 現有問題：Client與Authority相位無任何關係，固定參數無法保證每次啟動都達標。
3. 變更邊界：產品Runtime／Client兩份wire的`PlayerState`各加一個可選欄位；Host新增量測，
   Client預測新增一次性相位調整。
4. 影響owner：只有object_fps_pvp的Match Host、Gateway adapter、Client與專用測試。
5. 依賴方向：不變。沒有新增Engine、公共Gateway或跨產品依賴；`MovementTrace.hpp`未修改。
6. Ownership：量測屬Host（擁有時鐘），調整屬Client預測；Match仍無時鐘，不讀此欄位。
7. 更小的改動不足：不改wire的A2只能用Snapshot接收時間推估，需假設上下行對稱，
   RTT40或非對稱網路會誤判。

## 實作

- Host：`SubmitInput`在游標為0且新接受seq1時記錄收到時刻（重送不重記）；執行seq1的Tick
  算出等待並夾在0～1,000,000µs，之後該epoch每份Snapshot都帶此值。epoch／life變更、Leave、
  Reset清除。
- Wire：兩份proto `PlayerState`加`optional uint32 epoch_start_wait_us = 16`；IPC、Go adapter、
  ClientConnection保留presence並拒絕超過上限的值；Go bindings以pinned protoc 36.2／
  protoc-gen-go v1.36.11重新產生。
- Client：只有epoch起點播種（`lastResolvedCommand==0`）才等待量測；首窗口發布時記錄首個合法
  步已過時間。收到w後調整量為「w＋已過時間−4ms」，每幀最多移動經過時間25%。
  w>1Tick＋2ms、中途重新播種或首窗口後丟棄時間則不採用。觀測新增`epochStartWaitSeconds`
  與`startPhaseShiftSeconds`供分析。

這是相位調整而非延遲本機顯示：命令仍在產生幀取樣並立即顯示。鎖頻幀率下調整會量化為整幀，
此時命令落在較晚的幀，最差餘裕等同現在「w≈目標值」的啟動，模擬中全部Actual。

## 驗證

- C++ `gyo_object_fps_pvp_tests`：118 cases／1,398,468 assertions通過。
  既有486組停頓恢復矩陣與首幀延後矩陣改為經過模擬Host回報w，維持全部原斷言。
- 新增：Host計時（缺seq1不計、重送不重設、Leave清除）；預測相位調整（調整量、顯示不倒退、
  重複回報不再調整、Host遲到與中途重新播種不採用）；相位矩陣30／60／144FPS×RTT0／20／40×
  三個worker相位×12個Authority相位，全部Actual、零重設、每相位不慢於未對齊。
- 模擬「命令產生→執行」中位數（RTT0、worker 4000）：

| FPS | 未對齊各相位 | 對齊後各相位 |
|---:|---:|---:|
| 30 | 51.4–66.7ms | 22.2–37.5ms |
| 60 | 34.7–50.0ms | 22.2–37.5ms |
| 144 | 38.9–45.8ms | 34.7–36.1ms |

- Go：`go vet`、unit與race通過，含新欄位presence／上限測例。
- 正式Client網路、IPC、Client support及action／network／timing／worker probe建置成功。
- CTest `-L pvp`：11／11通過（含worker真loopback socket、wire與證據分析器）。
  `presentation_cpu`需要staged內容／shader工具，本環境下載shader工具失敗，未建置也未執行。

## 未執行與限制

- 本環境沒有顯示與GPU，未執行原生雙GUI可見延遲短測，也未重跑25案真網路矩陣。
  第03批結案前須重跑GUI短測並記錄每輪w與調整量；不可改門檻或重跑挑分數。
- 只在epoch起點對齊一次。跨機器時鐘漂移或epoch中途重新播種後相位會再次漂移，
  連續微調另議。
