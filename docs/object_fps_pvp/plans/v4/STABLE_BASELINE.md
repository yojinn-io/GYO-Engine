# PvP v4 穩定基線

升格日期：2026-09-28。Owner：`object_fps_pvp`。
依使用者追加授權完成第05批完整驗收後，**v4升格為穩定基線**。
範圍為Linux／X11／GNOME／Vulkan、同機雙玩家，以及既有受控網路／阻塞恢復測例。
這是已驗證範圍的開發基線，不代表實體LAN、Windows、跨主機時鐘、GUI長時間運行
或射擊回溯均已驗證。

## 固定契約與版本

Client／Gateway／Match共同使用v4，明確拒絕v1–v3。保持60Hz模擬／傳送／快照、
兩步移動lead、既定epoch恢復、1Tick遠端插值。Mark23單發、無限彈藥、100HP、
25傷害、20Tick冷卻；Match查詢當前權威幾何，獨立ActionId去重與確認。
HP=0仍可操作，離開再加入取得新身分與滿血，沒有新增死亡／重生或歷史命中。
詳細政策以 [v4契約](../../protocol-v4.zh-Hant.md) 為準。

起始HEAD：`bfeb047669d465ce2a9a43af43c85fb3e577e4bc`。工作樹尚未提交；
不能只用HEAD識別本基線。來源、內容、分析器、產物、證據及git status記於：
`build/target/_build/test/logs/pvp-v4-release-manifest.json`。
v3及第05批原始manifest不覆寫；`project.json`／`checks.json`格式版本不變。

| 正式產物 | SHA-256 |
|---|---|
| Client | `8cb197fdc5fd686a2ecb848b91ef3d6c1797ffd9b814fba89709f0a754fc8f29` |
| Match | `29f64b3cf1957cffbc6cda6ee3c324fa5eb0a0a741c148161f50554b8ee70ac5` |
| Gateway | `ff4e4c7d162caa59cb10c72c935e1566ec874ce2225396dd69c60859bd5db554` |

## 本次完整結果

三輪GUI各自120秒、200個獨立移動事件、150次真SDL射擊；每輪200/200配對、
150個唯一接受裁決、四次25HP傷害、全部射擊在下一成功Presented開始回饋。
Actual均100%，零受干擾標記／epoch重設。保留全部幀間隔，不剔除慢幀。

| GUI輪 | 可見交越P50 | P95 | 門檻 |
|---:|---:|---:|---|
| 1 | 37.82ms | 38.76ms | 50／80ms |
| 2 | 39.81ms | 40.76ms | 50／80ms |
| 3 | 38.53ms | 39.55ms | 50／80ms |

| 1800秒headless | 60Hz | 144Hz |
|---|---:|---:|
| 實際迴圈Hz／量測幀 | 60.0006／108001 | 144.0006／259201 |
| 移動命令產生／Actual | 215998／215998 | 216000／216000 |
| 首次送出P95 | 16.00ms | 15.99ms |
| Actual P50／P95 | 46.94／47.02ms | 42.50／45.32ms |
| 射擊提交／交付／接受 | 8802／8802／8802 | 8938／8938／8938 |
| Match裁決P95保守上界 | 13.73ms | 17.61ms |
| Client取得裁決P95 | 100.00ms | 97.26ms |
| HP觀測／每位玩家唯一傷害 | 432964／100HP | 1039108／100HP |
| 移動窗口／動作保留最高 | 4／1 | 4／1 |
| 30Tick積欠總和最高 | 60 | 60 |
| 最大幀間隔 | 18.09ms | 9.45ms |

兩組各自零重設、零診斷遺失、零模擬掉時、零≥100ms排程間隔、零持續凍結，
Gateway零限流拒絕，每Session每秒最高78包。兩玩家持續依≥400ms合法節奏射擊，
不是只有開頭四槍；全部17740個動作都有唯一裁決。HP歸零後不注入治療，後續零傷害
不能只靠HP飽和證明去重，仍結合ActionId／不可變裁決與domain／網路帳本回歸。
全部延遲只比較同機單調時鐘，不稱input-to-photon。

原生X11／XTest V1–V8全部通過：移動／瞄準／同時射擊、捕捉單擊／按住、
100→75→50→25→0、隔牆／射偏、零血操作、新身分滿血、Tab／失焦、實際標題列
拖曳與原生WM縮放、雙Esc後Lobby0人、Alt+F4。9槍均於首次成功Presented回饋；
實際HUD與槍模圖像已核對。這是Agent執行的原生功能驗收，非使用者手感回報；
刻意干擾跑次含120.38ms幀間隔，不當作乾淨性能證據。

## 證據索引

下列相對於`build/target/_build/test/logs/`；原始trace及失敗跑次為ignored本機
產物，需與manifest一起保留／打包。提交Markdown不會自動保存這些證據。

| 路徑 | 意義 |
|---|---|
| `pvp-v4-release-gui-1/results.json`及三個round目錄 | 三輪GUI完整呈現、射擊、HP、各輪原始manifest |
| `pvp-v4-release-evidence-audit-1/qualification.json` | 新增零干擾／零重設gate後補核對，原始報告不改 |
| `pvp-v4-release-soak60-1/result.json` | 60Hz完整30分鐘、原始資料與產物指紋 |
| `pvp-v4-release-soak144-1/result.json` | 144Hz完整30分鐘、原始資料與產物指紋 |
| `pvp-v4-release-soak-suite-1/results.json` | 串行執行、兩輪退出碼0與完整資格 |
| `pvp-v4-native-window-6/native-window-result.json`、`native-frame-audit.json` | 原生V1–V8、9槍逐幀與HUD；四張PNG |
| `pvp-v4-release-provenance-check.json` | 正式產品與第05批產物一致；GUI probe新增native模式 |
| `pvp-v4-release-environment.json` | OS／CPU／GPU及工具版本 |

既有C++97 cases、Go race、wire／shader／GPU、30／60／144FPS短測、16案射擊
故障矩陣及新增RTT／burst2、真實產品移除驗證，依不變的正式來源／產物重用。
本輪驗收器回歸19/19通過，native GUI probe建置成功。GUI三輪使用新增native模式
前的probe；manifest保留各自正確SHA，不把新的probe SHA冒充舊跑次產物。

## 修復、限制與後續

本輪修復限產品acceptance：驗證全程射擊覆蓋、完整HP快照／終局狀態、GUI乾淨
跑次gate，以及原生測例的焦點、GNOME縮放手勢、遮牆站位前提。沒有正式玩法修正、
Engine／公共Gateway責任移動或新通用框架。工具仍由object_fps_pvp擁有。

保留第05批P50 52.02ms失敗與兩次pointer-release診斷，不能用新通過跑次抹除；
原生1／2／3／5的測例失敗亦保留。歷史整機拖窗停頓根因仍未證實。
詳見 [完整驗收dev_log](../../../dev_logs/2026_09_28_pvp_v4_full_acceptance.zh-Hant.md)。

[手動命令、日誌與門檻](MANUAL_ACCEPTANCE.md) 及
[驗收狀態／回報模板](ACCEPTANCE_STATUS.md) 永久保留供後續重驗。
本次完成後停止；不自動開啟試玩服務、提交commit或執行下一階段功能。
