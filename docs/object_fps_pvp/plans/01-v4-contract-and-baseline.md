# 第 01 批：v4 契約與 v3 基線

前置：既有 v3 可運行。執行進度以 [README](README.md) 為準。

## 目標與邊界

鎖定時間、身分、資料語意與射擊規則，讓後續各批不必重新猜測。本批只整理文件、
核對基線及執行一次現有產品 CPU 短回歸；不改 wire、C++ API、遊戲行為或測試實作。

## 工作內容

- 在 [v4 契約](../protocol-v4.zh-Hant.md) 定義 SimulationTick、InputSequence、
  PacketSequence、ActionId、Session／PlayerId 與 movementEpoch 各自的用途。
- 區分 Network Snapshot、Hit-test History、Rollback State；後兩者不在 v4 實作。
- 記錄已批准的 CombatRules、本機回饋／權威裁決、動作窗口與確認、有效期限、
  生命週期及背壓語意。所有 v4 欄位明確標為待實作，不提前建立空 proto。
- 核對目前兩份 v3 schema、C++／Go 版本、產品 owner、現有槍模及測試入口。
- 保留兩步 lead、60 Hz、移動窗口、插值、積欠與耗盡恢復，不重新調參。
- 在產品 protocol README 加入 v4 文件入口，架構文件說明分批依賴及預計 Delta。
- 保存五份計畫，建立進度、交接及本批 dev_log。

## 一次短回歸

從 repository root 執行，先確認既有測試目標是否需要建置：

```bash
cmake --build build/target/_build/test --target gyo_object_fps_pvp_tests --parallel 2
ctest --test-dir build/target/_build/test \
  -R '^object_fps_pvp\.(cpu|presentation_evidence|command_evidence|recovery_relay|backpressure_evidence)$' \
  --output-on-failure --output-junit logs/pvp-v4-batch01-cpu.xml \
  -O logs/pvp-v4-batch01-cpu.log -V
```

這是既有 domain／分析器測試，不啟動完整網路、GUI 或長測。若發現與本批無關的
環境／程式失敗，記錄證據及範圍，不直接展開下一批實作。

## 完成條件

- 契約、Architecture Delta、基線證據與五批依賴可由文件獨立讀懂。
- 短回歸通過，文件連結與格式正確，既有修改保留。
- 更新進度與交接；明確標示線上仍為 v3，完成後停止。
