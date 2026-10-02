# PvP v4 驗收狀態與回報表

更新：2026-09-28。**完整驗收已通過；v4升格為穩定基線。**
使用者本次明確授權完整測試、必要修復與通過後升格；以下原手動回報模板保留作未來測例參考。
第 05 批工具交付後已追加完整驗收。以下以實際新跑次判斷；既有故障矩陣與
CPU／GPU 證據僅在正式產品來源及產物未變的範圍重用，不以歷史 v3 長測抵銷缺項。

操作入口：[手動驗收指南](MANUAL_ACCEPTANCE.md)。契約：[Protocol v4](../protocol-v4.zh-Hant.md)。

## 本次完整驗收

環境：Linux 7.0.0-34／X11／GNOME／Vulkan，同機雙玩家。原生操作由 Agent 透過
XTest 執行並核對 HUD 圖像，並非使用者人工回報。長測是 headless 網路／預測，
不冒充 GUI 長測；未測實體 LAN、Windows、跨主機時鐘或射擊回溯。

| 項目 | 結果 | 指標與證據（logs 相對路徑） |
|---|---|---|
| GUI round1 | 通過 | 120秒，200/200，P50/P95 37.82/38.76ms，150槍；`pvp-v4-release-gui-1/round-1/round.json` |
| GUI round2 | 通過 | 120秒，200/200，39.81/40.76ms，150槍；同入口 round-2 |
| GUI round3 | 通過 | 120秒，200/200，38.53/39.55ms，150槍；同入口 round-3 |
| GUI 乾淨跑次補核對 | 通過 | 三輪零受干擾／重設、450個唯一裁決與下一Presented回饋；`pvp-v4-release-evidence-audit-1/qualification.json` |
| 原生 V1–V8＋實際 HUD | 通過 | 9槍下一Presented、4×25HP、遮牆／射偏、零血、重入、真拖曳／縮放、Lobby0人；`pvp-v4-native-window-6/native-window-result.json`及`native-frame-audit.json` |
| Headless 60Hz 1800秒 | 通過 | 8802/8802接受，兩玩家各扣100HP，Actual100%，零重設／掉時／遺失／凍結；`pvp-v4-release-soak60-1/result.json` |
| Headless 144Hz 1800秒 | 通過 | 8938/8938接受，兩玩家各扣100HP，Actual100%，零重設／掉時／遺失／凍結；`pvp-v4-release-soak144-1/result.json` |

原生測試前五次中的失敗保留：GNOME縮放命中區、焦點、遮牆站位均為測例前提問題；
run4為視窗專測通過。修正只在acceptance。最終原生跑次刻意干擾且有120.38ms幀間隔，
不當作乾淨效能證據。第05批舊的延遲超標報告也保留，見下表與完整dev_log。

## 第 05 批原始整合證據（歷史）

第 05 批整合與工具交付已完成；以下包含未通過跑次及限制。所有檔案位於
`build/target/_build/test/logs/`；這些是本機 ignored 證據，請保留／打包，不只保留本頁。

| 項目 | 狀態 | 證據／限制 |
|---|---|---|
| C++／Go race／wire／shader／GPU | 重用已通過證據 | 03／04 正式產品未變；04 manifest 137 項中 131 相符，6 項差異均 acceptance／GUI probe，無正式產品差異；新 probe 建置與分析器另驗 |
| GUI 120 秒／200 事件，第一輪 | **未通過** | `pvp-v4-batch05-gui-120-1/round-1/round.json`；200/200，P50 52.02 ms >50，P95 52.73 ms；與 nice19 單工作建置重疊，不推定因果、不抹除失敗 |
| 純移動排除並行建置的有限補驗 | 指標通過，僅診斷 | `pvp-v4-batch05-gui-120-2`；200/200，P50/P95 40.81/41.49 ms；有一次 pointer-release 記錄，不稱完整無干擾驗收 |
| GUI 移動＋射擊＋HP，16 秒 | 通過，僅短回歸 | `pvp-v4-batch05-combat-gui-short-1`；20/20 移動、20/20 SDL 射擊／裁決，100 HP 真扣減 |
| GUI 移動＋射擊＋HP，120 秒 | **單輪整合通過** | `pvp-v4-batch05-combat-gui-120-1`；200/200、P50/P95 42.29/43.48 ms；150/150 SDL 射擊／唯一裁決、四次25HP、下一 Presented 回饋、兩端 HP 核對；不是使用者三輪完整驗收 |
| 30／60／144 Hz 移動＋合法射擊短測 | 通過 | `pvp-v4-batch05-legal{30,60,144}-1/result.json`；每案 10 秒量測＋暖機／排空，合計 148/148 接受，三案 Actual 100%，各玩家各案實際扣 100 HP |
| 射擊故障短矩陣 | 通過 | `pvp-v4-batch05-action-summary.json`；新增 4 案、新裁決恢復 ≤414.44 ms；RTT20 一次 Cooldown 明確拒絕且零傷害，無未知結果；03 ACK／阻塞證據重用 |
| 產品移除／Client-only 模型依賴 | 通過 | `build/target/fitness/pvp-removal-v4-batch05-20260928/logs/fitness-result.json`；實際移除 224 檔與 registry，Engine／獨立 fixture 可建置運行 |

