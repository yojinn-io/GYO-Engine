# 第01批：v5契約與v4基線

狀態：已完成，2026-09-28。先讀 [進度](README.md)、[交接](HANDOFF.md)。

## 目標與範圍

保存已批准的五批計畫，固定人物／跳躍／彈匣／死亡／重生的資料與時間語意，
讓下一批可獨立接續。**本批不實作v5 wire、C++型別、玩法或新動畫。**

## 交付

- [v5契約](../../protocol-v5.zh-Hant.md)：唯一預設、Tick順序、生命世代、
  動作去重／ACK與重生、三維預測、呈現區段、拒絕語意與驗收分母。
- [基線](BASELINE.md)：提交識別、現有來源／產物指紋、歷史證據入口、素材、
  現有程式入口及待替換的v4斷言。
- 五份批次文件、README、HANDOFF及dev_log；原v4手動指南、回報與基線保留。
- 現行產品文件增加「規劃中的v5」入口，明確標示目前仍是v4。
- 核對ownership與預計Architecture Delta。人物專用資料及Presenter留Client，
  Match不載入骨骼，公共Gateway／Engine不新增FPS政策。

## 一次有界短回歸

從repository root執行；重新使用時請換成新證據目錄，不覆寫本批結果：

```bash
cmake --build build/target/_build/test --target gyo_object_fps_pvp_tests --parallel 2
ctest --test-dir build/target/_build/test \
  -R '^object_fps_pvp\.(cpu|action_evidence|presentation_evidence|command_evidence|recovery_relay|backpressure_evidence)$' \
  --output-on-failure --output-junit logs/pvp-v5-batch01-20260928/cpu.xml \
  -O logs/pvp-v5-batch01-20260928/cpu.log -V
cp logs/pvp-v5-batch01-20260928/cpu.log \
  build/target/_build/test/logs/pvp-v5-batch01-20260928/cpu.log
```

CTest 的 `-O` 日誌相對呼叫目錄，JUnit 相對 `--test-dir`；本批將原始文字日誌
複製到同一證據目錄，保留原檔，沒有為整理路徑重跑測試。

結果：目標無需重建，6／6通過、9.88秒；C++97 cases與1,390,416 assertions
全通過。這是現有domain及分析器回歸，不是新玩法／GPU／GUI／長時間認證。

## 完成條件與停止

來源／產物與現行v4基線一致；契約與跨批依賴明確，文件連結、素材路徑及變更
範圍已核對。更新進度、交接後停止。第02批人物實作未開始，不能把本批文件
宣稱為已能跳躍、換彈或死亡的試玩版本。
