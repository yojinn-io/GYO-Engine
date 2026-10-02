# PvP v4 完整驗收與穩定基線

日期：2026-09-28。Owner：`object_fps_pvp`。
狀態：完整驗收通過，v4已升格Linux／X11／Vulkan同機雙玩家穩定基線。

使用者在第05批交付後追加授權：由Agent接手原手動完整驗收，發現問題須修復，
全部通過後將v4升格穩定基線。`MANUAL_ACCEPTANCE.md`與`ACCEPTANCE_STATUS.md`
保留為未來測試案例參考。新授權取代原來「不代跑長測」的本次執行分工，門檻不變。

## 執行與證據保護

開始前核對第05批最終manifest，224個來源／文件／內容及7個產物指紋完全相符。
現有未提交變更保留；不覆寫v3或第05批原始成功／失敗報告。
量測依序執行，不同時建置／運行其他性能測試。正式產品維持60Hz／兩步lead、
當前權威命中、100HP／25傷害／20Tick冷卻與既定恢復政策。

環境記錄：`build/target/_build/test/logs/pvp-v4-release-environment.json`。
驗收範圍是Linux／X11／Vulkan、同機兩玩家與受控網路；不擴大為實體LAN、Windows、
跨主機時鐘漂移或歷史命中補償認證。

## 驗收器缺口修正

前置唯讀審查發現三個判斷缺口，修正限於產品acceptance：

1. 長測原只要求有射擊。需驗證兩玩家依probe既定400ms排程持續射擊到量測結尾，
   不能只在開頭射四槍再停止，卻宣稱30分鐘射擊共存通過。
2. 每份戰鬥快照必須恰好包含兩個不重複的有效PlayerId，Tick單調，HP與終局狀態
   符合唯一裁決累計傷害；不能以早期少量HP觀測掩蓋後續遺漏。
3. 完整GUI跑次必須零受干擾標記／epoch重設，不可只因延遲分位數仍合格而取得
   乾淨跑次資格。保留原始結果，另用修正後判斷核對。

沒有放寬產品或延遲門檻，沒有將probe排程規則改成正式武器政策。

## 三輪GUI共存

命令使用現有`run_timing.py --gui --combat --rounds 3 --duration 120 --events 200 --fps 60`，
每輪獨立服務與產物manifest，全程包含真SDL射擊、HP／HUD與下一成功Presented觀測。

| 輪 | 移動配對 | P50／P95 | SDL射擊／唯一裁決 | 實際正傷害 | Actual |
|---:|---:|---:|---:|---:|---:|
| 1 | 200／200 | 37.82／38.76ms | 150／150 | 100HP | 100% |
| 2 | 200／200 | 39.81／40.76ms | 150／150 | 100HP | 100% |
| 3 | 200／200 | 38.53／39.55ms | 150／150 | 100HP | 100% |

原始三輪皆通過、未記錄干擾或重設，所有慢幀與未知事件分母保留。
入口：`build/target/_build/test/logs/pvp-v4-release-gui-1/results.json`。
HP=0後後續命中傷害0；不以飽和HP單獨證明去重，仍結合ID、帳本與domain／wire證據。

## 真實桌面操作與圖像

新增 acceptance-only `--native-window` 被動觀測模式及 `run_native_window.py`：
使用正式 PvpApplication、真實 X11／XTest 鍵鼠與 GNOME 視窗管理員，初次加入後
不注入 SDL 事件、不呼叫 SubmitShot 或直接修改世界。20Hz 唯讀狀態與有界逐幀
Presented 資料供核對；圖像經 XGetImage 取得，包含實際 HUD。
此功能驗證不作效能量測，也不宣稱有人親手試玩或量到顯示器 scanout。

最終 `pvp-v4-native-window-6/native-window-result.json` 的 V1–V8 全部通過：

- 直／斜移動、停止、原生滑鼠轉向、牆面／牆角，以及同時移動、瞄準、射擊。
- 捕捉點擊不射擊、單擊一次、按住不連發；四次命中 HP 為75、50、25、0。
- 隔牆玩家不受傷、射偏不受傷／無命中提示；後座沒有改變瞄準角。
- 零血仍能移動與射擊，Esc 後重入由 Player2 變為 Player3、恢復100HP。
- Tab／失焦、真實標題列拖曳（位置66,137→111,172）與原生 WM 縮放
  （800×600→875×642）均釋放捕捉且沒有幽靈射擊。