## 使用者完整驗收回報

請複製本節到新的 Markdown 檔填寫，或回傳相同欄位。`未測`／`受干擾`／`缺資料`
都不是通過。測試開始後不要編譯或更新本次二進位。

```text
跑次／日期：
工作樹 HEAD（若未提交，附 status 及來源／產物 SHA）：
OS / Kernel / CPU / GPU / GPU driver：
X11 或 Wayland / 顯示器刷新率：
同機或實體 LAN（跨主機不得直接比較 steady_clock）：
Match / Gateway / Client / probe SHA（附 artifacts 或 run-manifest）：
完整輸出目錄：
是否有更新版本、休眠、切換視窗、大型工作或已知干擾：

人工 V1 移動／視角／碰撞：未測
人工 V2 同時移動＋瞄準＋射擊：未測
人工 V3 捕捉／單擊／按住：未測
人工 V4 傷害／命中標記：未測
人工 V5 牆／未命中：未測
人工 V6 零血／重入／舊狀態：未測
人工 V7 Tab／失焦／實際標題列拖曳／縮放：未測
人工 V8 Lobby 人數／離開／斷線：未測

GUI round 1：未測；退出碼／配對數／P50／P95／實際 FPS／射擊與 HP：
GUI round 2：未測；退出碼／配對數／P50／P95／實際 FPS／射擊與 HP：
GUI round 3：未測；退出碼／配對數／P50／P95／實際 FPS／射擊與 HP：
Headless 60 Hz 1800秒：未測；退出碼／clean／重設／掉時／診斷遺失：
Headless 144 Hz 1800秒：未測；退出碼／clean／重設／掉時／診斷遺失：
兩組動作產生／裁決／接受／拒絕／重複／未知結果：
兩組移動產消／Actual比例／窗口／積欠趨勢／排程間隔：

附檔：各 runner 的 JSON 摘要、summary.md、原始輸出路徑及 manifest。
人工日誌：四個 .log、三個 *commands.jsonl、.exit、artifacts.sha256。
異常：發生時間、操作順序、錯誤、是否恢復、恢復花費時間。
完整驗收自評：未完成 / 有失敗 / 全部通過待核對
```

## 升格條件與交接規則（本次全部滿足）

本次依使用者追加授權，由Agent執行與核對；原生X11操作及圖像核對取代原先
等待使用者試玩的分工，不宣稱人工手感評價。正式產品指紋未變，三輪及兩組長測
均各自通過。基線與後續重驗範圍見 [STABLE_BASELINE](STABLE_BASELINE.md)。

- 三輪 GUI **各自**符合 [指南門檻](MANUAL_ACCEPTANCE.md)，不能平均或省略失敗。
- 60／144 Hz 各自有完整 30 分鐘乾淨的移動＋合法射擊證據；零非預期重設／診斷遺失／
  模擬掉時／持續凍結，全部裁決及 HP 效果可核對。headless 不冒充 GUI 長測。
- 原生操作與目視核對逐項完成，受影響的既有故障／生命週期基線仍有效；已知失敗先有結論。
- Agent 讀取**實際回報與原始摘要**，確認版本、完整性、固定門檻和缺項後，才另行
  更新穩定基線。`probe passed`、`long_run_certification` 或工具正常退出的單一欄位
  都不代表 v4 自動升格。
- 任何產品修正使產物改變時，先列影響與所需補驗；不回填舊版證據給新版本。
- 本次驗收已結束，全部私有測試服務已清理；不自動提交commit或開始下一階段。