- 雙方 Esc 後 Lobby 為0／2，原生 Alt+F4 正常結束，待確認窗口保持≤12。

已目視最終四張圖片：`peer-hp-zero-hud.png`、`creator-wall-hud.png`、
`resized-actual-hud.png`、`empty-lobby-ui.png`。槍模、HUD、縮放與空房人數符合狀態。
所有路徑位於 `build/target/_build/test/logs/pvp-v4-native-window-6/`。

先前原生跑次保留：1／2的縮放手勢未命中GNOME可用區域；3的EWMH焦點請求被拒；
4為實際拖曳／縮放專測通過；5的射擊前提錯誤，兩人都在牆東側，權威命中正確。
修正為PID／title驗證後聚焦自有視窗、GNOME原生Super＋中鍵縮放，以及射擊前
驗證射線確實經過牆的站位。這些是測試工具／fixture修正，不是正式產品缺陷。
歷史整機拖窗停頓的根因仍未證實；本次只能證明目前環境的測例通過。

逐幀補核對：`native-frame-audit.json` 證明9次原生射擊均在首次成功Presented
出現動畫，命中標記只對應有效玩家命中。刻意干擾的功能跑次包含一次120.38ms
迴圈間隔；不從此跑次推定乾淨FPS或長測資格。

## 60Hz 實時30分鐘

`pvp-v4-release-soak60-1/result.json`完整通過。實際60.0006Hz，108001量測幀，
最大幀間隔18.09ms；8802提交／交付／接受，兩玩家各100HP唯一傷害，432964份HP觀測。
移動215998個量測命令全為Actual；首次送出P95 16.00ms，Actual P50/P95
46.94/47.02ms。射擊Match裁決保守上界P95 13.73ms，Client取得P95 100.00ms。
移動窗口最高4，動作保留最高1，30Tick積欠總和最高60、尾端52，零重設、零掉時、
零診斷遺失、零100ms排程間隔或持續凍結；Gateway零限流拒絕，單Session每秒最高78包。
全部原始間隔保留。HP歸零後不做測試專用治療，後續去重依唯一ID／不可變裁決及既有
帳本測試核對，不把零傷害HP飽和當成全部證據。

## 144Hz 實時30分鐘與升格

`pvp-v4-release-soak144-1/result.json`完整通過。實際144.0006Hz，259201量測幀，
最大幀間隔9.45ms；8938提交／交付／接受，兩玩家各100HP唯一傷害，1039108份HP觀測。
216000個量測移動命令全為Actual；首次送出P95 15.99ms，Actual P50/P95
42.50/45.32ms；射擊Match裁決保守上界P95 17.61ms，Client取得P95 97.26ms。
移動窗口最高4、動作保留最高1、30Tick積欠總和最高60、尾端31／33；零重設、
掉時、診斷遺失、100ms間隔、持續凍結或Gateway限流拒絕，每Session最高78包／秒。
串行兩輪退出碼均0，沒有失敗長測或重跑：`pvp-v4-release-soak-suite-1/results.json`。

正式Client／Match／Gateway、action probe、Arena/catalog與第05批指紋一致；
GUI probe只新增本次native觀測模式。三輪GUI仍引用原probe SHA，原始manifest不改。
C++／Go race／wire／shader／GPU／故障／fitness重用範圍由未變正式產品支持，
驗收器新增負面回歸19／19通過。所有本次自有測試服務均已退出。

本次滿足完整驗收後正式升格v4，詳見
[穩定基線](../object_fps_pvp/plans/STABLE_BASELINE.md)。完整來源／產物／證據指紋
新增於`build/target/_build/test/logs/pvp-v4-release-manifest.json`；v3 manifest保留。
手動指南及回報表保留；沒有自動提交或開始歷史命中下一階段。

Architecture：本輪只擴充產品專用acceptance的被動觀測／原生操作及嚴格分析，
正式產品、Engine、公共Gateway與資料契約沒有改動；沒有新公共依賴或ownership移轉。
原有第04批Client模型依賴／呈現API Delta仍以其dev_log為準。
